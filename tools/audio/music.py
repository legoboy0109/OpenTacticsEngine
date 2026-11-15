"""Step sequencer: turns a track spec (see tracks.py, README) into one mono buffer.

Time is a grid of 16 steps per bar (sixteenth notes, 4/4). Channels:
  lead   - notes written out per bar, e.g. "A4 - - - C5 - - - E5 - - - D5 - C5 -"
  arp    - the section's chords played by tone index: 0 root, 1 third, 2 fifth, 3 octave
  bass   - r root, f fifth, o octave-up root
  drums  - one character per step: k kick, s snare, h hat, o open hat, K kick+hat, S snare+hat
Token rules (lead, arp, bass): a note token starts a note; '-' holds it; '.' is a rest.
A note lasts until the next token that is not '-'.

Loops are seamless by construction: the track is a whole number of bars and any note
tail running past the last bar is folded back onto the start (the same overlap the
listener hears when the loop repeats).
"""
from synth import RATE, decay, hz, midi, nsamp, tone

STEPS = 16

ARP = {
    'none': '. . . . . . . . . . . . . . . .',
    'fast': '0 1 2 3 2 1 2 1 0 1 2 3 2 1 2 1',
    'eighth': '0 . 2 . 1 . 2 . 0 . 2 . 1 . 3 .',
    'slow': '0 - - - 2 - - - 1 - - - 2 - - -',
    'fanfare': '0 . 1 . 2 . 3 - - - - - - - - -',
}
BASS = {
    'none': '. . . . . . . . . . . . . . . .',
    'eighth': 'r - r - o - r - r - r - f - r -',
    'drive': 'r r o r r r o r r r o r f r o r',
    'pump': 'r . r r . r . r r . r . f . r .',
    'half': 'r - - - - - - - f - - - - - - -',
    'whole': 'r - - - - - - - - - - - - - - -',
}
DRUMS = {
    'none': '.' * 16,
    'light': 'K.h.S.h.K.h.S.h.',
    'rock': 'K.h.S.hhK.K.S.hh',
    'march': 'K.h.S.h.K.hkS.hs',
    'drive': 'K.hhS.hhK.hhS.hh',
}

DEFAULT_LEAD = dict(kind='square', duty=0.25, gain=0.30, gate=0.9,
                    env=(0.004, 0.08, 0.7, 0.03), vib=(5.5, 0.25, 0.12))
ARP_VOICE = dict(kind='square', duty=0.125, gain=0.13, env=(0.002, 0.05, 0.4, 0.02))
BASS_VOICE = dict(kind='triangle', gain=0.55, env=(0.003, 0.05, 0.85, 0.02))


def chord_info(name):
    """'Am' -> (pitch class 9, [0, 3, 7, 12]); 'G7', 'F#m', 'Bb' also work."""
    root = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}[name[0]]
    i = 1
    while i < len(name) and name[i] in '#b':
        root += 1 if name[i] == '#' else -1
        i += 1
    suffix = name[i:]
    third = 3 if suffix.startswith('m') else 4
    top = 10 if suffix.endswith('7') else 12
    return root % 12, [0, third, 7, top]


def tokens(text, per_bar_check=True):
    """Splits '/'-separated bars of whitespace-separated tokens into a flat token list."""
    out = []
    for bar in text.split('/'):
        t = bar.split()
        if per_bar_check and len(t) != STEPS:
            raise ValueError('bar needs %d tokens, got %d: %r' % (STEPS, len(t), bar.strip()))
        out += t
    return out


def events(toks, to_midi):
    """[(start_step, length_in_steps, midi)] from a token list."""
    evs = []
    for i, t in enumerate(toks):
        if t == '-':
            if not evs or evs[-1][0] + evs[-1][1] != i:
                raise ValueError("'-' with nothing to hold at step %d" % i)
            evs[-1][1] += 1
        elif t != '.':
            evs.append([i, 1, to_midi(i, t)])
    return evs


def section_bars(sec):
    return len(sec['chords'].split())


def render(spec, loop=True):
    """spec: dict(bpm, sections=[...], lead=optional voice overrides, tail=seconds of extra
    release for a jingle). Returns raw floats (master() them afterwards)."""
    step = 60.0 / spec['bpm'] / 4
    lead_v = dict(DEFAULT_LEAD, **spec.get('lead', {}))
    lead_t, arp_t, bass_t, drum_t, chord_by_step = [], [], [], [], []
    for sec in spec['sections']:
        chords = sec['chords'].split()
        for c in chords:
            chord_by_step += [chord_info(c)] * STEPS
        bars = len(chords)
        lead = sec.get('lead')
        lead_t += tokens(lead) if lead else ['.'] * (STEPS * bars)
        if lead and len(tokens(lead)) != STEPS * bars:
            raise ValueError('lead has %d bars, chords %d' % (len(tokens(lead)) // STEPS, bars))
        arp_t += (ARP[sec.get('arp', 'none')].split() * bars)
        bass_t += (BASS[sec.get('bass', 'none')].split() * bars)
        drum_t += list(DRUMS[sec.get('drums', 'none')] * bars)
    total = len(chord_by_step)
    length = round(total * step * RATE)
    start = lambda s: round(s * step * RATE)
    tail = nsamp(spec.get('tail', 0.0)) if not loop else 0
    buf = [0.0] * (length + tail + RATE)  # +1 s scratch for note tails past the end
    cache = {}

    def put(at, key, make, gain):
        if key not in cache:
            cache[key] = make()
        for i, x in enumerate(cache[key]):
            buf[at + i] += x * gain

    # lead: written pitches
    for s, ln, m in events(lead_t, lambda i, t: midi(t)):
        v = lead_v
        dur = ln * step * v['gate'] + v['env'][3]
        put(start(s), ('lead', m, ln),
            lambda: tone(v['kind'], dur, hz(m), duty=v['duty'], env=v['env'], vib=v['vib']),
            v['gain'])

    # arp / bass: chord tones of the chord under each token
    def chord_midi(base, kind):
        def f(i, t):
            pc, tones = chord_info_at(i)
            root = base + (pc - base) % 12
            if kind == 'arp':
                return root + tones[int(t)]
            return root + {'r': 0, 'f': 7, 'o': 12}[t]
        return f

    def chord_info_at(i):
        return chord_by_step[i]

    for s, ln, m in events(arp_t, chord_midi(55, 'arp')):
        v = ARP_VOICE
        put(start(s), ('arp', m, ln),
            lambda: tone(v['kind'], ln * step + v['env'][3], hz(m), duty=v['duty'], env=v['env']),
            v['gain'])
    for s, ln, m in events(bass_t, chord_midi(33, 'bass')):
        v = BASS_VOICE
        put(start(s), ('bass', m, ln),
            lambda: tone(v['kind'], ln * step * 0.95 + v['env'][3], hz(m), env=v['env']),
            v['gain'])

    # drums
    for s, c in enumerate(drum_t):
        if c == '.':
            continue
        if c in 'kK':
            put(start(s), 'k', kick, 0.60)
        if c in 'sS':
            put(start(s), 's', snare, 0.35)
        if c in 'hKS':
            put(start(s), 'h', hat, 0.13)
        if c == 'o':
            put(start(s), 'o', open_hat, 0.13)

    if loop:
        # fold the tail that ran past the last bar back to the start
        for i in range(length, len(buf)):
            buf[i - length] += buf[i]
        return buf[:length]
    return buf[:length + tail]


def kick():
    return [x * 1.0 for x in decay(tone('sine', 0.16, 150, 42, env=(0.001, 0.03, 0.8, 0.04)), 0.06)]


def snare():
    return decay(tone('noise', 0.14, 9000, 4000, env=(0.001, 0.01, 1.0, 0.03)), 0.04)


def hat():
    return decay(tone('noise', 0.045, 14000, env=(0.001, 0.01, 1.0, 0.01), short=True), 0.012)


def open_hat():
    return decay(tone('noise', 0.16, 14000, env=(0.001, 0.01, 1.0, 0.04), short=True), 0.05)
