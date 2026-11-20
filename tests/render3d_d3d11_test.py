#!/usr/bin/env python3
"""The Direct3D 11 3D backend inside the player (Windows only; elsewhere it reports SKIP and ctest shows it skipped).

  python3 tests/render3d_d3d11_test.py ./build/engine/player/ote-player.exe .

Windowless (the backend's own device, the picture read back), compared with the software backend, the reference:
  - feature level capped at 10_1 (what an Xbox One gives UWP apps): the same pictures as at the best level;
  - a simulated device loss (OTE_3D_SIMULATE_LOSS): rebuilt, the same picture; with OTE_3D_SIMULATE_NO_REBUILD the
    software backend takes over (log lines);
  - the display's resolution with 4x multisampling (--3d-scale 2): only edges differ from the software picture.
In a real (hidden) window, where SDL draws the 2D layer with its own Direct3D 11 renderer and the backend draws the 3D
picture straight onto SDL's back buffer: the screenshot (3D picture, then the 2D things drawn over it by SDL, after
many frames) against the windowless one; at twice the size, against the windowless picture of that size; and a device
loss there. A window needs an interactive desktop: run over ssh (session 0) SDL cannot make its Direct3D 11 renderer,
and those checks report SKIP with SDL's reason.
"""
import os
import struct
import subprocess
import sys
from ote_testkit import scratch_dir  # noqa: E402

SKIP = 77  # ctest: SKIP_RETURN_CODE

SCENES = [
    ('test scene', ['--test-3d', '--frames', '30']),
    ('demo-srpg battle with models', ['--game', 'demo-srpg', '--map', 'bridge', '--style', 'isometric-3d',
                                      '--autoplay', '--seed', '3', '--frames', '320']),
    ('demo-srpg, a unit falling (blended)', ['--game', 'demo-srpg', '--map', 'bridge', '--style', 'isometric-3d',
                                             '--autoplay', '--seed', '3', '--frames', '995']),
    ('demo-hex, full 3D, fog', ['--game', 'demo-hex', '--map', 'foggy_hills', '--style', 'full-3d',
                                 '--input', 'wait:14,confirm,wait:10,turnright,turnright,tiltdown,wait:30']),
]
MAX_DIFFERENT = 0.002  # as tests/render3d_backends_test.py
TOLERANCE = 8

results = {'pass': 0, 'fail': 0, 'skip': 0}


def read_bmp(path):
    """(width, height, rows of bytes top first, bytes per pixel) of an uncompressed 24/32-bit BMP."""
    data = open(path, 'rb').read()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    stride = (width * bpp + 3) & ~3
    rows = [data[offset + r * stride: offset + r * stride + width * bpp] for r in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), rows, bpp


def compare(a_path, b_path):
    """(same size, share of pixels with a channel more than TOLERANCE apart, largest channel difference). The two may
    have different pixel formats: SDL's software renderer writes 24-bit screenshots, its Direct3D 11 one 32-bit."""
    w1, h1, a, bpp1 = read_bmp(a_path)
    w2, h2, b, bpp2 = read_bmp(b_path)
    if (w1, h1) != (w2, h2):
        print('  (pictures differ in size: %d x %d and %d x %d)' % (w1, h1, w2, h2))
        return False, 1.0, 255
    different = worst = 0
    for ra, rb in zip(a, b):
        if bpp1 == bpp2 and ra == rb:
            continue
        for x in range(w1):
            i, j = x * bpp1, x * bpp2
            d = max(abs(ra[i + k] - rb[j + k]) for k in range(3))  # B, G, R in both
            worst = max(worst, d)
            if d > TOLERANCE:
                different += 1
    return True, different / float(w1 * h1), worst


def check(name, ok, detail=''):
    print('%s %s%s' % ('PASS' if ok else 'FAIL', name, (': ' + detail) if detail else ''))
    results['pass' if ok else 'fail'] += 1


def skip(name, why):
    print('SKIP %s: %s' % (name, why))
    results['skip'] += 1


def shot(player, repo, mode, args, out, env=None, window=False):
    e = dict(os.environ)
    e.update(env or {})
    cmd = [player, '--3d', mode, '--no-audio', '--no-settings', '--no-saving', '--fixed-dt', '0.0166',
           '--games', os.path.join(repo, 'games'), '--assets', os.path.join(repo, 'assets'), '--screenshot', out]
    if window:
        e['OTE_WINDOW_HIDDEN'] = '1'  # a real window and SDL's real renderer, never shown
    else:
        cmd.append('--headless')
    res = subprocess.run(cmd + args, capture_output=True, text=True, timeout=300, env=e)
    return res.returncode, res.stdout + res.stderr


def windowless(player, repo, tmp):
    for name, args in SCENES:
        soft, best, low = (os.path.join(tmp, n) for n in ('s.bmp', 'h.bmp', 'l.bmp'))
        code_s, log_s = shot(player, repo, 'software', args, soft)
        code_h, log_h = shot(player, repo, 'hardware', args, best)
        code_l, log_l = shot(player, repo, 'hardware', args, low, {'OTE_3D_D3D11_LEVEL': '10_1'})
        if code_s or code_h or code_l:
            check(name + ': the player runs', False, 'exit codes: software %d, best level %d, 10_1 %d\n%s' % (
                code_s, code_h, code_l, (log_s if code_s else log_h if code_h else log_l)[-800:]))
            continue
        if '3D drawing: Direct3D 11' not in log_h:
            skip(name, 'no Direct3D 11 backend here %s' % [l for l in log_h.splitlines() if '3D' in l][:2])
            continue
        check(name + ': feature level 10_1 is used when capped', 'feature level 10_1' in log_l)
        same, share, worst = compare(best, low)
        check(name + ': 10_1 draws the same picture as the best level', same and worst == 0, 'max %d' % worst)
        same, share, worst = compare(soft, best)
        check(name + ': as the software backend', same and share <= MAX_DIFFERENT,
              '%.3f%% of pixels differ, max %d' % (share * 100, worst))

    # Device loss, simulated after 5 frames: rebuilt (the same picture), or refused (the software backend's picture).
    args = SCENES[0][1]
    soft, best, rebuilt, fallback = (os.path.join(tmp, n) for n in ('s.bmp', 'h.bmp', 'r.bmp', 'f.bmp'))
    shot(player, repo, 'software', args, soft)
    shot(player, repo, 'hardware', args, best)
    _, log = shot(player, repo, 'hardware', args, rebuilt, {'OTE_3D_SIMULATE_LOSS': '5'})
    check('device loss: logged and rebuilt', 'losing the 3D device now' in log and 'device rebuilt' in log,
          [l for l in log.splitlines() if 'Direct3D' in l][-2:].__repr__())
    same, share, worst = compare(best, rebuilt)
    check('device loss: the rebuilt backend draws the same picture', same and worst == 0, 'max %d' % worst)
    _, log = shot(player, repo, 'hardware', args, fallback, {'OTE_3D_SIMULATE_LOSS': '5', 'OTE_3D_SIMULATE_NO_REBUILD': '1'})
    check('device loss, no rebuild: the software backend takes over (logged)',
          'using the software 3D renderer for the rest of the session' in log)
    same, share, worst = compare(soft, fallback)
    check('device loss, no rebuild: the software picture', same and worst == 0, 'max %d' % worst)

    # The display's resolution (twice the logical size here) with 4x multisampling: the edges differ, nothing else.
    soft2, best2 = os.path.join(tmp, 's2.bmp'), os.path.join(tmp, 'h2.bmp')
    big = ['--3d-scale', '2'] + args
    shot(player, repo, 'software', big, soft2)
    _, log = shot(player, repo, 'hardware', big, best2)
    same, share, worst = compare(soft2, best2)
    check('4x multisampling at 960 x 540: only edges differ from the software picture', same and 0 < share <= 0.03,
          '%.3f%% of pixels differ, max %d' % (share * 100, worst))


def window(player, repo, tmp):
    probe = os.path.join(tmp, 'w.bmp')
    args = SCENES[0][1] + ['--scale', '1']
    code, log = shot(player, repo, 'hardware', args, probe, window=True)
    if code != 0 or "renderer 'direct3d11'" not in log:
        why = [l.strip() for l in log.splitlines() if 'Direct3D 11 renderer' in l or 'renderer' in l][:2]
        skip('window: SDL Direct3D 11 renderer and the 3D picture on its back buffer', 'not available here %s' % why)
        return
    check('window: the backend shares SDL\'s device', "drawn on SDL's back buffer" in log,
          [l for l in log.splitlines() if '3D drawing' in l].__repr__())
    for name, scene in SCENES:
        if name == 'demo-srpg, a unit falling (blended)':
            continue  # (the same as the battle above, later: a long run on a software device)
        head, win, soft = (os.path.join(tmp, n) for n in ('wh.bmp', 'ww.bmp', 'ws.bmp'))
        shot(player, repo, 'hardware', scene, head)
        code_s, _ = shot(player, repo, 'software', scene + ['--scale', '1'], soft, window=True)
        code, log = shot(player, repo, 'hardware', scene + ['--scale', '1'], win, window=True)
        # The same window with the software 3D backend (its picture goes through an SDL texture, Show3D): only the 3D
        # backend differs, so this is the backends test again, now with everything SDL draws over the picture.
        same, share, worst = compare(soft, win)
        check('window, %s: as the software 3D backend in the same window' % name, code == 0 and code_s == 0 and same and
              share <= MAX_DIFFERENT, '%.3f%% of pixels differ, max %d' % (share * 100, worst))
        # Against the windowless run: the 2D things over the picture (text, panels, cursors) come from SDL's Direct3D
        # 11 renderer instead of its software one, and a window has a mouse pointer somewhere (a hover can light a
        # pixel up): a few values round differently, a few pixels may differ.
        same, share, worst = compare(head, win)
        check('window, %s: as the windowless picture (3D under, 2D over)' % name, same and share <= MAX_DIFFERENT,
              '%.3f%% of pixels differ, max %d' % (share * 100, worst))
    # Twice the size: the picture at the display's resolution, multisampled, on a bigger back buffer.
    head, win = os.path.join(tmp, 'wh2.bmp'), os.path.join(tmp, 'ww2.bmp')
    shot(player, repo, 'hardware', ['--3d-scale', '2'] + SCENES[0][1], head)
    code, log = shot(player, repo, 'hardware', ['--scale', '2', '--3d-res', 'display'] + SCENES[0][1], win, window=True)
    same, share, worst = compare(head, win)
    check('window at 960 x 540, display resolution: as the windowless picture of that size', code == 0 and same and share <= MAX_DIFFERENT,
          '%.3f%% of pixels differ, max %d' % (share * 100, worst))
    # A device loss with SDL's device: rebuilt on it, the same picture.
    rebuilt = os.path.join(tmp, 'wr.bmp')
    code, log = shot(player, repo, 'hardware', args, rebuilt, {'OTE_3D_SIMULATE_LOSS': '5'}, window=True)
    same, share, worst = compare(probe, rebuilt)
    check('window, device loss: rebuilt on SDL\'s device, the same picture', code == 0 and 'device rebuilt' in log and same and worst == 0,
          'max %d' % worst)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    if sys.platform != 'win32':
        print('SKIP: Direct3D 11 exists on Windows only')
        return SKIP
    player, repo = sys.argv[1], sys.argv[2]
    with scratch_dir() as tmp:
        windowless(player, repo, tmp)
        window(player, repo, tmp)
    print('\n%d passed, %d failed, %d skipped' % (results['pass'], results['fail'], results['skip']))
    return 1 if results['fail'] else 0


if __name__ == '__main__':
    sys.exit(main())
