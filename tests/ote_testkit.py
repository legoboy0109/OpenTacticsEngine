"""Shared helpers of the Python tests (tests/*.py). Standard library only.

Scripts import it as a sibling: `python3 tests/x.py` puts the script's own folder first on sys.path, so a plain
`from ote_testkit import ...` works from any working directory, and on Windows. A script another script imports (the
`hp` / `s3` modules) is imported after its importer has put tests/ on sys.path, so the same holds there.

Keep this file to helpers that mean exactly the same in every script that uses them. Where scripts differ (message
format, extra arguments), parameterise here rather than changing what a script prints: the full-check tooling greps
the final "N passed, M failed" line and counts the checks.

The counters are module state of THIS file, shared by every script and module in the process. That is deliberate:
hex_player_test.py's `check` is also used by the scripts that import it as `hp`, and they must all count into the
one total that summary() prints.
"""
import json
import os
import struct
import subprocess
import sys

# ---- checks and the final line ---------------------------------------------------------------------------------
passed = 0
failures = []  # names of the failed checks; scripts read it (`return 1 if failures else 0`), so never rebind it


def check(name, cond, detail=''):
    """Counts one check; prints PASS/FAIL. `detail` is shown only on a failure."""
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def check_dash(name, cond, detail=''):
    """Like check(), but the older "PASS name" / "FAIL name -> detail" layout, and it returns the condition."""
    global passed
    if cond:
        passed += 1
        print('PASS', name)
    else:
        failures.append(name)
        print('FAIL', name, ('-> ' + str(detail)) if detail != '' else '')
    return bool(cond)


def summary(blank_line=False):
    """Prints the line the full-check tooling greps: "N passed, M failed" (a blank line first if the script did)."""
    print(('\n' if blank_line else '') + '%d passed, %d failed' % (passed, len(failures)))


# ---- command line ----------------------------------------------------------------------------------------------
def player_and_repo(absolute=True):
    """(player binary, repo root) from `x.py <player> <repo>`; the repo defaults to '.' and the player to ''."""
    fix = os.path.abspath if absolute else (lambda p: p)
    player = fix(sys.argv[1]) if len(sys.argv) > 1 else ''
    repo = fix(sys.argv[2]) if len(sys.argv) > 2 else '.'
    return player, repo


def shots_dir(absolute=False):
    """The folder after `--shots` (screenshots are kept there for a human to look at), or '' when not given."""
    if '--shots' not in sys.argv:
        return ''
    value = sys.argv[sys.argv.index('--shots') + 1]
    return os.path.abspath(value) if absolute else value


# ---- running programs ------------------------------------------------------------------------------------------
def player_cmd(player, games, repo):
    """The start of a windowless player command: its games folder and the repo's assets, no window or sound device,
    no settings file. Append the test's own arguments. Never drop --headless/--no-audio: tests must stay silent."""
    return [player, '--games', games, '--assets', os.path.join(repo, 'assets'), '--headless', '--no-audio', '--no-settings']


def run_text(cmd, timeout=300, **kw):
    """Runs cmd, stdout and stderr merged into .stdout (text). A hang ends in subprocess.TimeoutExpired."""
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=timeout, **kw)


# ---- JSON ------------------------------------------------------------------------------------------------------
def read_json(path):
    with open(path) as f:
        return json.load(f)


def read_jsonc(path):
    """JSON whose own lines may be `//` comments (the fixtures and the shipped games are written that way)."""
    with open(path) as f:
        return json.loads(''.join(l for l in f if not l.lstrip().startswith('//')))


def write_json(path, obj):
    """Writes obj as JSON (indent 1), making the folder first."""
    folder = os.path.dirname(path)
    if folder:
        os.makedirs(folder, exist_ok=True)
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def edit(g, rel, fn):
    """Loads the JSON file g/rel, lets fn change it in place, writes it back (without the file's `//` comment lines).
    Reads with read_jsonc: some scripts edit commented fixtures; for a strict file the result is the same."""
    path = os.path.join(g, rel)
    obj = read_jsonc(path)
    fn(obj)
    write_json(path, obj)


def save_of(saves, game):
    """The game's Continue save under a --saves folder, or None."""
    path = os.path.join(saves, game, 'continue.json')
    return json.load(open(path)) if os.path.exists(path) else None


# ---- player input scripts and logs -----------------------------------------------------------------------------
def inp(*parts):
    """Joins --input tokens; a part may be one token or a list of them."""
    flat = []
    for p in parts:
        flat.extend(p if isinstance(p, list) else [p])
    return ','.join(flat)


def ticks(script):
    """Input steps of a --input script ("wait:N" counts N, every other token 1): how many steps to run it."""
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def lines(log, key):
    """The log lines containing key."""
    return [l for l in log.splitlines() if key in l]


# ---- screenshots -----------------------------------------------------------------------------------------------
class Bmp:
    """An uncompressed 24/32-bit BMP screenshot (what the player's --screenshot writes) as w, h and px[y][x] = (r, g, b),
    top row first. Scripts subclass it for their own questions about a picture."""

    def __init__(self, path):
        data = open(path, 'rb').read()
        off = struct.unpack_from('<I', data, 10)[0]
        self.w, h = struct.unpack_from('<ii', data, 18)
        bpp = struct.unpack_from('<H', data, 28)[0] // 8
        self.h = abs(h)
        stride = (self.w * bpp + 3) // 4 * 4  # rows are padded to 4 bytes
        self.px = []
        for y in range(self.h):
            row = (self.h - 1 - y) if h > 0 else y  # a positive height means the rows are stored bottom first
            base = off + row * stride
            self.px.append([(data[base + x * bpp + 2], data[base + x * bpp + 1], data[base + x * bpp]) for x in range(self.w)])

    def count(self, rgb, tol=12, box=None):
        """Pixels within `tol` per channel of rgb, in the box (x0, y0, x1, y1) or the whole picture."""
        x0, y0, x1, y1 = box or (0, 0, self.w, self.h)
        n = 0
        for y in range(y0, min(y1, self.h)):
            for x in range(x0, min(x1, self.w)):
                p = self.px[y][x]
                if abs(p[0] - rgb[0]) <= tol and abs(p[1] - rgb[1]) <= tol and abs(p[2] - rgb[2]) <= tol:
                    n += 1
        return n
