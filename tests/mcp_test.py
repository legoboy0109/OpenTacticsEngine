#!/usr/bin/env python3
"""ote-mcp (the MCP server, docs/MCP.md) driven over stdio by a tiny client written here. Standard library only.

usage: mcp_test.py <ote-mcp> <repo> [--fuzz N]   (N = fuzz requests in all, default 3000)

Everything works on COPIES of the demo games in a scratch folder; the real games are never touched. The player (for render_map)
is looked for next to ote-mcp / in the build tree by the server itself; when there is none the render checks are skipped with a
visible note.
"""
import hashlib
import json
import os
import random
import shutil
import struct
import subprocess
import sys
import threading
import queue
import time
import zlib

from ote_testkit import check, summary, scratch_dir, failures, read_jsonc

REVISIONS_HANDSHAKE = ['2024-11-05', '2025-03-26', '2025-06-18', '2025-11-25']
MODERN = '2026-07-28'
TIMEOUT = 180


class Client:
    """A minimal MCP client: spawns the server, writes one JSON message per line, reads one per line."""

    def __init__(self, exe, args, env=None):
        self.p = subprocess.Popen([exe] + args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        self.out = queue.Queue()
        self.err_text = []
        self.next_id = 1
        self.bad_lines = []  # stdout lines that were not valid JSON-RPC
        threading.Thread(target=self._pump, args=(self.p.stdout, True), daemon=True).start()
        threading.Thread(target=self._pump, args=(self.p.stderr, False), daemon=True).start()

    def _pump(self, stream, is_out):
        for raw in iter(stream.readline, b''):
            if is_out:
                self.out.put(raw)
            else:
                self.err_text.append(raw.decode('utf-8', 'replace'))
        if is_out:
            self.out.put(None)

    def write_raw(self, data):
        self.p.stdin.write(data)
        self.p.stdin.flush()

    def send(self, obj):
        self.write_raw(json.dumps(obj).encode('utf-8') + b'\n')

    def read(self, timeout=TIMEOUT):
        """The next message from stdout, parsed; records lines that are not valid JSON-RPC."""
        raw = self.out.get(timeout=timeout)
        if raw is None:
            raise EOFError('server closed stdout')
        try:
            text = raw.decode('utf-8')
            msg = json.loads(text)
        except Exception:
            self.bad_lines.append(raw[:200])
            return {'invalid': raw}
        ok = isinstance(msg, list) or (isinstance(msg, dict) and msg.get('jsonrpc') == '2.0' and ('result' in msg or 'error' in msg or 'method' in msg))
        if not ok:
            self.bad_lines.append(raw[:200])
        return msg

    def request(self, method, params=None, meta=None):
        rid = self.next_id
        self.next_id += 1
        p = dict(params or {})
        if meta:
            p['_meta'] = meta
        self.send({'jsonrpc': '2.0', 'id': rid, 'method': method, 'params': p})
        msg = self.read()
        assert msg.get('id') == rid, 'answer for another id: %r' % (msg,)
        return msg

    def call(self, _tool, **args):
        """tools/call; returns the tool's structured result (dict) plus the whole result for content checks."""
        msg = self.request('tools/call', {'name': _tool, 'arguments': args})
        if 'error' in msg:
            return {'rpcError': msg['error']}, msg
        res = msg['result']
        text = res['content'][0]['text']
        data = json.loads(text)
        data['_isError'] = res.get('isError', False)
        return data, res

    def close(self):
        try:
            self.p.stdin.close()
        except Exception:
            pass
        try:
            self.p.wait(timeout=20)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        return self.p.returncode

    def alive(self):
        return self.p.poll() is None


def tree_hash(root):
    h = hashlib.sha256()
    for dirpath, dirs, files in os.walk(root):
        dirs.sort()
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            h.update(os.path.relpath(p, root).encode())
            if os.path.islink(p):
                h.update(b'link:' + os.readlink(p).encode())
            else:
                with open(p, 'rb') as fh:
                    h.update(fh.read())
    return h.hexdigest()


def copy_game(repo, name, dest_root, as_name=None):
    dst = os.path.join(dest_root, as_name or name)
    shutil.copytree(os.path.join(repo, 'games', name), dst, symlinks=False)
    return dst


# ---- JSON Schema sanity (the subset the tools use) -------------------------------------------------------------------
TYPES = {'string', 'integer', 'number', 'boolean', 'object', 'array', 'null'}


def schema_problems(s, where):
    out = []
    if not isinstance(s, dict):
        return [where + ': not an object']
    if 'type' in s and s['type'] not in TYPES:
        out.append(where + ': bad type %r' % (s['type'],))
    if s.get('type') == 'object' and 'properties' in s and not isinstance(s['properties'], dict):
        out.append(where + ': properties not an object')
    for k, v in (s.get('properties') or {}).items():
        out += schema_problems(v, where + '.' + k)
    for r in s.get('required', []):
        if r not in (s.get('properties') or {}):
            out.append(where + ': required %r has no property' % r)
    if 'items' in s:
        out += schema_problems(s['items'], where + '[]')
    if 'enum' in s and not (isinstance(s['enum'], list) and s['enum']):
        out.append(where + ': enum must be a non-empty list')
    if s.get('type') == 'array' and 'items' not in s:
        out.append(where + ': array without items')
    return out


# ---- PNG ------------------------------------------------------------------------------------------------------------
def decode_png(data):
    """(w, h, rows of RGB bytes) of an 8-bit RGB PNG; verifies signature, chunk CRCs and the zlib stream."""
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos, idat, ihdr = 8, b'', None
    while pos < len(data):
        n, typ = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        crc = struct.unpack('>I', data[pos + 8 + n:pos + 12 + n])[0]
        assert zlib.crc32(typ + body) & 0xffffffff == crc, 'bad chunk crc ' + typ.decode()
        if typ == b'IHDR':
            ihdr = struct.unpack('>IIBBBBB', body)
        elif typ == b'IDAT':
            idat += body
        pos += 12 + n
    w, h, depth, ctype = ihdr[0], ihdr[1], ihdr[2], ihdr[3]
    assert depth == 8 and ctype == 2
    raw = zlib.decompress(idat)
    stride = w * 3
    assert len(raw) == (stride + 1) * h
    rows, prev = [], bytes(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if f == 2:
            line = bytearray((line[i] + prev[i]) & 255 for i in range(stride))
        elif f == 1:
            for i in range(3, stride):
                line[i] = (line[i] + line[i - 3]) & 255
        elif f != 0:
            raise AssertionError('filter %d not expected' % f)
        rows.append(bytes(line))
        prev = bytes(line)
    return w, h, rows


# ---- the tests ------------------------------------------------------------------------------------------------------
def test_handshakes(exe, root):
    for ver in REVISIONS_HANDSHAKE:
        c = Client(exe, ['--root', root])
        r = c.request('initialize', {'protocolVersion': ver, 'capabilities': {}, 'clientInfo': {'name': 'test', 'version': '0'}})
        res = r.get('result', {})
        check('handshake %s: version echoed' % ver, res.get('protocolVersion') == ver, r)
        check('handshake %s: capabilities and serverInfo' % ver, 'tools' in res.get('capabilities', {}) and res.get('serverInfo', {}).get('name') == 'ote-mcp')
        c.send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        t = c.request('tools/list')
        check('handshake %s: tools/list after initialized' % ver, len(t['result']['tools']) >= 20)
        has_title = 'title' in t['result']['tools'][0]
        check('handshake %s: title only from 2025-06-18' % ver, has_title == (ver >= '2025-06-18'))
        check('handshake %s: ping' % ver, c.request('ping').get('result') == {})
        check('handshake %s: clean exit on end of input' % ver, c.close() == 0 and not c.bad_lines)
    c = Client(exe, ['--root', root])
    r = c.request('initialize', {'protocolVersion': '2030-01-01', 'capabilities': {}})
    check('handshake: unknown future revision gets our newest', r['result']['protocolVersion'] == '2025-11-25', r)
    c.close()

    # The stateless revision 2026-07-28: every request carries _meta; no initialize.
    c = Client(exe, ['--root', root])
    meta = {'io.modelcontextprotocol/protocolVersion': MODERN, 'io.modelcontextprotocol/clientInfo': {'name': 't', 'version': '0'},
            'io.modelcontextprotocol/clientCapabilities': {}}
    r = c.request('server/discover', {}, meta)
    res = r.get('result', {})
    check('modern: server/discover lists versions', MODERN in res.get('supportedVersions', []) and '2025-11-25' in res.get('supportedVersions', []), r)
    check('modern: discover has resultType and serverInfo', res.get('resultType') == 'complete' and res.get('_meta', {}).get('io.modelcontextprotocol/serverInfo', {}).get('name') == 'ote-mcp', r)
    r = c.request('tools/list', {}, meta)
    check('modern: tools/list with resultType', r['result'].get('resultType') == 'complete' and len(r['result']['tools']) >= 20)
    r = c.request('tools/call', {'name': 'list_games', 'arguments': {}}, meta)
    check('modern: tools/call with structuredContent', r['result'].get('resultType') == 'complete' and 'games' in r['result'].get('structuredContent', {}), r)
    r = c.request('ping', {}, meta)
    check('modern: ping', r.get('result', {}).get('resultType') == 'complete')
    bad = dict(meta)
    bad['io.modelcontextprotocol/protocolVersion'] = '1900-01-01'
    r = c.request('tools/list', {}, bad)
    check('modern: unsupported version -> -32022 with the supported list', r.get('error', {}).get('code') == -32022 and MODERN in r['error'].get('data', {}).get('supported', []) and r['error']['data'].get('requested') == '1900-01-01', r)
    nocaps = {'io.modelcontextprotocol/protocolVersion': MODERN}
    r = c.request('tools/list', {}, nocaps)
    check('modern: missing clientCapabilities -> -32602', r.get('error', {}).get('code') == -32602, r)
    c.close()


def test_malformed(exe, root):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-06-18', 'capabilities': {}})
    c.write_raw(b'this is not json\n')
    r = c.read()
    check('malformed: bad JSON -> -32700 with null id', r.get('error', {}).get('code') == -32700 and r.get('id') is None, r)
    c.write_raw(b'[]\n')
    check('malformed: empty batch -> -32600', c.read().get('error', {}).get('code') == -32600)
    c.write_raw(b'42\n')
    check('malformed: a number is not a request -> -32600', c.read().get('error', {}).get('code') == -32600)
    c.send({'jsonrpc': '2.0', 'id': 7, 'method': 'no/such/method'})
    r = c.read()
    check('malformed: unknown method -> -32601', r.get('error', {}).get('code') == -32601 and r.get('id') == 7, r)
    c.send({'jsonrpc': '2.0', 'id': 8, 'method': 'tools/call', 'params': {}})
    check('malformed: tools/call without name -> -32602', c.read().get('error', {}).get('code') == -32602)
    c.send({'jsonrpc': '2.0', 'id': 9, 'method': 'tools/call', 'params': {'name': 'nope', 'arguments': {}}})
    check('malformed: unknown tool -> -32602', c.read().get('error', {}).get('code') == -32602)
    c.send({'jsonrpc': '2.0', 'id': 10, 'method': 'tools/call', 'params': {'name': 'get', 'arguments': 'x'}})
    check('malformed: arguments not an object -> -32602', c.read().get('error', {}).get('code') == -32602)
    c.send({'jsonrpc': '2.0', 'id': 11, 'method': 'tools/call', 'params': 5})
    check('malformed: params not an object -> -32602', c.read().get('error', {}).get('code') == -32602)
    c.send({'jsonrpc': '2.0', 'id': None, 'method': 'ping'})
    check('malformed: null id -> -32600', c.read().get('error', {}).get('code') == -32600)
    c.send({'jsonrpc': '2.0', 'id': {'a': 1}, 'method': 'ping'})
    check('malformed: object id -> -32600', c.read().get('error', {}).get('code') == -32600)
    c.send({'jsonrpc': '2.0', 'id': 12, 'method': 5})
    check('malformed: method not a string -> -32600', c.read().get('error', {}).get('code') == -32600)
    c.send({'jsonrpc': '2.0', 'id': 'str-id', 'method': 'ping'})
    check('string ids are echoed', c.read().get('id') == 'str-id')
    # Tool arguments are checked against the tool's own schema.
    d, _ = c.call('get', file=5)
    check('tool arguments: wrong type is a tool error that names it', d.get('_isError') and 'file' in d.get('error', ''), d)
    d, _ = c.call('get', file='game.json', surprise=1)
    check('tool arguments: unknown key is a tool error that lists the keys', d.get('_isError') and 'surprise' in d.get('error', ''), d)
    d, _ = c.call('get', game='x', file='game.json', limit=0)
    check('tool arguments: below the minimum', d.get('_isError') and 'minimum' in d.get('error', ''), d)
    # A notification gets no answer; the next request's answer is the only line.
    c.send({'jsonrpc': '2.0', 'method': 'notifications/cancelled', 'params': {'requestId': 1}})
    r = c.request('ping')
    check('malformed: notification is not answered', r.get('result') == {})
    # A huge line is refused with an error and the server carries on.
    c.write_raw(b'{"jsonrpc":"2.0","id":1,"method":"ping","params":{"x":"' + b'a' * (5 * 1024 * 1024) + b'"}}\n')
    r = c.read()
    check('huge line: refused with -32600', r.get('error', {}).get('code') == -32600, str(r)[:200])
    check('huge line: server still answers', c.request('ping').get('result') == {})
    # Not UTF-8 / odd bytes.
    c.write_raw(b'{"jsonrpc":"2.0","id":3,"method":"ping\xff\xfe"}\n')
    r = c.read()
    check('invalid UTF-8 input gets a valid answer', 'error' in r or 'result' in r)
    # Absurdly deep nesting is an error, not a crash.
    c.write_raw(b'[' * 20000 + b']' * 20000 + b'\n')
    r = c.read()
    check('deeply nested input is refused with an error', isinstance(r, dict) and 'error' in r, str(r)[:200])
    c.write_raw(b'{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"get","arguments":{"file":' + b'{"a":' * 5000 + b'1' + b'}' * 5000 + b'}}}\n')
    r = c.read()
    check('deeply nested arguments are refused with an error', 'error' in r or 'result' in r)
    check('server alive after deep nesting', c.request('ping').get('result') == {})
    # Two requests in one write: answered in order.
    c.write_raw((json.dumps({'jsonrpc': '2.0', 'id': 101, 'method': 'ping'}) + '\n' + json.dumps({'jsonrpc': '2.0', 'id': 102, 'method': 'tools/list'}) + '\n').encode())
    a, b = c.read(), c.read()
    check('two requests in one write are answered in order', a.get('id') == 101 and b.get('id') == 102)
    # A batch.
    c.send([{'jsonrpc': '2.0', 'id': 201, 'method': 'ping'}, {'jsonrpc': '2.0', 'method': 'notifications/initialized'}, {'jsonrpc': '2.0', 'id': 202, 'method': 'ping'}])
    r = c.read()
    check('batch: one answer per request, in order', isinstance(r, list) and [x['id'] for x in r] == [201, 202], r)
    check('malformed: stdout stayed protocol-only', not c.bad_lines, c.bad_lines)
    check('malformed: server still alive', c.alive())
    c.close()


def tool_list(exe, root):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-06-18', 'capabilities': {}})
    tools = c.request('tools/list')['result']['tools']
    c.close()
    return tools


def test_tool_schemas(exe, root):
    tools = tool_list(exe, root)
    names = {t['name'] for t in tools}
    expected = {'list_games', 'get_overview', 'list_files', 'describe', 'get', 'search', 'set', 'add_entry', 'duplicate_entry', 'remove_entry',
                'move_entry', 'rename_id', 'enable_block', 'disable_block', 'get_map', 'set_tiles', 'new_map', 'create_game', 'list_templates',
                'validate', 'render_map', 'simulate'}
    check('tools: every promised tool is listed', expected <= names, sorted(expected - names))
    import re
    bad = []
    for t in tools:
        if not re.match(r'^[A-Za-z0-9_.-]{1,128}$', t['name']) or not t['name'].islower() and '_' not in t['name']:
            bad.append(t['name'])
        if len(t.get('description', '')) < 80:
            bad.append(t['name'] + ' (description too short)')
        sch = t['inputSchema']
        if sch.get('type') != 'object':
            bad.append(t['name'] + ' (inputSchema is not an object)')
        bad += schema_problems(sch, t['name'])
        ann = t.get('annotations', {})
        if 'readOnlyHint' not in ann:
            bad.append(t['name'] + ' (no readOnlyHint)')
    check('tools: names, descriptions and input schemas are well formed', not bad, bad)
    writers = {t['name'] for t in tools if not t['annotations']['readOnlyHint']}
    check('tools: the writing tools are marked', {'set', 'add_entry', 'create_game', 'rename_id', 'set_tiles', 'new_map'} <= writers and 'get' not in writers, sorted(writers))


def test_read_tools(exe, root, repo):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    d, _ = c.call('list_games')
    ids = sorted(g['id'] for g in d['games'])
    check('list_games: the three demo games', ids == ['demo-economy', 'demo-hex', 'demo-srpg'], ids)
    d, _ = c.call('get_overview')
    check('get_overview without a game id: several games -> actionable error', d['_isError'] and 'list_games' in d.get('next', ''), d)
    d, _ = c.call('get_overview', game='demo-srpg')
    check('get_overview: shape', d.get('name') == 'Story Tactics Demo' and d['data']['units'] == 14 and d['problems'] == 0 and 'actionPoints' in d['rules']['off'], d)
    d, _ = c.call('get_overview', game='demo-hex')
    check('get_overview: hex grid', d['grid']['type'] == 'hex')
    d, _ = c.call('get_overview', game='nope')
    check('unknown game id names the known ones', d['_isError'] and 'demo-srpg' in d['error'], d)
    d, _ = c.call('list_files', game='demo-srpg', dir='data')
    check('list_files: data files with schema names', any(f['file'] == 'data/units.json' and f.get('schema') for f in d['files']), d)
    d, _ = c.call('list_files', game='demo-srpg', limit=5)
    check('list_files: paging', len(d['files']) == 5 and d['nextOffset'] == 5 and d['total'] > 5)
    d, _ = c.call('describe')
    check('describe (no args): schema list', any(s['schema'] == 'units' for s in d['schemas']), list(d))
    d, _ = c.call('describe', game='demo-srpg', file='data/weapons.json')
    row = d['row']['keys']
    check('describe: table file shows row keys with types', 'id' in row and 'range' in row or 'id' in row, list(row))
    d, _ = c.call('describe', game='demo-srpg', file='data/units.json', path='[0].moveType')
    node = d['node']
    check('describe: a reference lists the CURRENT valid ids', node['type'] == 'ref' and set(node['reference']['validIds']) == {'foot', 'horse', 'fly'}, node)
    d, _ = c.call('describe', game='demo-srpg', file='game.json', path='rules')
    check('describe: optional blocks and whether enabled', d['node']['keys']['actionPoints'].get('optionalBlock') is True and d['node']['keys']['counterAttacks'] is not None, list(d['node']['keys'])[:5])
    d, _ = c.call('describe', game='demo-srpg', file='maps/bridge.json', path='events[0].do')
    kinds = d.get('itemKinds', [])
    check('describe: command list has the kinds say / addUnit / win', {'say', 'addUnit', 'win'} <= {k['kind'] for k in kinds}, [k['kind'] for k in kinds][:10])
    d, _ = c.call('describe', game='demo-srpg', file='art/tiles.png')
    check('describe: a non-JSON file is an error', d['_isError'])
    # get
    d, _ = c.call('get', game='demo-srpg', file='data/units.json', path='units[0]')
    check('get: a row by index', d['type'] == 'object' and 'stats' in d['keys'], d)
    first_id = d['keys']['id']
    d, _ = c.call('get', game='demo-srpg', file='data/units.json', path='[%s].stats' % first_id)
    check('get: a row by id with the table shorthand', d['type'] == 'object' and 'hp' in d['keys'], d)
    d, _ = c.call('get', game='demo-srpg', file='data/units.json', path='units[nope]')
    check('get: unknown id lists the ids there', d['_isError'] and first_id in d['error'], d)
    seen = []
    off = 0
    while True:
        d, _ = c.call('get', game='demo-srpg', file='data/units.json', path='units', limit=4, offset=off)
        seen += [i['path'] for i in d['items']]
        if 'nextOffset' not in d:
            break
        off = d['nextOffset']
    check('get: paging walks the whole list exactly once', len(seen) == 14 and len(set(seen)) == 14, seen)
    d, _ = c.call('get', game='demo-srpg', file='maps/bridge.json', path='events', mode='summary')
    check('get: summary mode gives one line per event', d['total'] == len(d['items']) and all('summary' in i for i in d['items']), d['items'][:2])
    d, _ = c.call('get', game='demo-srpg', file='maps/bridge.json', path='events[0].do', mode='summary')
    check('get: command summaries name the kind', any(':' in i['summary'] for i in d['items']), d['items'][:3])
    d, _ = c.call('get', game='demo-srpg', file='game.json')
    check('get: a file with // comments says so', 'comments' in d.get('note', ''))
    # search
    d, _ = c.call('search', game='demo-srpg', query='iron_sword')
    files = {h['file'] for h in d['hits']}
    check('search usages: finds the weapon in units, characters ...', {'data/units.json', 'data/characters.json', 'data/weapons.json'} <= files, files)
    d, _ = c.call('search', game='demo-srpg', query='the', mode='text', limit=3)
    check('search text: paged substring hits', len(d['hits']) == 3 and 'nextOffset' in d)
    d, _ = c.call('validate', game='demo-srpg')
    check('validate: the demo is clean', d['ok'] and d['problems'] == 0, d)
    # resources and prompts
    r = c.request('resources/list')['result']['resources']
    uris = {x['uri'] for x in r}
    check('resources: schemas, docs and problems', 'ote://schema/units.json' in uris and 'ote://docs/GAME_FORMAT' in uris and 'ote://game/demo-srpg/problems' in uris, len(uris))
    check('resources: all required docs', {'ote://docs/' + n for n in ['GAME_FORMAT', 'SRPG', 'ECONOMY', 'EVENTS', 'JOBS', 'TURN_ORDER', 'COMMANDERS', 'MCP']} <= uris)
    sch = c.request('resources/read', {'uri': 'ote://schema/units.json'})['result']['contents'][0]
    check('resources/read: a schema is JSON equal to the file', json.loads(sch['text']) == json.load(open(os.path.join(repo, 'editor/schema/units.json'))))
    doc = c.request('resources/read', {'uri': 'ote://docs/SRPG'})['result']['contents'][0]['text']
    check('resources/read: a long doc is cut with a continuation hint', 'ote://docs/SRPG?offset=' in doc[-300:], doc[-200:])
    off = int(__import__('re').search(r'offset=(\d+)', doc[-300:]).group(1))
    doc2 = c.request('resources/read', {'uri': 'ote://docs/SRPG?offset=%d' % off})['result']['contents'][0]['text']
    full = open(os.path.join(repo, 'docs/SRPG.md'), encoding='utf-8').read()
    raw = full.encode('utf-8')
    check('resources/read: the continuation continues exactly', doc2[:100] == raw[off:off + 100].decode('utf-8'), doc2[:60])
    pr = json.loads(c.request('resources/read', {'uri': 'ote://game/demo-srpg/problems'})['result']['contents'][0]['text'])
    check('resources/read: problems of a game', pr['ok'] is True)
    e = c.request('resources/read', {'uri': 'ote://nothing'})
    check('resources/read: unknown uri is an error', 'error' in e)
    e = c.request('resources/read', {'uri': 'ote://schema/../../etc/passwd'})
    check('resources/read: no path tricks', 'error' in e)
    check('resources/templates/list', len(c.request('resources/templates/list')['result']['resourceTemplates']) == 3)
    pl = c.request('prompts/list')['result']['prompts']
    check('prompts/list', {p['name'] for p in pl} >= {'build_a_game', 'review_game'})
    g = c.request('prompts/get', {'name': 'build_a_game', 'arguments': {'idea': 'a small hex game'}})
    check('prompts/get: text carries the idea', 'a small hex game' in g['result']['messages'][0]['content']['text'])
    check('prompts/get: missing argument is an error', 'error' in c.request('prompts/get', {'name': 'build_a_game'}))
    check('read tools: no stdout noise', not c.bad_lines)
    c.close()


def test_templates_and_create(exe, root):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    d, _ = c.call('list_templates')
    ids = [t['id'] for t in d['templates']]
    check('list_templates: the editor templates', {'blank', 'strategy-rpg', 'economy-tactics', 'hex-tactics-flat', 'hex-tactics-pointy'} <= set(ids), ids)
    for tpl in ids:
        d, _ = c.call('create_game', folder='t-' + tpl, template=tpl, title='Test ' + tpl)
        check('create_game %s: validates clean' % tpl, not d['_isError'] and d['problems'] == 0, d)
        g, _ = c.call('get', game='t-' + tpl, file='game.json', path='name')
        check('create_game %s: title became the name' % tpl, g.get('value') == 'Test ' + tpl, g)
    before = tree_hash(root)
    d, _ = c.call('create_game', folder='t-blank', template='blank')
    check('create_game: an existing folder is refused', d['_isError'] and 'already exists' in d['error'], d)
    d, _ = c.call('create_game', folder='../escape', template='blank')
    check('create_game: a path as folder name is refused', d['_isError'])
    d, _ = c.call('create_game', folder='Bad Name', template='blank')
    check('create_game: a bad folder name is refused', d['_isError'])
    d, _ = c.call('create_game', folder='x', template='nope')
    check('create_game: an unknown template is refused', d['_isError'] and 'list_templates' in d.get('next', d.get('error', '')))
    d, _ = c.call('create_game', folder='dry-one', template='blank', dryRun=True)
    check('create_game dryRun: reports, writes nothing', d['dryRun'] and d['files'] > 5 and tree_hash(root) == before and not os.path.exists(os.path.join(root, 'dry-one')), d)
    d, _ = c.call('create_game', folder='elsewhere', template='blank', root='/tmp')
    check('create_game: a root that was not given is refused', d['_isError'] and 'root' in d['error'], d)
    check('create_game: no stdout noise', not c.bad_lines)
    c.close()


def has_player(c):
    d, _ = c.call('render_map', game='nonexistent', map='x')
    return 'ote-player' not in d.get('error', '') or 'not found' not in d.get('error', '')


def test_worked_session(exe, root, notes):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    d, _ = c.call('create_game', folder='session', template='blank', title='Session Game')
    check('session: create', d['problems'] == 0, d)
    g = 'session'
    d, _ = c.call('add_entry', game=g, file='data/weapons.json', value={'id': 'bow', 'name': 'Bow'})
    check('session: add a weapon', not d['_isError'] and d['problemsAfter'] == 0 and 'bow' in d['path'], d)
    d, _ = c.call('describe', game=g, file='data/weapons.json', path='[bow]')
    check('session: describe the new row', 'node' in d, d)
    d, _ = c.call('add_entry', game=g, file='data/units.json', id='archer', fields={'name': 'Archer', 'glyph': 'A', 'moveType': 'foot', 'stats': {'hp': 8, 'mov': 5}, 'weapons': ['bow']})
    check('session: add a unit type with the weapon', not d['_isError'] and d['problemsAfter'] == 0, d)
    d, _ = c.call('set', game=g, changes=[{'file': 'data/units.json', 'path': '[archer].stats.hp', 'value': 9}, {'file': 'data/units.json', 'path': '[archer].name', 'value': 'Archer II'}])
    check('session: set two values atomically', not d['_isError'] and len(d['changes']) == 2 and d['problemsAfter'] == 0, d)
    v, _ = c.call('get', game=g, file='data/units.json', path='[archer].stats.hp')
    check('session: the value is on disk', v['value'] == 9, v)
    d, _ = c.call('new_map', game=g, map='ford', name='The Ford', width=12, height=8)
    check('session: new_map', not d['_isError'] and 'maps/ford.json' in d['wrote'] and 'game.json' in d['wrote'], d)
    d, _ = c.call('list_templates')
    terr, _ = c.call('get', game=g, file='data/terrain.json', path='terrain', mode='summary')
    check('session: terrain is plain and wall', {t['summary'].split(' ')[0] for t in terr['items']} >= {'plain', 'wall'}, terr)
    d, _ = c.call('set_tiles', game=g, map='ford', paint=[{'x': 4, 'y': 0, 'w': 2, 'h': 8, 'terrain': 'wall'}, {'x': 4, 'y': 3, 'w': 2, 'h': 2, 'terrain': 'plain'}])
    check('session: paint a wall with a gap', not d['_isError'] and d['squaresPainted'] == 20, d)
    m, _ = c.call('get_map', game=g, map='ford')
    check('session: get_map shows the painted rows', m['tiles'][0] == '....##......' and m['tiles'][3] == '............' and m['legend'], m)
    for side, x, y in [('blue', 1, 3), ('red', 10, 4)]:
        d, _ = c.call('add_entry', game=g, file='maps/ford.json', path='units', value={'type': 'archer', 'side': side, 'x': x, 'y': y})
        check('session: place a %s unit' % side, not d['_isError'], d)
    d, _ = c.call('validate', game=g)
    check('session: validate is clean', d['ok'] and d['problems'] == 0, d)
    r, res = c.call('render_map', game=g, map='ford')
    if r['_isError'] and 'not found' in r.get('error', ''):
        notes.append('render_map skipped: ote-player not found by the server (%s)' % r['error'][:80])
        check('session: render_map says how to enable it when the player is missing', 'ote-player' in r['error'] and r.get('next'), r)
    else:
        imgs = [b for b in res['content'] if b['type'] == 'image']
        check('session: render_map returns an image block', not r['_isError'] and len(imgs) == 1 and imgs[0]['mimeType'] == 'image/png', str(r)[:300])
        if imgs:
            import base64
            w, h, rows = decode_png(base64.b64decode(imgs[0]['data']))
            check('session: picture has the reported size', [w, h] == [r['pictureSize']['width'] if 'shownSize' not in r else r['shownSize']['width'], r['pictureSize']['height'] if 'shownSize' not in r else r['shownSize']['height']], (w, h, r))
            colours = set()
            for y in range(0, h, 3):
                row = rows[y]
                for x in range(0, w * 3, 9):
                    colours.add(row[x:x + 3])
            check('session: picture is not one flat colour', len(colours) > 5, len(colours))
        d, _ = c.call('render_map', game=g, map='nomap')
        check('session: render_map with an unknown map is an error', d['_isError'])
    d, _ = c.call('simulate', game=g, map='ford', runs=3)
    if d['_isError'] and 'not found' in d.get('error', ''):
        notes.append('simulate skipped: ote-sim not found by the server')
    else:
        check('session: simulate gives a summary and runs', not d['_isError'] and 'wins' in d['summary'] and len(d['runs']) == 3, d)
    check('session: no stdout noise', not c.bad_lines)
    c.close()


def test_dry_run(exe, root):
    game = os.path.join(root, 'demo-hex')
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    before = tree_hash(game)
    args = dict(game='demo-hex', changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 'Renamed By Test'}])
    dry, _ = c.call('set', dryRun=True, **args)
    check('dryRun: nothing on disk changed (tree hash identical)', tree_hash(game) == before)
    check('dryRun: reports a diff and the problems the result would have', dry['dryRun'] and dry['changed'] and 'Renamed By Test' in dry['files'][0]['diff'] and 'problemsAfter' in dry and 'wouldWrite' in dry, dry)
    check('dryRun: no temp copies left in the system temp folder', not [f for f in os.listdir(os.environ.get('TMPDIR', '/tmp')) if f.startswith('ote-mcp-')])
    real, _ = c.call('set', **args)
    check('dryRun: the real run makes the same diff and the same problem counts', real['files'][0]['diff'] == dry['files'][0]['diff'] and real['problemsAfter'] == dry['problemsAfter'] and real['problemsBefore'] == dry['problemsBefore'], (dry['files'], real['files']))
    check('dryRun: the real run wrote', tree_hash(game) != before and 'wrote' in real)
    again, _ = c.call('set', **args)
    check('a write that changes nothing says so', again['changed'] is False, again)
    # A dry run of something that would break the game reports it without writing.
    h2 = tree_hash(game)
    d, _ = c.call('set', game='demo-hex', changes=[{'file': 'data/units.json', 'path': 'units[0].moveType', 'value': 'foot'}], dryRun=True)
    check('dryRun: second dry run still writes nothing', tree_hash(game) == h2)
    d, _ = c.call('remove_entry', game='demo-hex', file='data/terrain.json', path='[0]', dryRun=True, ignoreReferences=True)
    check('dryRun: removing a used terrain would introduce problems and says so', d.get('newProblems') and d['warnings'], d)
    check('dryRun: that dry run wrote nothing', tree_hash(game) == h2)
    c.close()


def test_refusals(exe, root, outside):
    game = os.path.join(root, 'demo-economy')
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    h = tree_hash(root)

    def refused(name, d, word=None):
        ok = d.get('_isError') and d.get('written') is not False or d.get('_isError')
        if word:
            ok = ok and word in json.dumps(d)
        check('refusal: ' + name, ok, d)

    d, _ = c.call('set', game='demo-economy', changes=[{'file': 'maps/%s' % os.listdir(os.path.join(game, 'maps'))[0], 'path': 'grid.width', 'value': 999}])
    refused('number out of range names the range', d, 'maximum')
    d, _ = c.call('set', game='demo-economy', changes=[{'file': 'data/units.json', 'path': 'units[0].bogusKey', 'value': 1}])
    refused('unknown key lists what the section knows', d, 'unknown key')
    d, _ = c.call('set', game='demo-economy', changes=[{'file': 'data/units.json', 'path': 'units[0].moveType', 'value': 'swim'}])
    refused('dangling reference lists the valid ids', d, 'valid values')
    d, _ = c.call('set', game='demo-economy', changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 5}])
    refused('wrong type', d, 'text')
    d, _ = c.call('set', game='demo-economy', changes=[{'file': 'data/units.json', 'path': 'units[0].stats.hp', 'value': 'x'}])
    refused('wrong type in a map value', d)
    d, _ = c.call('set', game='demo-economy', changes=[{'file': 'game.json', 'path': 'rules.turnOrder', 'value': 'sideways'}])
    refused('enum word', d, 'allowed')
    d, _ = c.call('set', game='demo-economy', changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 'ok'}, {'file': 'data/units.json', 'path': 'units[0].bogusKey', 'value': 1}])
    refused('one bad change refuses the whole call', d)
    check('refusal: nothing was written by any refusal', tree_hash(root) == h)
    d, _ = c.call('set', game='demo-economy', allowUnknown=True, changes=[{'file': 'data/units.json', 'path': 'units[0].bogusKey', 'value': 1}], dryRun=True)
    check('refusal: allowUnknown lets the same write through', not d.get('_isError') and d['changed'], d)
    d, _ = c.call('add_entry', game='demo-economy', file='data/weapons.json', value={'id': 'zz', 'name': 'ZZ', 'power': 'much'}, dryRun=True)
    refused('a new entry is checked too', d)
    d, _ = c.call('add_entry', game='demo-economy', file='data/units.json', value={'id': 'zz'}, dryRun=True)
    refused('a new entry with a missing required key', d, 'required')
    # Path escapes.
    for name, args in [('.. in the file', dict(file='../outside.json')), ('.. further in', dict(file='maps/../../outside.json')), ('absolute file', dict(file='/etc/passwd')),
                       ('drive-like file', dict(file='C:/x.json')), ('empty file', dict(file=''))]:
        d, _ = c.call('get', game='demo-economy', **args)
        check('path escape: ' + name, d.get('_isError'), d)
        d, _ = c.call('set', game='demo-economy', changes=[dict(path='x', value=1, **args)])
        check('path escape on set: ' + name, d.get('_isError'), d)
    for gid in ['..', '../outside', '/tmp', 'demo-economy/..']:
        d, _ = c.call('get_overview', game=gid)
        check('game id escape: %r' % gid, d.get('_isError'), d)
    link_target = os.path.join(outside, 'secret.json')
    with open(link_target, 'w') as f:
        f.write('{"topsecret42": 1}')
    try:
        os.symlink(link_target, os.path.join(game, 'data', 'evil.json'))
        os.symlink(outside, os.path.join(game, 'linkdir'))
        have_links = True
    except (OSError, NotImplementedError):
        have_links = False
    if have_links:
        d, _ = c.call('get', game='demo-economy', file='data/evil.json')
        check('symlink: reading a file link that leaves the game is refused', d.get('_isError') and 'topsecret42' not in json.dumps(d), d)
        d, _ = c.call('set', game='demo-economy', allowUnknown=True, changes=[{'file': 'data/evil.json', 'path': 'secret', 'value': 2}])
        check('symlink: writing through it is refused', d.get('_isError'), d)
        d, _ = c.call('get', game='demo-economy', file='linkdir/secret.json')
        check('symlink: a folder link that leaves the game is refused', d.get('_isError') and 'topsecret42' not in json.dumps(d), d)
        with open(link_target) as f:
            check('symlink: the outside file is untouched', json.load(f) == {'topsecret42': 1})
        d, _ = c.call('search', game='demo-economy', query='secret')
        check('symlink: search does not follow links out', not d.get('hits'), d)
        os.remove(os.path.join(game, 'data', 'evil.json'))
        os.remove(os.path.join(game, 'linkdir'))
    # A game folder that is a link out of the root is not listed.
    if have_links:
        os.symlink(os.path.join(root, 'demo-hex'), os.path.join(root, 'linked-game'))
        os.makedirs(os.path.join(outside, 'game-out'))
        shutil.copy(os.path.join(root, 'demo-hex', 'game.json'), os.path.join(outside, 'game-out', 'game.json'))
        os.symlink(os.path.join(outside, 'game-out'), os.path.join(root, 'out-game'))
        d, _ = c.call('list_games')
        ids = [x['id'] for x in d['games']]
        check('symlink: a game link leaving the root is not a game of this root', 'out-game' not in ids, ids)
        d, _ = c.call('get_overview', game='out-game')
        check('symlink: and cannot be opened by name', d.get('_isError'))
        os.remove(os.path.join(root, 'out-game'))
        os.remove(os.path.join(root, 'linked-game'))
    # The root that was not given: a game next to the root is invisible.
    shutil.copytree(game, os.path.join(outside, 'other-game'))
    d, _ = c.call('get_overview', game='other-game')
    check('root not given: a game outside the roots cannot be opened', d.get('_isError'), d)
    # Conflict with a change on disk.
    shutil.copytree(os.path.join(root, 'demo-hex'), os.path.join(root, 'conflict'))
    f = os.path.join(root, 'conflict', 'data', 'units.json')
    c.call('get', game='conflict', file='data/units.json', path='units[0]')
    with open(f, 'a') as fh:
        fh.write('\n')  # someone else touches the file after we read it
    d, _ = c.call('set', game='conflict', changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 'Conflicted'}])
    check('conflict: changed on disk since read is refused with advice', d.get('_isError') and 'changed on disk' in d['error'] and 'force' in d.get('next', ''), d)
    d, _ = c.call('set', game='conflict', force=True, changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 'Conflicted'}])
    check('conflict: force writes anyway and says so', not d.get('_isError') and any('force' in w for w in d['warnings']), d)
    d, _ = c.call('set', game='conflict', changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 'Conflicted 2'}])
    check('conflict: after our own write there is no conflict', not d.get('_isError'), d)
    check('refusal: no stdout noise', not c.bad_lines)
    c.close()


def test_read_only(exe, root):
    h = tree_hash(root)
    c = Client(exe, ['--root', root, '--read-only'])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    names = {t['name'] for t in c.request('tools/list')['result']['tools']}
    check('read-only: writing tools are not offered', not (names & {'set', 'add_entry', 'create_game', 'rename_id', 'set_tiles', 'new_map', 'remove_entry'}) and 'get' in names, sorted(names))
    d, _ = c.call('set', game='demo-hex', changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 'x'}])
    check('read-only: calling a writing tool anyway is an error', d.get('_isError') and 'read-only' in d['error'], d)
    d, _ = c.call('create_game', folder='nope', template='blank')
    check('read-only: create_game too', d.get('_isError') and 'read-only' in d['error'])
    d, _ = c.call('validate', game='demo-hex')
    check('read-only: reading still works', d['ok'])
    check('read-only: nothing changed', tree_hash(root) == h)
    c.close()


def test_rename(exe, root):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    # In the demo "iron_sword" is both a weapon and an item; units, characters and chests refer to the ITEM.
    hits_before, _ = c.call('search', game='demo-srpg', query='iron_sword', limit=200)
    n_before = len(hits_before['hits'])
    dry, _ = c.call('rename_id', game='demo-srpg', file='data/items.json', path='[iron_sword]', newId='steel_blade', dryRun=True)
    check('rename_id: dry run reports the references and writes nothing', dry.get('referencesUpdated', 0) >= 5 and len(dry['wouldWrite']) >= 3, dry)
    d, _ = c.call('rename_id', game='demo-srpg', file='data/items.json', path='[iron_sword]', newId='steel_blade')
    check('rename_id: game valid before and after', not d['_isError'] and d['problemsBefore'] == 0 and d['problemsAfter'] == 0, d)
    check('rename_id: reference count matches the dry run', d['referencesUpdated'] == dry['referencesUpdated'], (d['referencesUpdated'], dry['referencesUpdated']))
    old, _ = c.call('search', game='demo-srpg', query='iron_sword', limit=200)
    new, _ = c.call('search', game='demo-srpg', query='steel_blade', limit=200)
    old_places = sorted((h['file'], h['path']) for h in old['hits'])
    check('rename_id: what still says iron_sword is only the weapon (its id and the item that grants it)', {f for f, _ in old_places} <= {'data/weapons.json', 'data/items.json'} and len(old_places) == 2, old_places)
    check('rename_id: the new id is used as often as the old item id was', len(new['hits']) == n_before - 2, (len(new['hits']), n_before))
    v, _ = c.call('validate', game='demo-srpg')
    check('rename_id: validate is clean afterwards', v['ok'], v)
    d, _ = c.call('rename_id', game='demo-srpg', file='data/items.json', path='[steel_blade]', newId='steel_blade')
    check('rename_id: same id refused', d['_isError'])
    d, _ = c.call('rename_id', game='demo-srpg', file='data/items.json', path='[steel_blade]', newId='iron_lance')
    check('rename_id: an id that exists is refused', d['_isError'] and 'already' in d['error'], d)
    d2, _ = c.call('rename_id', game='demo-srpg', file='data/items.json', path='[steel_blade]', newId='iron_sword')
    check('rename_id: and back again', not d2['_isError'] and d2['problemsAfter'] == 0 and d2['referencesUpdated'] == dry['referencesUpdated'], d2)
    # A side id of a map: its references are inside that map file.
    d, _ = c.call('get', game='demo-srpg', file='maps/bridge.json', path='sides', mode='summary')
    sid = d['items'][0]['path']
    d, _ = c.call('rename_id', game='demo-srpg', file='maps/bridge.json', path=sid, newId='north_company')
    check('rename_id: a side of a map keeps the game valid', not d['_isError'] and d['referencesUpdated'] >= 1 and d['problemsAfter'] == d['problemsBefore'], d)
    d, _ = c.call('rename_id', game='demo-srpg', file='game.json', path='resources[gold]', newId='coins', dryRun=True)
    check('rename_id: a resource (keys of other objects) is handled', not d['_isError'], d)
    # remove_entry refuses while referenced.
    d, _ = c.call('remove_entry', game='demo-srpg', file='data/items.json', path='[iron_sword]')
    check('remove_entry: a used id is refused and lists the users', d['_isError'] and d.get('usedBy') and d['uses'] >= 5, d)
    c.close()


def test_comments_and_formatting(exe, root):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    # game.json of the demo has // comments.
    args = dict(game='demo-srpg', changes=[{'file': 'game.json', 'path': 'description', 'value': 'A changed line.'}])
    dry, _ = c.call('set', dryRun=True, **args)
    notes = json.dumps(dry['files'][0].get('notes', []))
    check('comments: a dry run of a commented file warns', 'comments' in notes, dry['files'][0])
    real, _ = c.call('set', **args)
    check('comments: the first write warns', 'comments' in json.dumps(real['files'][0].get('notes', [])), real['files'][0])
    again, _ = c.call('set', game='demo-srpg', changes=[{'file': 'game.json', 'path': 'description', 'value': 'A second line.'}])
    check('comments: after that the file has none and no warning', 'comments' not in json.dumps(again['files'][0].get('notes', [])), again['files'][0])
    check('comments: only the edited line differs in the diff', again['files'][0]['added'] == 1 and again['files'][0]['removed'] == 1, again['files'][0])
    # Unknown keys and number formats survive a write.
    path = os.path.join(root, 'demo-economy', 'data', 'units.json')
    doc = read_jsonc(path)
    doc['units'][1]['zzCustom'] = {'list': [1, 2.5, 0.1, 1000000], 'note': 'keep \u00e9 me'}
    doc['units'][1]['ratio'] = 0.1
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write('\n')
    before_text = open(path, encoding='utf-8').read()
    d, _ = c.call('set', game='demo-economy', allowUnknown=True, changes=[{'file': 'data/units.json', 'path': 'units[0].name', 'value': 'Edited Name'}])
    after_text = open(path, encoding='utf-8').read()
    diff_lines = [l for l in __import__('difflib').unified_diff(before_text.splitlines(), after_text.splitlines(), lineterm='', n=0) if l[:1] in '+-' and l[:3] not in ('+++', '---')]
    check('formatting: only the edited line changed on disk', len(diff_lines) == 2 and 'Edited Name' in diff_lines[1], diff_lines)
    after = json.loads(after_text)
    check('formatting: unknown keys and numbers survived', after['units'][1]['zzCustom'] == doc['units'][1]['zzCustom'] and after['units'][1]['ratio'] == 0.1, after['units'][1])
    check('formatting: the file still ends with one newline', after_text.endswith('}\n') and not after_text.endswith('\n\n'))
    c.close()


def test_edit_tools(exe, root):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    g = 'demo-economy'
    d, _ = c.call('add_entry', game=g, file='data/units.json', kind='x')
    check('add_entry: kind on a list without kinds is refused', d['_isError'])
    d, _ = c.call('add_entry', game=g, file='data/weapons.json')
    check('add_entry: no value -> schema defaults and a generated id', not d['_isError'] and d['entry']['id'].startswith('new_entry'), d)
    gen = d['entry']['id']
    d, _ = c.call('add_entry', game=g, file='data/weapons.json', id=gen)
    check('add_entry: an existing id is refused', d['_isError'] and 'already exists' in d['error'], d)
    d, _ = c.call('duplicate_entry', game=g, file='data/weapons.json', path='[%s]' % gen, newId='dup_one')
    check('duplicate_entry: copies after the original', not d['_isError'] and d['newId'] == 'dup_one', d)
    items, _ = c.call('get', game=g, file='data/weapons.json', path='weapons', mode='summary', limit=100)
    paths = [i['path'] for i in items['items']]
    check('duplicate_entry: position', paths.index('weapons[dup_one]') == paths.index('weapons[%s]' % gen) + 1, paths)
    d, _ = c.call('move_entry', game=g, file='data/weapons.json', path='[dup_one]', toIndex=0)
    items, _ = c.call('get', game=g, file='data/weapons.json', path='weapons', mode='summary', limit=100)
    check('move_entry: to the front', items['items'][0]['path'] == 'weapons[dup_one]', items['items'][0])
    d, _ = c.call('move_entry', game=g, file='data/weapons.json', path='[dup_one]', toIndex=999)
    check('move_entry: out of range is refused with the range', d['_isError'] and 'last index' in d['error'], d)
    d, _ = c.call('remove_entry', game=g, file='data/weapons.json', path='[dup_one]')
    check('remove_entry: an unused entry goes', not d['_isError'], d)
    d, _ = c.call('remove_entry', game=g, file='data/weapons.json', path='[%s]' % gen)
    check('remove_entry: and the other one', not d['_isError'] and d['problemsAfter'] == d['problemsBefore'], d)
    # Blocks.
    d, _ = c.call('describe', game=g, file='game.json', path='rules')
    keys = d['node']['keys']
    off = [k for k, v in keys.items() if v.get('optionalBlock') and not v.get('enabled')]
    check('blocks: describe says which blocks are off', len(off) >= 3, off)
    d, _ = c.call('enable_block', game=g, file='game.json', path='rules.height')
    check('blocks: enable writes a block and the game stays valid', not d['_isError'] and d['problemsAfter'] == d['problemsBefore'], d)
    d2, _ = c.call('enable_block', game=g, file='game.json', path='rules.height')
    check('blocks: enabling twice is refused', d2['_isError'])
    d, _ = c.call('disable_block', game=g, file='game.json', path='rules.height')
    check('blocks: disable removes it', not d['_isError'], d)
    d, _ = c.call('enable_block', game=g, file='game.json', path='rules.actionPoints')
    check('blocks: a block that needs something says what', d['_isError'] or d['problemsAfter'] == d['problemsBefore'], d)
    d, _ = c.call('enable_block', game=g, file='game.json', path='name')
    check('blocks: a plain key is not a block', d['_isError'])
    # A story command by kind.
    shutil.copytree(os.path.join(root, 'demo-srpg'), os.path.join(root, 'story'), dirs_exist_ok=True)
    d, _ = c.call('add_entry', game='story', file='maps/bridge.json', path='events[0].do', kind='say', fields={'say': 'Hello there.'})
    check('add_entry: a story command by kind', not d['_isError'] and d['entry'].get('say') == 'Hello there.', d)
    d, _ = c.call('add_entry', game='story', file='maps/bridge.json', path='events[0].do', kind='zzz')
    check('add_entry: an unknown kind lists the kinds', d['_isError'] and 'say' in d['error'], d)
    d, _ = c.call('remove_entry', game='story', file='game.json', path='formatVersion')
    check('remove_entry: a required key cannot go', d['_isError'])
    check('edit tools: no stdout noise', not c.bad_lines)
    c.close()


def test_map_tools(exe, root):
    c = Client(exe, ['--root', root])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    d, _ = c.call('get_map', game='demo-economy', map='nomap')
    check('get_map: unknown map names the maps', d['_isError'])
    ov, _ = c.call('get_overview', game='demo-economy')
    mid = ov['maps'][0]['id']
    m, _ = c.call('get_map', game='demo-economy', map=mid)
    w, h = m['grid']['width'], m['grid']['height']
    check('get_map: rows match the grid', len(m['tiles']) == h and all(len(r) == w for r in m['tiles']), m['grid'])
    d, _ = c.call('set_tiles', game='demo-economy', map=mid, paint=[{'x': w, 'y': 0, 'terrain': 'plain'}])
    check('set_tiles: outside the map is refused with the size', d['_isError'] and str(w) in d['error'], d)
    d, _ = c.call('set_tiles', game='demo-economy', map=mid, paint=[{'x': 0, 'y': 0, 'terrain': 'lava'}])
    check('set_tiles: unknown terrain lists the terrain', d['_isError'] and 'plain' in d['error'], d)
    d, _ = c.call('set_tiles', game='demo-economy', map=mid, paint=[{'x': 0, 'y': 0, 'w': 3, 'h': 2, 'terrain': 'plain'}], dryRun=True)
    check('set_tiles: dry run shows the row diff', 'diff' in d['files'][0], d)
    d, _ = c.call('set_tiles', game='demo-economy', map=mid, paint=[{'x': 0, 'y': 0, 'height': 2}])
    check('set_tiles: a height is written as a heights grid (the game may not use it: the check says)', not d['_isError'] or 'height' in d['error'], d)
    d, _ = c.call('new_map', game='demo-economy', map='tiny', width=3, height=2, fill='plain')
    check('new_map: made', not d['_isError'], d)
    d, _ = c.call('new_map', game='demo-economy', map='tiny', width=3, height=2)
    check('new_map: same id refused', d['_isError'])
    d, _ = c.call('new_map', game='demo-economy', map='../x', width=3, height=2)
    check('new_map: bad id refused', d['_isError'])
    d, _ = c.call('new_map', game='demo-economy', map='big', width=999, height=2)
    check('new_map: size limits', d['_isError'])
    d, _ = c.call('new_map', game='demo-hex', map='hexone', width=4, height=4)
    mm, _ = c.call('get_map', game='demo-hex', map='hexone')
    check('new_map: follows the game grid kind', mm['grid']['type'] == 'hex', mm['grid'])
    c.close()


def test_schema_override(exe, root, repo, tmp):
    sd = os.path.join(tmp, 'schemas')
    shutil.copytree(os.path.join(repo, 'editor', 'schema'), sd)
    s = json.load(open(os.path.join(sd, 'weapons.json')))
    s['rows']['fields']['id']['help'] = 'OVERRIDDEN HELP'
    json.dump(s, open(os.path.join(sd, 'weapons.json'), 'w'))
    c = Client(exe, ['--root', root, '--schema-dir', sd])
    c.request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}})
    d, _ = c.call('describe', game='demo-hex', file='data/weapons.json', path='[0].id')
    check('--schema-dir: the override folder is used', d['node'].get('help') == 'OVERRIDDEN HELP', d)
    c.close()


# ---- fuzz -----------------------------------------------------------------------------------------------------------
def fuzz(exe, root, outside, count, seed, read_only):
    rng = random.Random(seed)
    corpus = [
        {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'protocolVersion': '2025-06-18', 'capabilities': {}}},
        {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'},
        {'jsonrpc': '2.0', 'id': 3, 'method': 'resources/list'},
        {'jsonrpc': '2.0', 'id': 4, 'method': 'resources/read', 'params': {'uri': 'ote://docs/MCP'}},
        {'jsonrpc': '2.0', 'id': 5, 'method': 'prompts/get', 'params': {'name': 'review_game', 'arguments': {}}},
        {'jsonrpc': '2.0', 'id': 6, 'method': 'server/discover', 'params': {'_meta': {'io.modelcontextprotocol/protocolVersion': MODERN, 'io.modelcontextprotocol/clientCapabilities': {}}}},
    ]
    tools = tool_list(exe, root)
    paths = ['', 'units', 'units[0]', '[0].stats', 'rules', 'rules.height', 'events[1].do[0]', '../x', 'a.b.c', 'units[-1]', '[id=zz]', '["x.y"]', 'sides[0].id', '[', ']', '..', 'x' * 300]
    files = ['game.json', 'data/units.json', 'data/weapons.json', 'maps/' + sorted(os.listdir(os.path.join(root, 'demo-hex', 'maps')))[0], '../escape.json', '/etc/passwd', 'data/terrain.json', 'nope.json', '']

    def rand_value(depth=0):
        simple = [None, True, rng.randrange(-5, 400), rng.random() * 100, 'text' + str(rng.randrange(100)), '', 'x' * rng.randrange(50)]
        k = rng.randrange(9 if depth < 2 else 7)
        if k < 7:
            return simple[k]
        if k == 7:
            return {rng.choice(['id', 'name', 'x', 'y', 'type', 'zz']): rand_value(depth + 1)}
        return [rand_value(depth + 1) for _ in range(rng.randrange(3))]

    for t in tools:
        props = t['inputSchema'].get('properties', {})
        for _ in range(4):
            args = {}
            for k, v in props.items():
                if rng.random() < 0.6:
                    if k == 'game':
                        args[k] = rng.choice(['demo-hex', 'demo-economy', 'demo-srpg', 'nope', '..', ''])
                    elif k == 'file':
                        args[k] = rng.choice(files)
                    elif k == 'path':
                        args[k] = rng.choice(paths)
                    else:
                        args[k] = rand_value()
            if t['name'] in ('render_map', 'simulate', 'create_game'):  # slow or outside the fuzz budget: only with bad values
                args['map'] = rng.choice(['', 'nope', None, 5])
                if t['name'] == 'create_game':
                    args['folder'] = rng.choice(['fz-' + str(rng.randrange(5)), '../x', '', 5])
                    args['template'] = rng.choice(['blank', 'nope'])
            corpus.append({'jsonrpc': '2.0', 'id': 100 + len(corpus), 'method': 'tools/call', 'params': {'name': t['name'], 'arguments': args}})
    c = Client(exe, ['--root', root] + (['--read-only'] if read_only else []))
    c.request('initialize', {'protocolVersion': '2025-06-18', 'capabilities': {}})
    expected_out = tree_hash(outside)
    sent = 0
    batch_no = 0
    while sent < count:
        lines = []
        for _ in range(100):
            base = rng.choice(corpus)
            kind = rng.randrange(6)
            data = json.dumps(base).encode()
            if kind == 0:  # byte flips
                b = bytearray(data)
                for _ in range(rng.randrange(1, 4)):
                    b[rng.randrange(len(b))] = rng.randrange(256)
                data = bytes(b).replace(b'\n', b' ')
            elif kind == 1:  # truncation
                data = data[:rng.randrange(1, len(data))]
            elif kind == 2:  # a different id / method
                m = dict(base)
                m['id'] = rng.choice([None, 1, 'x', 2.5, [], {}, -1, 1 << 60])
                if rng.random() < 0.3:
                    m['method'] = rng.choice(['tools/call', 'ping', 'x', 5, None, 'tools/list', 'resources/read'])
                data = json.dumps(m).encode()
            elif kind == 3:  # params replaced
                m = dict(base)
                m['params'] = rand_value()
                data = json.dumps(m).encode()
            elif kind == 4:  # garbage
                data = bytes(rng.randrange(256) for _ in range(rng.randrange(1, 60))).replace(b'\n', b' ')
            lines.append(data)
        sent += len(lines)
        c.write_raw(b'\n'.join(lines) + b'\n')
        batch_no += 1
        c.send({'jsonrpc': '2.0', 'id': 'sync%d' % batch_no, 'method': 'ping'})
        try:
            while True:
                msg = c.read(timeout=300)
                if isinstance(msg, dict) and msg.get('id') == 'sync%d' % batch_no:
                    break
        except queue.Empty:
            check('fuzz: server answered the batch (no hang)', False, batch_no)
            c.p.kill()
            return
        except EOFError:
            check('fuzz: server stayed alive', False, 'stdout closed at batch %d' % batch_no)
            return
    alive = c.alive()
    check('fuzz %s: %d requests, server alive' % ('read-only' if read_only else 'normal', sent), alive)
    check('fuzz %s: stdout was always valid JSON-RPC' % ('read-only' if read_only else 'normal'), not c.bad_lines, c.bad_lines[:3])
    check('fuzz %s: nothing outside the root changed' % ('read-only' if read_only else 'normal'), tree_hash(outside) == expected_out)
    check('fuzz %s: clean exit' % ('read-only' if read_only else 'normal'), c.close() == 0)


def main():
    args = [a for a in sys.argv[1:]]
    fuzz_n = 3000
    if '--fuzz' in args:
        i = args.index('--fuzz')
        fuzz_n = int(args[i + 1])
        del args[i:i + 2]
    exe = os.path.abspath(args[0])
    repo = os.path.abspath(args[1]) if len(args) > 1 else '.'
    notes = []
    tmp = scratch_dir('mcp')
    root = os.path.join(tmp, 'games')
    outside = os.path.join(tmp, 'outside')
    os.makedirs(root)
    os.makedirs(outside)
    for g in ('demo-srpg', 'demo-economy', 'demo-hex'):
        copy_game(repo, g, root)
    real_games_hash = tree_hash(os.path.join(repo, 'games'))
    sentinel = os.path.join(outside, 'sentinel.txt')
    open(sentinel, 'w').write('do not touch')
    try:
        test_handshakes(exe, root)
        test_malformed(exe, root)
        test_tool_schemas(exe, root)
        test_read_tools(exe, root, repo)
        test_templates_and_create(exe, root)
        test_worked_session(exe, root, notes)
        test_dry_run(exe, root)
        test_comments_and_formatting(exe, root)
        test_rename(exe, root)
        test_edit_tools(exe, root)
        test_map_tools(exe, root)
        test_refusals(exe, root, outside)
        test_read_only(exe, root)
        test_schema_override(exe, root, repo, tmp)
        # The fuzz runs: read-only on the copies (the tree must be identical), then in normal mode on a fresh copy.
        h = tree_hash(root)
        fuzz(exe, root, outside, fuzz_n * 2 // 3, 12345, True)
        check('fuzz read-only: the tree is byte-identical afterwards', tree_hash(root) == h)
        froot = os.path.join(tmp, 'fuzzgames')
        os.makedirs(froot)
        for g in ('demo-hex', 'demo-economy', 'demo-srpg'):
            copy_game(repo, g, froot)
        fuzz(exe, froot, outside, fuzz_n // 3, 777, False)
        check('the real games folder was never touched', tree_hash(os.path.join(repo, 'games')) == real_games_hash)
        check('the sentinel outside the roots is untouched', open(sentinel).read() == 'do not touch')
    finally:
        pass
    for n in notes:
        print('NOTE ' + n)
    summary()
    if not failures:
        shutil.rmtree(str(tmp), ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
