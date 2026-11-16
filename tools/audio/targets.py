"""Where the retro starter set ships, and what each place gets (docs/SOUND.md "Starter audio").

Every shipped copy is self-contained (a project made from a template must not depend on
anything outside its folder), so each target lists exactly the files its data/audio.json
refers to: no unused files, and a copy is only as big as it needs to be.

`extras` are effect ids beyond the 20 standard ones, used by the per-weapon / per-unit
overrides in that game's data files (written by hand there, see docs/SOUND.md "Overrides");
`use_extra` maps further standard cues (`ambush`) to a sound; `scene` adds the scene track
for games with a campaign (none of these has one yet: the demo with a campaign, demo-srpg,
gets the CC0 set instead).
"""

STANDARD_KEYS = ['cursor', 'confirm', 'cancel', 'error', 'step', 'hit', 'miss', 'critical',
                 'heal', 'defeated', 'capture', 'build', 'itemUse', 'gold', 'dialogue',
                 'turnStart', 'upgrade', 'research', 'repair', 'supply']
# Cue key -> file stem / sound id where they differ: file names are lower case (the editor refuses others).
STEMS = {'itemUse': 'item_use', 'turnStart': 'turn_start'}
EXTRA_SOUNDS = ['sword_hit', 'bow_shot', 'magic_cast', 'cannon_shot', 'hooves', 'tank_move',
                'potion', 'ambush', 'levelup_tick']
BASE_MUSIC = ['title', 'battle', 'enemy', 'victory', 'defeat', 'levelup']

# path below the repository root -> profile
TARGETS = {
    # tanks and artillery: cannon shots, tread rumble for tread units' steps; fog maps use ambush
    'games/demo-economy': dict(extras=['cannon_shot', 'tank_move'], use_extra={'ambush': 'ambush'}),
    # A blank project has one foot unit and one sword: the standard set only.
    'editor/templates/blank': dict(extras=[]),
    'editor/templates/economy-tactics': dict(extras=['cannon_shot', 'tank_move']),
    # the hexagon games: the same tanks and artillery; the demo's second map is a fog map
    'games/demo-hex': dict(extras=['cannon_shot', 'tank_move'], use_extra={'ambush': 'ambush'}),
    'editor/templates/hex-tactics-pointy': dict(extras=['cannon_shot', 'tank_move']),
    'editor/templates/hex-tactics-flat': dict(extras=['cannon_shot', 'tank_move']),
    'editor/templates/strategy-rpg': dict(extras=['sword_hit', 'bow_shot', 'hooves']),
}

# The player's own menu sounds, for the screens where no game is open (the launcher's home list and the global
# options; docs/SOUND.md "The launcher's own sounds"). Same layout as a game: data/audio.json + audio/.
LAUNCHER_DIR = 'assets/launcher'
LAUNCHER_SOUNDS = ['cursor', 'confirm', 'cancel', 'error']


def launcher_json():
    return {'music': {}, 'sounds': {s: {'file': 'audio/%s.wav' % s} for s in LAUNCHER_SOUNDS},
            'use': {'sounds': {s: s for s in LAUNCHER_SOUNDS}}}


def profile_sounds(profile):
    """Effect file stems a target ships (the standard `step` has two variants)."""
    out = []
    for k in STANDARD_KEYS:
        out += ['step1', 'step2'] if k == 'step' else [STEMS.get(k, k)]
    out += profile['extras']
    out += [s for s in profile.get('use_extra', {}).values() if s not in out]
    return out


def profile_music(profile):
    return BASE_MUSIC + (['scene'] if profile.get('scene') else [])


def audio_json(music_tracks, loops, sound_stems, use_extra=None, ext='.ogg', scene=False):
    """A game's data/audio.json. `music_tracks` in order, `loops` name -> bool."""
    music = {}
    for name in music_tracks:
        entry = {'file': 'audio/%s%s' % (name, ext)}
        if loops[name]:
            entry['loopStart'] = 0
        else:
            entry['loop'] = False
        music[name] = entry
    music['battle']['volume'] = 80
    sounds = {}
    for stem in sound_stems:
        if stem in ('step1', 'step2'):
            sounds['step'] = {'files': ['audio/step1.wav', 'audio/step2.wav']}
        else:
            sounds[stem] = {'file': 'audio/%s.wav' % stem}
    use_sounds = {k: STEMS.get(k, k) for k in STANDARD_KEYS}
    use_sounds.update(use_extra or {})
    return {
        'music': music,
        'sounds': sounds,
        'use': {
            'titleMusic': 'title', 'mapMusic': 'battle',
            'playerTurnMusic': '', 'enemyTurnMusic': 'enemy',
            'sceneMusic': 'scene' if scene else '',
            'victory': 'victory', 'defeat': 'defeat', 'levelUp': 'levelup',
            'sounds': use_sounds,
        },
    }
