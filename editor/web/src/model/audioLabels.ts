// The keys of data/audio.json that the editor offers, with their plain-language names (placeholder wording; the owner
// decides). The key lists mirror engine/player/sound/cues.cpp; the groups are only how the Sound screen lays them out.
// Kept here, not in a screen, because the model's "used by" sentences and the checks use the same names.

export type MusicSlot = 'titleMusic' | 'mapMusic' | 'playerTurnMusic' | 'enemyTurnMusic' | 'sceneMusic' | 'victory' | 'defeat' | 'levelUp'

/** In the order the Sound screen lists them. */
export const MUSIC_SLOTS: MusicSlot[] = ['titleMusic', 'mapMusic', 'playerTurnMusic', 'enemyTurnMusic', 'sceneMusic', 'victory', 'defeat', 'levelUp']
/** These three play once: the loader refuses a track that loops. */
export const JINGLE_SLOTS: MusicSlot[] = ['victory', 'defeat', 'levelUp']

export const MUSIC_SLOT_LABEL: Record<MusicSlot, string> = {
  titleMusic: 'Title screen',
  mapMusic: 'Battles',
  playerTurnMusic: 'While it is your turn',
  enemyTurnMusic: 'While the enemy plays',
  sceneMusic: 'Story scenes between battles',
  victory: 'You win',
  defeat: 'You lose',
  levelUp: 'A unit levels up',
}

/** The 22 event keys of `use.sounds`. */
export const STANDARD_SOUND_KEYS = [
  'cursor', 'confirm', 'cancel', 'error', 'step', 'hit', 'miss', 'critical', 'heal', 'defeated', 'capture', 'build',
  'itemUse', 'gold', 'dialogue', 'turnStart', 'upgrade', 'research', 'repair', 'supply', 'ambush', 'select',
] as const
export type StandardSoundKey = typeof STANDARD_SOUND_KEYS[number]

export const SOUND_KEY_LABEL: Record<StandardSoundKey, string> = {
  cursor: 'The cursor moves',
  confirm: 'Choosing something',
  cancel: 'Going back',
  error: 'Something is not allowed',
  select: 'A unit is picked on the map',
  step: 'A unit takes a step',
  ambush: 'A move is stopped by an enemy nobody saw',
  turnStart: 'A turn begins',
  hit: 'An attack hits',
  miss: 'An attack misses',
  critical: 'A critical hit',
  heal: 'A unit is healed',
  defeated: 'A unit is defeated',
  capture: 'A structure is captured',
  build: 'A unit is built',
  upgrade: 'A structure is upgraded',
  research: 'Research starts or finishes',
  repair: 'A unit is repaired',
  supply: 'A unit is resupplied',
  gold: 'Money comes in',
  itemUse: 'An item is used',
  dialogue: 'Text appears in a conversation',
}

export const SOUND_GROUPS: Array<{ title: string; keys: StandardSoundKey[] }> = [
  { title: 'Menus', keys: ['cursor', 'confirm', 'cancel', 'error'] },
  { title: 'Moving and fighting', keys: ['select', 'step', 'ambush', 'turnStart', 'hit', 'miss', 'critical', 'heal', 'defeated'] },
  { title: 'Economy', keys: ['capture', 'build', 'upgrade', 'research', 'repair', 'supply', 'gold', 'itemUse'] },
  { title: 'Story', keys: ['dialogue'] },
]

/** Which events a weapon, a unit type and an item may override (the loader refuses any other key). */
export const WEAPON_SOUND_KEYS = ['hit', 'miss', 'critical'] as const
export const UNIT_SOUND_KEYS = ['step', 'hit', 'miss', 'critical', 'defeated', 'select', 'heal'] as const
export const ITEM_SOUND_KEYS = ['use'] as const
export type OverrideOwner = 'weapons' | 'units' | 'items'
export const OVERRIDE_KEYS: Record<OverrideOwner, readonly string[]> = { weapons: WEAPON_SOUND_KEYS, units: UNIT_SOUND_KEYS, items: ITEM_SOUND_KEYS }

export const OVERRIDE_KEY_LABEL: Record<string, string> = {
  hit: 'When it hits', miss: 'When it misses', critical: 'On a critical hit',
  step: 'Walking', defeated: 'Being defeated', select: 'Being picked', heal: 'Being healed',
  use: 'When it is used',
}
/** The same keys when the owner is a unit type (they describe what the unit does or has done to it). */
export const UNIT_KEY_LABEL: Record<string, string> = {
  step: 'Walking', hit: 'Its attack hits', miss: 'Its attack misses', critical: 'Its critical hit', defeated: 'Being defeated', select: 'Being picked', heal: 'Being healed',
}

export type MapMusicKey = 'music' | 'playerTurnMusic' | 'enemyTurnMusic'
export const MAP_MUSIC_KEYS: MapMusicKey[] = ['music', 'playerTurnMusic', 'enemyTurnMusic']
export const MAP_MUSIC_LABEL: Record<MapMusicKey, string> = { music: 'Music', playerTurnMusic: 'Music on your turn', enemyTurnMusic: 'Music on the enemy’s turn' }
