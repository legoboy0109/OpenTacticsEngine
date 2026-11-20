#!/usr/bin/env python3
"""The hint lines name the controls of the input device in use, through the real player, windowless.

  python3 tests/input_hints_player_test.py ./build/engine/player/ote-player .

The player logs "hint: <text>" whenever the bottom line of a screen changes and "input device: X" whenever the device
changes (docs/PORTING.md "Hints name the device"). Headless scripted input is keyboard unless OTE_INPUT_DEVICE says
otherwise; the script word `device:controller|touch|keyboard` switches the device of the tokens after it.
"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ote_testkit import check, inp, lines, player_and_repo, run_text, summary  # noqa: E402

PLAYER, REPO = player_and_repo()


def run(script, game='demo-srpg', map_id='bridge', env_device=None, extra=()):
    games = os.path.join(REPO, 'games')
    cmd = [PLAYER, '--games', games, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--no-saving', '--frames', '600', '--fixed-dt', '0.016', '--input-interval', '3', '--game', game, '--map', map_id,
           '--input', script] + list(extra)
    env = dict(os.environ)
    env.pop('OTE_INPUT_DEVICE', None)
    if env_device:
        env['OTE_INPUT_DEVICE'] = env_device
    return run_text(cmd, timeout=120, env=env).stdout


# The bridge opens with a short story and a banner; skip them, so the first line is the battle's own.
INTRO = ['wait:40', 'skip', 'wait:60']


def hints(log):
    return [l.split('hint: ', 1)[1] for l in lines(log, 'hint: ')]


def main():
    # The battle's first hint line (nothing selected yet), on each device.
    kb = hints(run(inp(INTRO)))
    pad = hints(run(inp(INTRO, 'device:controller', 'wait:6')))
    touch = hints(run(inp(INTRO, 'device:touch', 'wait:6')))
    check('keyboard names the keys', any('Z:select' in h and 'Arrows:cursor' in h and 'M:menu' in h for h in kb), str(kb[:3]))
    check('controller names the pad', any('A:select' in h and 'D-pad:cursor' in h and 'Start:menu' in h for h in pad), str(pad[:3]))
    check('touch names taps', any(h.startswith('tap:select') or h.strip() == 'tap:select' for h in touch), str(touch[:3]))
    check('no key name on the controller line', not any(' Z:' in h or 'Arrows' in h for h in pad[-1:]), str(pad[-1:]))
    check('no key name on the touch line', not any('Z:' in h or 'Arrows' in h or 'D-pad' in h for h in touch[-1:]), str(touch[-1:]))

    # An action menu line: every device its own words, "Back" for touch collapses.
    menu = inp(INTRO, 'confirm', 'wait:8', 'confirm', 'wait:8', 'confirm', 'wait:30')
    kbm = hints(run(menu))
    padm = hints(run(inp('device:controller', 'wait:1', menu)))
    check('menu hint, keyboard', any('Up/Down + Z:choose' in h for h in kbm), str(kbm))
    check('menu hint, controller', any('D-pad + A:choose' in h for h in padm), str(padm))

    # Switching: a script word moves the device mid-run and the next hint follows it at once.
    log = run(inp(INTRO, 'device:controller', 'wait:6', 'device:touch', 'wait:6', 'device:keyboard', 'wait:6'))
    order = [l.split('input device: ', 1)[1].strip() for l in lines(log, 'input device: ')]
    check('device switches are logged in order', order == ['controller', 'touch', 'keyboard'], str(order))
    hs = hints(log)
    check('the hint line follows the device', any('A:select' in h for h in hs) and any(h.startswith('tap:') for h in hs)
          and hs[-1].startswith('Arrows:'), str(hs))

    # The start value: OTE_INPUT_DEVICE sets it (a headless run is keyboard otherwise), and scripted presses keep it.
    padenv = hints(run(inp(INTRO, 'right', 'wait:6'), env_device='controller'))
    check('OTE_INPUT_DEVICE=controller starts on the controller and stays', bool(padenv) and all('Z:' not in h for h in padenv), str(padenv))
    touchenv = hints(run(inp(INTRO, 'right', 'wait:6'), env_device='touch'))
    check('OTE_INPUT_DEVICE=touch starts on touch', bool(touchenv) and all('Arrows' not in h for h in touchenv), str(touchenv))
    plain = lines(run(inp(INTRO)), 'input device:')
    check('a plain headless run logs no device change', not plain, str(plain))

    # A bad word is refused like any bad token.
    bad = run_text([PLAYER, '--headless', '--no-audio', '--no-settings', '--input', 'device:gamepad'], timeout=60).stdout
    check('an unknown device word is an error', 'bad --input token' in bad, bad[-200:])

    # The launcher's footer too (not only the battle strip).
    home_kb = hints(run_text([PLAYER, '--games', os.path.join(REPO, 'games'), '--assets', os.path.join(REPO, 'assets'), '--headless',
                              '--no-audio', '--no-settings', '--no-saving', '--frames', '40', '--fixed-dt', '0.016'], timeout=60).stdout)
    check('launcher footer, keyboard', any('Up/Down: choose   Confirm: open   or tap' == h for h in home_kb), str(home_kb))
    env = dict(os.environ, OTE_INPUT_DEVICE='controller')
    home_pad = hints(run_text([PLAYER, '--games', os.path.join(REPO, 'games'), '--assets', os.path.join(REPO, 'assets'), '--headless',
                               '--no-audio', '--no-settings', '--no-saving', '--frames', '40', '--fixed-dt', '0.016'], timeout=60, env=env).stdout)
    check('launcher footer, controller', any('D-pad: choose   A: open' == h for h in home_pad), str(home_pad))
    env = dict(os.environ, OTE_INPUT_DEVICE='touch')
    home_touch = hints(run_text([PLAYER, '--games', os.path.join(REPO, 'games'), '--assets', os.path.join(REPO, 'assets'), '--headless',
                                 '--no-audio', '--no-settings', '--no-saving', '--frames', '40', '--fixed-dt', '0.016'], timeout=60, env=env).stdout)
    check('launcher footer, touch', any('tap: open' == h for h in home_touch), str(home_touch))
    summary()
    return 0


if __name__ == '__main__':
    code = main()
    from ote_testkit import failures
    sys.exit(1 if failures else code)
