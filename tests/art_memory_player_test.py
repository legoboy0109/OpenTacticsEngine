#!/usr/bin/env python3
"""Art loads on demand, within a memory budget (docs/GRAPHICS.md "Art memory"), through the real player, windowless.

  python3 tests/art_memory_player_test.py ./build/engine/player/ote-player .

Builds a synthetic game (tests/fixtures/srpg-quotes with the generated battle art, plus 150 extra 1024x1024 sheets from
tools/art/synth_art_game.py, four of them drawn as the unit pictures, the rest unused: 600 MB when decoded) and checks:

  - the game loads and plays a fight scene, and the player's peak memory stays far below what decoding every sheet would take;
  - the picture of the map, of the fight mid-way and of the map after the fight is the same with a huge art budget, the
    default budget and a tiny one (OTE_ART_BUDGET_MB=2);
  - the tiny budget makes the player drop and reload sheets (the "art memory: dropped" line), the others never do;
  - a sheet whose file is damaged after its header logs its error once, not every frame, and the game goes on.
"""
import os
import shutil
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, edit, failures, player_and_repo, read_jsonc, summary, ticks, write_json)
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()
sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art)
import synth_art_game  # noqa: E402

SHEETS = 150
SIZE = 1024
ATTACK = ['wait:14', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm']
INPUT = ','.join(ATTACK + ['wait:120'])  # (the fixture's quotes are removed: the fight starts at once)
INTERVAL = 4


def make_game(gid, sheets=SHEETS):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    os.makedirs(os.path.join(g, 'art'))
    battle_art.main(os.path.join(g, 'art'))
    gfx = {
        'sheets': battle_art.BATTLE_SHEETS,
        'units': {},
        'battle': {'sets': battle_art.BATTLE_SET['sets'], 'assign': [{'set': 'knight', 'type': 'hero'}], 'effects': [],
                   'backgrounds': battle_art.BATTLE_SET['backgrounds'], 'defaultBackground': 'meadow'}}
    write_json(os.path.join(g, 'data', 'graphics.json'), gfx)
    def no_quotes(c):
        for ch in c['characters']:
            ch.pop('quotes', None)
    edit(g, 'data/characters.json', no_quotes)
    # Unit types hero, brute, rider, grunt: pictures from the first four filler sheets (the others are never drawn).
    edit(g, 'data/graphics.json', lambda x: x['units'].update({u: {'sheet': 'filler_%03d' % i, 'cell': [i, 0]} for i, u in enumerate(['hero', 'brute', 'rider', 'grunt'])}))
    synth_art_game.add_filler_sheets(g, sheets, SIZE, used=0)
    return g


def run_with_peak(cmd, env):
    """Runs the player; returns (its output, its peak memory in MB). `resource` is POSIX-only: on Windows the peak
    working set is read from the finished child's handle (Popen keeps it open until the object is dropped)."""
    if os.name != 'nt':
        import resource
        out = run_text(cmd, env=env).stdout
        return out, resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss / 1024.0  # (kilobytes on Linux)
    import ctypes
    from ctypes import wintypes

    class Counters(ctypes.Structure):  # PROCESS_MEMORY_COUNTERS
        _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD), ('PeakWorkingSetSize', ctypes.c_size_t),
                    ('WorkingSetSize', ctypes.c_size_t), ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaPagedPoolUsage', ctypes.c_size_t), ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaNonPagedPoolUsage', ctypes.c_size_t), ('PagefileUsage', ctypes.c_size_t),
                    ('PeakPagefileUsage', ctypes.c_size_t)]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, env=env)
    out, _ = p.communicate(timeout=300)
    c = Counters()
    c.cb = ctypes.sizeof(c)
    get = ctypes.windll.psapi.GetProcessMemoryInfo
    get.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
    ok = get(int(p._handle), ctypes.byref(c), c.cb)  # noqa: SLF001 (the handle is the only way to ask after exit)
    return out, (c.PeakWorkingSetSize / 1048576.0 if ok else 0.0)


def run(gid, frames, shot=None, budget=None, inp=INPUT, peak=False):
    saves = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(saves, ignore_errors=True)
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--saves', saves, '--frames', str(frames), '--input-interval', str(INTERVAL), '--fixed-dt', '0.016',
           '--game', gid, '--map', 'ch1', '--battle-scene', 'scene']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    env = dict(os.environ)
    env.pop('OTE_ART_BUDGET_MB', None)
    if budget:
        env['OTE_ART_BUDGET_MB'] = str(budget)
    if peak:
        return run_with_peak(cmd, env)
    return run_text(cmd, env=env).stdout


def same_file(a, b):
    with open(a, 'rb') as fa, open(b, 'rb') as fb:
        return fa.read() == fb.read()


def main():
    gid = 'art_big'
    make_game(gid)
    n_in = ticks(INPUT) * INTERVAL
    frames_map, frames_fight, frames_after = 30, ticks(','.join(ATTACK)) * INTERVAL + 40, n_in + 90

    # Peak memory first (the kit's children share one maximum, so this run goes before the others): decoding all the
    # sheets of this game at load would hold 150 x 4 MB (about 600 MB) before the first frame.
    log, peak_mb = run(gid, frames_after, peak=True)
    check('the big game loads and plays', 'loaded game' in log and 'battle scene: start' in log and 'battle scene: end' in log, log[-600:])
    check('peak memory stays under 250 MB (eager decoding of every sheet would need over 600 MB): %.0f MB' % peak_mb, peak_mb < 250)
    check('no art errors on the way', '[ERROR]' not in log, log[-400:])
    check('the default budget drops nothing here', 'art memory: dropped' not in log)

    shots = {}
    for label, budget in (('huge', 4096), ('default', None), ('tiny', 2)):
        out = {}
        for when, frames in (('map', frames_map), ('fight', frames_fight), ('after', frames_after)):
            path = os.path.join(TMP, '%s_%s.bmp' % (label, when))
            log = run(gid, frames, shot=path, budget=budget)
            out[when] = path
            out[when + '_log'] = log
        shots[label] = out
    check('the fight is on screen in the middle shot', 'battle scene: start' in shots['huge']['fight_log'] and 'battle scene: end' not in shots['huge']['fight_log'])
    check('the map and the fight differ (the fight shot is not the map)', not same_file(shots['huge']['map'], shots['huge']['fight']))
    for when in ('map', 'fight', 'after'):
        check('picture "%s": default budget = huge budget' % when, same_file(shots['huge'][when], shots['default'][when]))
        check('picture "%s": tiny budget = huge budget (dropped sheets come back as they were)' % when, same_file(shots['huge'][when], shots['tiny'][when]))
    tiny_log = shots['tiny']['after_log']
    check('the tiny budget drops sheets (and says so)', 'art memory: dropped' in tiny_log, tiny_log[-500:])
    check('the huge budget never does', all('art memory: dropped' not in shots['huge'][w + '_log'] for w in ('map', 'fight', 'after')))
    check('the tiny budget run still has no errors', '[ERROR]' not in tiny_log, tiny_log[-400:])
    check('the log line is rare (at most 3 in a short run)', tiny_log.count('art memory: dropped') <= 3, str(tiny_log.count('art memory: dropped')))

    # A sheet damaged after its header: the error is logged once, whatever the number of frames, and the unit keeps its placeholder.
    bad = make_game('art_bad', sheets=6)
    png = os.path.join(bad, 'art', 'filler_000.png')
    with open(png, 'rb') as f:
        head = f.read(64)
    with open(png, 'wb') as f:
        f.write(head)  # the header is fine, the pixel data ends after a few bytes
    log = run('art_bad', 120, inp=None)
    errs = [l for l in log.splitlines() if 'filler_000' in l and '[ERROR]' in l]
    check('a damaged sheet logs its error once, not every frame', len(errs) == 1, '\n'.join(errs[:3]))
    check('the error says the PNG cannot be decoded and what happens', errs and 'cannot be decoded' in errs[0] and 'placeholder' in errs[0], str(errs))
    check('the game goes on drawing', 'loaded game' in log and '[ERROR] art sheet "filler_001"' not in log)

    summary()
    if not failures:
        shutil.rmtree(TMP, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
