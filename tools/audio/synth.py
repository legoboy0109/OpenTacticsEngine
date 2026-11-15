"""Oscillators, envelopes and mastering. Everything is lists of floats at RATE Hz, mono.

Deterministic: no clock, no unseeded randomness. Noise is a 15-bit LFSR like the
old sound chips (long mode = hiss, short mode = metallic buzz).
"""
import math

RATE = 22050
_NAMES = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def midi(name):
    """'C4' -> 60, 'F#3', 'Bb2'. A4 = 69 = 440 Hz."""
    n = _NAMES[name[0]]
    i = 1
    while name[i] in '#b':
        n += 1 if name[i] == '#' else -1
        i += 1
    return n + 12 * (int(name[i:]) + 1)


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def nsamp(seconds):
    return max(1, int(round(seconds * RATE)))


def glide(f0, f1, n):
    """Per-sample frequencies sliding exponentially from f0 to f1 (constant if f1 is None)."""
    if f1 is None or f1 == f0 or n < 2:
        return [float(f0)] * n
    r = f1 / f0
    return [f0 * r ** (i / (n - 1.0)) for i in range(n)]


def vibrato(freqs, rate_hz, depth_semi, delay=0.0):
    """Wobbles the pitch; depth ramps in over `delay` seconds so note starts stay clean."""
    out = []
    d = max(delay * RATE, 1.0)
    for i, f in enumerate(freqs):
        ramp = min(1.0, i / d)
        out.append(f * 2 ** (depth_semi * ramp * math.sin(2 * math.pi * rate_hz * i / RATE) / 12.0))
    return out


def osc(kind, freqs, duty=0.5):
    """kind: square | triangle | saw | sine. duty is the square's pulse width."""
    out, ph = [], 0.0
    for f in freqs:
        ph += f / RATE
        ph -= int(ph)
        if kind == 'square':
            out.append(1.0 if ph < duty else -1.0)
        elif kind == 'triangle':
            out.append(4.0 * abs(ph - 0.5) - 1.0)
        elif kind == 'saw':
            out.append(2.0 * ph - 1.0)
        else:
            out.append(math.sin(2 * math.pi * ph))
    return out


def noise(freqs, short=False):
    """LFSR noise; `freqs` is the shift-register clock in Hz (high = bright, low = rumble)."""
    out, acc, state = [], 0.0, 0x4000
    tap = 6 if short else 1
    for f in freqs:
        acc += f / RATE
        while acc >= 1.0:
            acc -= 1.0
            state = (state >> 1) | (((state ^ (state >> tap)) & 1) << 14)
        out.append(1.0 if state & 1 else -1.0)
    return out


def adsr(n, a, d, s, r):
    """Envelope of n samples: attack a s, decay d s down to level s, sustain, then a
    linear release of r s ending exactly at zero on the last sample."""
    an, dn, rn = nsamp(a), nsamp(d), min(nsamp(r), n)
    rel = n - rn

    def level(i):
        if i < an:
            return i / an
        if i < an + dn:
            return 1.0 - (1.0 - s) * (i - an) / dn
        return s

    out = [level(i) for i in range(rel)]
    base = level(rel) if rel > 0 else 1.0
    out += [base * (rn - 1 - j) / max(1, rn - 1) for j in range(rn)]
    return out


def tone(kind, dur, f0, f1=None, duty=0.5, env=(0.003, 0.05, 0.6, 0.03), vib=None, vol=1.0,
         short=False):
    """One note: oscillator x envelope. kind may also be 'noise' (f = LFSR clock).
    vib = (rate_hz, depth_semitones, delay_s)."""
    n = nsamp(dur)
    fr = glide(f0, f1, n)
    if vib:
        fr = vibrato(fr, *vib)
    w = noise(fr, short) if kind == 'noise' else osc(kind, fr, duty)
    e = adsr(n, *env)
    return [x * y * vol for x, y in zip(w, e)]


def decay(buf, tau):
    """Exponential fade (tau seconds to 1/e): percussive tails."""
    k = math.exp(-1.0 / (tau * RATE))
    g, out = 1.0, []
    for x in buf:
        out.append(x * g)
        g *= k
    return out


def silence(sec):
    return [0.0] * nsamp(sec)


def mix(parts):
    """parts: [(offset_seconds, buffer, gain)] summed into one buffer."""
    end = max(int(round(o * RATE)) + len(b) for o, b, _ in parts)
    out = [0.0] * end
    for o, b, g in parts:
        s = int(round(o * RATE))
        for i, x in enumerate(b):
            out[s + i] += x * g
    return out


def arpeggio(kind, midis, step, **kw):
    """Notes one after another, `step` seconds each."""
    out = []
    for m in midis:
        out += tone(kind, step, hz(m), **kw)
    return out


def rms(buf):
    return math.sqrt(sum(x * x for x in buf) / max(1, len(buf)))


def master(buf, target_rms, fade_in=0.003, fade_out=0.010, ceiling=0.92):
    """Scales to a common loudness, soft-limits peaks (tanh knee), then fades the ends
    (fade_*=0 leaves an end alone, for loops)."""
    g = target_rms / (rms(buf) or 1.0)
    out = [ceiling * math.tanh(x * g / ceiling) for x in buf]
    fi = nsamp(fade_in) if fade_in else 0
    fo = nsamp(fade_out) if fade_out else 0
    for i in range(min(fi, len(out))):
        out[i] *= i / fi
    for i in range(min(fo, len(out))):
        out[-1 - i] *= i / fo
    return out
