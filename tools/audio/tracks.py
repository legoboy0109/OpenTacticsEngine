"""The starter tracks, written as data for music.render(). Original compositions.

Each section is 'chords' (one name per bar) plus a '/'-separated lead (16 tokens per
bar) and the names of the arp / bass / drum patterns (see music.py). Sections play in
order; reusing a section object repeats it. LOOPS end on a bar line and loop to bar 1.
"""

# --- title: A minor, 96 bpm, 12 bars = 30 s -----------------------------------
_T_A = dict(chords='Am F C G', arp='eighth', bass='half', drums='light', lead="""
    A4 - - - C5 - - - E5 - - - D5 - C5 - /
    C5 - - - A4 - - - F5 - - - E5 - D5 - /
    E5 - - - G5 - - - E5 - - - C5 - - - /
    D5 - - - B4 - - - G4 - - - B4 - D5 -""")
_T_B = dict(chords='F G Am Em', arp='fast', bass='eighth', drums='rock', lead="""
    F5 - A5 - C6 - A5 - F5 - - - C5 - - - /
    G5 - B5 - D6 - B5 - G5 - - - D5 - - - /
    A5 - - G5 E5 - - - C5 - E5 - A5 - - - /
    B4 - - - E5 - G5 - B5 - - - G5 - E5 -""")
_T_A2 = dict(_T_A, arp='fast', bass='eighth', drums='rock')
TITLE = dict(bpm=96, sections=[_T_A, _T_B, _T_A2])

# --- battle: E minor, 120 bpm, 20 bars = 40 s ---------------------------------
_B_A = dict(chords='Em C G D', arp='fast', bass='pump', drums='march', lead="""
    E5 - E5 G5 - B5 - A5 G5 - E5 - D5 - - - /
    C5 - C5 E5 - G5 - E5 C5 - E5 - G5 - - - /
    B4 - D5 G5 - F#5 - D5 B4 - D5 - G5 - - - /
    A4 - D5 F#5 - A5 - F#5 E5 - D5 - F#5 - - -""")
_B_B = dict(chords='C D Em B', arp='eighth', bass='pump', drums='march', lead="""
    G5 - G5 - E5 - C5 - E5 - G5 - C6 - - - /
    A5 - A5 - F#5 - D5 - F#5 - A5 - D6 - - - /
    B5 - G5 - E5 - G5 - B5 - E6 - D6 - B5 - /
    D#5 - F#5 - B5 - A5 - F#5 - D#5 - B4 - - -""")
_B_C = dict(chords='Am Bm C D', arp='slow', bass='half', drums='light', lead="""
    E5 - - - - - - - A5 - - - G5 - - - /
    F#5 - - - - - D5 - B4 - - - D5 - F#5 - /
    G5 - - - E5 - - - C5 - - - E5 - G5 - /
    A5 - F#5 - D5 - F#5 - A5 - - - - - - -""")
BATTLE = dict(bpm=120, sections=[_B_A, _B_B, _B_A, _B_C, _B_B])

# --- enemy turn: D minor, 128 bpm, 16 bars = 30 s, tenser ---------------------
_E_A = dict(chords='Dm Dm Bb A', arp='fast', bass='drive', drums='drive', lead="""
    D5 - D5 - F5 - D5 - A5 - - - G5 - F5 - /
    D5 - D5 - F5 - D5 - C6 - - - A5 - G5 - /
    Bb4 - Bb4 - D5 - Bb4 - F5 - - - E5 - D5 - /
    A4 - C#5 - E5 - A5 - G5 - E5 - C#5 - A4 -""")
_E_B = dict(chords='Gm C F A', arp='fast', bass='drive', drums='drive', lead="""
    G5 - - G5 Bb5 - - G5 D6 - - Bb5 - G5 - - /
    C6 - - C6 E6 - - C6 G6 - - E6 - C6 - - /
    A5 - - A5 C6 - - A5 F6 - - C6 - A5 - - /
    E5 - G5 - A5 - C#6 - E6 - - - D6 - C#6 -""")
ENEMY = dict(bpm=128, lead=dict(duty=0.125, gain=0.27, vib=(6.5, 0.35, 0.1)),
             sections=[_E_A, _E_B, _E_A, _E_B])

# --- scene: C major, 80 bpm, 10 bars = 30 s, calm -----------------------------
_S_A = dict(chords='C Am F G', arp='slow', bass='whole', lead="""
    E5 - - - - - - - G5 - - - E5 - - - /
    E5 - - - - - - - C5 - - - A4 - - - /
    A4 - - - C5 - - - F5 - - - E5 - - - /
    D5 - - - - - - - B4 - - - . . . .""")
_S_B = dict(chords='F G Em Am', arp='slow', bass='whole', lead="""
    C5 - - - A4 - - - F4 - - - A4 - - - /
    D5 - - - B4 - - - G4 - - - B4 - - - /
    B4 - - - G4 - - - E5 - - - G5 - - - /
    A5 - - - - - - - E5 - - - - - - -""")
_S_C = dict(chords='F G', arp='slow', bass='whole', lead="""
    A5 - - - G5 - - - F5 - - - E5 - - - /
    D5 - - - - - - - - - - - - - - -""")
SCENE = dict(bpm=80, lead=dict(kind='triangle', gain=0.45, vib=(5, 0.3, 0.2), env=(0.02, 0.1, 0.8, 0.12)),
             sections=[_S_A, _S_B, _S_C])

# --- jingles (not looped) -----------------------------------------------------
VICTORY = dict(bpm=144, tail=0.3, sections=[dict(chords='C F C', arp='fanfare', bass='half', lead="""
    G4 . G4 . G4 - C5 - E5 - - - G5 - - - /
    F5 - . . E5 - . . F5 - A5 - C6 - - - /
    G5 - - - - - - - C6 - - - - - - -""")])
DEFEAT = dict(bpm=96, tail=0.5, lead=dict(duty=0.5, gain=0.3, vib=(4.5, 0.4, 0.3), env=(0.01, 0.1, 0.7, 0.2)),
              sections=[dict(chords='Am Am', arp='slow', bass='whole', lead="""
    E5 - - - D5 - - - C5 - - - B4 - - - /
    A4 - - - - - - - - - - - - - - -""")])
LEVELUP = dict(bpm=120, tail=0.3, sections=[dict(chords='C', arp='none', bass='half', lead="""
    C5 - E5 - G5 - C6 - . . G5 C6 E6 - - -""")])

# id -> (spec, loops, target rms)
TRACKS = {
    'title': (TITLE, True), 'battle': (BATTLE, True), 'enemy': (ENEMY, True),
    'scene': (SCENE, True), 'victory': (VICTORY, False), 'defeat': (DEFEAT, False),
    'levelup': (LEVELUP, False),
}
