"""Sound effect recipes. Each function returns a raw float buffer; build() masters it.

To add one: write a function below, add it to EFFECTS (id -> function). Keep it
0.05..1.2 s. Files named in FILES when one id needs several variants.
"""
from synth import (RATE, arpeggio, decay, hz, master, midi, mix, noise, glide, silence, tone)

TARGET_RMS = 0.16  # every effect is scaled to this loudness before soft limiting


def n(name):
    return hz(midi(name))


def burst(dur, clock0, clock1=None, tau=0.05, vol=1.0, short=False):
    """Percussive noise: LFSR clock sliding clock0 -> clock1, exponential tail."""
    return [x * vol for x in decay(tone('noise', dur, clock0, clock1, env=(0.001, 0.01, 1.0, 0.01),
                                        short=short), tau)]


def cursor():
    return tone('square', 0.06, 1320, duty=0.25, env=(0.002, 0.01, 0.7, 0.02))


def confirm():
    return tone('square', 0.07, n('E5'), duty=0.5) + tone('square', 0.14, n('A5'), duty=0.5,
                                                          env=(0.002, 0.04, 0.5, 0.06))


def cancel():
    return tone('square', 0.07, n('A4'), duty=0.5) + tone('square', 0.14, n('D4'), duty=0.5,
                                                          env=(0.002, 0.04, 0.5, 0.06))


def error():
    one = tone('square', 0.09, 140, duty=0.5, env=(0.002, 0.02, 0.9, 0.02), vib=(30, 1.2, 0))
    return one + silence(0.03) + one


def step1():
    return mix([(0, burst(0.07, 2600, 900, 0.018), 0.8),
                (0, tone('sine', 0.07, 150, 80, env=(0.001, 0.02, 0.3, 0.03)), 0.7)])


def step2():
    return mix([(0, burst(0.07, 3400, 1200, 0.016), 0.8),
                (0, tone('sine', 0.07, 180, 95, env=(0.001, 0.02, 0.3, 0.03)), 0.7)])


def hit():
    return mix([(0, burst(0.16, 7000, 1500, 0.04), 1.0),
                (0, tone('triangle', 0.16, 220, 60, env=(0.001, 0.05, 0.3, 0.05)), 0.9)])


def miss():
    return tone('noise', 0.22, 9000, 2500, env=(0.08, 0.05, 0.5, 0.1), vol=0.8)


def critical():
    return mix([(0, hit(), 1.0),
                (0.03, tone('square', 0.3, 1100, 2600, duty=0.25, env=(0.002, 0.08, 0.4, 0.15)), 0.35),
                (0.06, burst(0.28, 5000, 800, 0.09), 0.6)])


def heal():
    return arpeggio('triangle', [midi('C5'), midi('E5'), midi('G5'), midi('C6')], 0.08,
                    env=(0.004, 0.04, 0.7, 0.04)) + tone(
        'triangle', 0.2, n('E6'), env=(0.004, 0.1, 0.4, 0.12), vib=(7, 0.3, 0.05))


def defeated():
    return mix([(0, tone('square', 0.5, 440, 70, duty=0.25, env=(0.003, 0.1, 0.7, 0.2),
                         vib=(9, 0.8, 0.05)), 0.8),
                (0.2, burst(0.3, 2500, 400, 0.1), 0.5)])


def capture():
    return arpeggio('square', [midi('G4'), midi('C5'), midi('E5')], 0.08, duty=0.25) + tone(
        'square', 0.3, n('G5'), duty=0.25, env=(0.003, 0.1, 0.6, 0.2), vib=(6, 0.25, 0.1))


def build():
    out = []
    for i, f in enumerate([700, 700, 900]):
        hammer = mix([(0, burst(0.05, 5000, 2000, 0.012), 0.9),
                      (0, tone('square', 0.12, f, duty=0.125, env=(0.001, 0.04, 0.2, 0.06)), 0.35)])
        out += hammer + silence(0.07 if i < 2 else 0)
    return out


def itemUse():
    return tone('triangle', 0.14, 500, 1300, env=(0.003, 0.05, 0.7, 0.05)) + tone(
        'square', 0.12, n('E6'), duty=0.125, env=(0.002, 0.04, 0.4, 0.06), vol=0.5)


def gold():
    return tone('square', 0.06, n('B5'), duty=0.25) + tone(
        'square', 0.28, n('E6'), duty=0.25, env=(0.002, 0.1, 0.4, 0.15))


def dialogue():
    return tone('square', 0.06, 560, 600, duty=0.5, env=(0.003, 0.02, 0.6, 0.02), vol=0.8)


def turnStart():
    return arpeggio('triangle', [midi('C5'), midi('E5'), midi('G5')], 0.08,
                    env=(0.004, 0.06, 0.6, 0.04)) + tone(
        'triangle', 0.3, n('C6'), env=(0.004, 0.15, 0.3, 0.12))


def upgrade():
    return arpeggio('square', [midi(x) for x in ('C5', 'E5', 'G5', 'C6')], 0.06, duty=0.25) + tone(
        'square', 0.3, n('E6'), duty=0.25, env=(0.003, 0.1, 0.6, 0.2), vib=(7, 0.3, 0.08))


def research():
    return arpeggio('sine', [midi(x) for x in ('E5', 'B5', 'E5', 'B5', 'G5', 'D6', 'G5', 'D6')],
                    0.05, env=(0.004, 0.03, 0.6, 0.015))


def repair():
    out = []
    for i, f in enumerate([2200, 2600, 3000]):
        out += mix([(0, burst(0.05, f, f / 2, 0.012), 0.8),
                    (0, tone('triangle', 0.1, 300 + 110 * i, env=(0.002, 0.04, 0.4, 0.05)), 0.6)]) + silence(0.03)
    return out + tone('triangle', 0.12, n('C6'), env=(0.003, 0.05, 0.5, 0.08))


def supply():
    return mix([(0, tone('square', 0.07, 900, duty=0.25), 0.7),
                (0.09, tone('square', 0.07, 700, duty=0.25), 0.7),
                (0.18, tone('triangle', 0.14, 220, 160, env=(0.002, 0.05, 0.5, 0.06)), 1.0)])


def sword_hit():
    return mix([(0, burst(0.25, 9000, 3000, 0.05), 1.0),
                (0, tone('square', 0.3, 2200, 2150, duty=0.125, env=(0.001, 0.2, 0.0, 0.08)), 0.3),
                (0, tone('square', 0.3, 3300, 3250, duty=0.125, env=(0.001, 0.15, 0.0, 0.1)), 0.2)])


def bow_shot():
    return mix([(0, tone('saw', 0.15, 700, 180, env=(0.001, 0.06, 0.3, 0.07), vib=(40, 1.5, 0)), 0.7),
                (0.03, tone('noise', 0.25, 9000, 4000, env=(0.05, 0.05, 0.5, 0.12)), 0.5)])


def magic_cast():
    shimmer = arpeggio('sine', [midi(x) for x in ('E6', 'G6', 'B6', 'E7', 'B6', 'G6')], 0.045,
                       env=(0.003, 0.02, 0.6, 0.015))
    return mix([(0, tone('sine', 0.7, 300, 1500, env=(0.1, 0.3, 0.6, 0.3), vib=(8, 0.6, 0.1)), 0.8),
                (0.25, shimmer, 0.3)])


def cannon_shot():
    return mix([(0, decay(tone('noise', 0.6, 4500, 300, env=(0.001, 0.02, 1.0, 0.05)), 0.18), 1.0),
                (0, tone('sine', 0.5, 120, 35, env=(0.001, 0.1, 0.5, 0.2)), 1.2)])


def hooves():
    out = []
    for i in range(2):   # one step of a mounted unit: two clops (the walk plays one per square)
        out += mix([(0, burst(0.06, 2000, 700, 0.015), 0.8),
                    (0, tone('triangle', 0.06, 260 - 50 * (i % 2), 150, env=(0.001, 0.02, 0.3, 0.03)), 0.8)])
        out += silence(0.06 if i % 2 == 0 else 0.1)
    return out


def tank_move():
    # One square of a tread unit's walk (a step sound plays once per square, about every 0.1 s):
    # short, so a long move is a steady rumble and not a pile of overlapping tails.
    rumble = tone('noise', 0.22, 140, 120, env=(0.02, 0.04, 0.9, 0.06), short=True)
    drone = tone('saw', 0.22, 48, env=(0.02, 0.04, 0.8, 0.06), vib=(14, 0.8, 0))
    return mix([(0, rumble, 0.5), (0, drone, 0.5)])


def potion():
    out = []
    for f in (400, 520, 460, 640, 580):
        out += tone('sine', 0.07, f, f * 1.6, env=(0.004, 0.03, 0.5, 0.03)) + silence(0.01)
    return out


def ambush():
    return mix([(0, tone('saw', 0.4, n('A2'), env=(0.003, 0.15, 0.4, 0.25)), 0.7),
                (0, tone('saw', 0.4, n('Bb2'), env=(0.003, 0.15, 0.4, 0.25)), 0.7),
                (0, burst(0.3, 3000, 600, 0.08), 0.6),
                (0.1, tone('square', 0.3, n('E5'), n('Eb5'), duty=0.125, env=(0.003, 0.1, 0.3, 0.2)), 0.3)])


def levelup_tick():
    return tone('square', 0.08, n('C6'), n('G6'), duty=0.25, env=(0.002, 0.03, 0.5, 0.03))


EFFECTS = {
    'cursor': cursor, 'confirm': confirm, 'cancel': cancel, 'error': error,
    'step1': step1, 'step2': step2, 'hit': hit, 'miss': miss, 'critical': critical,
    'heal': heal, 'defeated': defeated, 'capture': capture, 'build': build,
    'itemUse': itemUse, 'gold': gold, 'dialogue': dialogue, 'turnStart': turnStart,
    'upgrade': upgrade, 'research': research, 'repair': repair, 'supply': supply,
    'sword_hit': sword_hit, 'bow_shot': bow_shot, 'magic_cast': magic_cast,
    'cannon_shot': cannon_shot, 'hooves': hooves, 'tank_move': tank_move,
    'potion': potion, 'ambush': ambush, 'levelup_tick': levelup_tick,
}


TAIL_FLOOR = 0.004   # about -48 dB: a tail quieter than this is inaudible next to the game's own noise
TAIL_KEEP = 0.012    # seconds kept after the last audible sample


def trim_tail(buf, fade=0.010):
    """Cuts silence and inaudible decay off the end (the files are stored as 16-bit PCM, so every
    10 ms of tail costs 441 bytes), then fades the new end so it does not click."""
    last = len(buf) - 1
    while last > 0 and abs(buf[last]) < TAIL_FLOOR:
        last -= 1
    out = buf[:min(len(buf), last + 1 + int(TAIL_KEEP * RATE))]
    n = min(int(fade * RATE), len(out))
    for i in range(n):
        out[-1 - i] *= i / n
    return out


def build_effect(name):
    return trim_tail(master(EFFECTS[name](), TARGET_RMS))
