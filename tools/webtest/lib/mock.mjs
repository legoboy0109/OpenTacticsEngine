// Builds a static mock of the editor helper's HTTP API in a temp folder:
//   api/projects/<id>/files            JSON list (no extension: the server sends JSON)
//   api/projects/<id>/file/<path>      project files
//   assets/                            copy of the build's assets
//   player/                            copy of the web build (embed.html, ote-player.*)
//   host.html                          tiny page with the player in an iframe
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

function walk(dir, base = dir) {
  const out = [];
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, e.name);
    if (e.isDirectory()) out.push(...walk(full, base));
    else out.push(path.relative(base, full).split(path.sep).join('/'));
  }
  return out;
}

function addProject(root, id, srcDir, mutate) {
  const files = walk(srcDir).map((p) => ({ path: p, bytes: fs.readFileSync(path.join(srcDir, p)) }));
  if (mutate) mutate(files);
  const apiDir = path.join(root, 'api/projects', id);
  fs.mkdirSync(path.join(apiDir, 'file'), { recursive: true });
  for (const f of files) {
    const dest = path.join(apiDir, 'file', f.path);
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    fs.writeFileSync(dest, f.bytes);
  }
  fs.writeFileSync(path.join(apiDir, 'files'),
    JSON.stringify({ files: files.map((f) => ({ path: f.path, size: f.bytes.length })) }));
}

// Returns the temp folder (caller removes it with fs.rmSync).
export function buildMock(build, gamesSrc) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'ote-mock-'));
  fs.cpSync(path.join(build, 'assets'), path.join(root, 'assets'), { recursive: true });
  fs.cpSync(build, path.join(root, 'player'), { recursive: true,
    filter: (src) => !/[\\/](games|assets)$/.test(src) });
  addProject(root, 'demo-srpg', path.join(gamesSrc, 'demo-srpg'), (files) => {
    // A file name that needs URL encoding (space, #, non-ASCII).
    files.push({ path: 'notes/odd name #1 ä.txt', bytes: Buffer.from('hello') });
  });
  addProject(root, 'broken', path.join(gamesSrc, 'demo-srpg'), (files) => {
    const g = files.find((f) => f.path === 'game.json');
    g.bytes = Buffer.from('{ "name": "Broken", this is not json');
  });
  fs.writeFileSync(path.join(root, 'host.html'), `<!doctype html><meta charset="utf-8">
<body style="margin:0;background:#223">
<div style="height:3000px"><iframe id="f" width="640" height="360" style="border:0;display:block"></iframe></div>
<script>
window.msgs = [];
window.addEventListener('message', e => window.msgs.push(e.data));
const q = new URLSearchParams(location.search);
document.getElementById('f').src = '/player/embed.html?' + (q.get('embed') || '');
</script>`);
  return root;
}
