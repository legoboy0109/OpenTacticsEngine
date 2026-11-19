// The keys of data/audio.json that the editor offers, with their plain-language names (placeholder wording; the owner
// decides). The key lists mirror engine/player/sound/cues.cpp; the groups are only how the Sound screen lays them out.
// Kept here, not in a screen, because the model's "used by" sentences and the checks use the same names.

export type MusicSlot = 'titleMusic' | 'mapMusic' | 'playerTurnMusic' | 'enemyTurnMusic' | 'sceneMusic' | 'victory' | 'defeat' | 'levelUp' | 'prepMusic' | 'promotion' | 'chapterCard' | 'shopMusic' | 'bootJingle'

/** In the order the Sound screen lists them. */
export const MUSIC_SLOTS: MusicSlot[] = ['titleMusic', 'bootJingle', 'mapMusic', 'playerTurnMusic', 'enemyTurnMusic', 'sceneMusic', 'shopMusic', 'victory', 'defeat', 'levelUp', 'prepMusic', 'promotion', 'chapterCard']
/** These play once: the loader refuses a track that loops. */
export const JINGLE_SLOTS: MusicSlot[] = ['victory', 'defeat', 'levelUp', 'promotion', 'chapterCard']
/** Every slot whose track must not repeat: the jingles above and the boot jingle (fidelity 3). */
export const isJingleSlot = (slot: MusicSlot): boolean => JINGLE_SLOTS.includes(slot) || slot === 'bootJingle'

export const MUSIC_SLOT_LABEL: Record<MusicSlot, string> = {
  titleMusic: 'Title screen',
  mapMusic: 'Battles',
  playerTurnMusic: 'While it is your turn',
  enemyTurnMusic: 'While the enemy plays',
  sceneMusic: 'Story scenes between battles',
  victory: 'You win',
  defeat: 'You lose',
  levelUp: 'A unit levels up',
  prepMusic: 'Preparation screen',
  promotion: 'A unit is promoted',
  chapterCard: 'A chapter’s title card',
  shopMusic: 'Shop screens',
  bootJingle: 'When the game’s page opens',
}

/** The 24 event keys of `use.sounds`. */
export const STANDARD_SOUND_KEYS = [
  'cursor', 'confirm', 'cancel', 'error', 'step', 'hit', 'miss', 'critical', 'heal', 'defeated', 'capture', 'build',
  'itemUse', 'gold', 'dialogue', 'turnStart', 'upgrade', 'research', 'repair', 'supply', 'ambush', 'select', 'levelUpTick', 'save',
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
  levelUpTick: 'Level-up: each stat that rises',
  save: 'A save was written',
}

export const SOUND_GROUPS: Array<{ title: string; keys: StandardSoundKey[] }> = [
  { title: 'Menus', keys: ['cursor', 'confirm', 'cancel', 'error'] },
  { title: 'Moving and fighting', keys: ['select', 'step', 'ambush', 'turnStart', 'hit', 'miss', 'critical', 'heal', 'defeated'] },
  { title: 'Economy', keys: ['capture', 'build', 'upgrade', 'research', 'repair', 'supply', 'gold', 'itemUse'] },
  { title: 'Story', keys: ['dialogue', 'levelUpTick'] },
  { title: 'Saving', keys: ['save'] },
]

/** Which events a weapon, a unit type and an item may override (the loader refuses any other key). */
export const WEAPON_SOUND_KEYS = ['hit', 'miss', 'critical'] as const
export const UNIT_SOUND_KEYS = ['step', 'hit', 'miss', 'critical', 'defeated', 'select', 'heal'] as const
export const ITEM_SOUND_KEYS = ['use', 'heal', 'healScene'] as const  // heal: instead of the game's heal sound for a staff or healing item; healScene: the same in the battle scene only (docs/SOUND.md)
export type OverrideOwner = 'weapons' | 'units' | 'items'
export const OVERRIDE_KEYS: Record<OverrideOwner, readonly string[]> = { weapons: WEAPON_SOUND_KEYS, units: UNIT_SOUND_KEYS, items: ITEM_SOUND_KEYS }

export const OVERRIDE_KEY_LABEL: Record<string, string> = {
  hit: 'When it hits', miss: 'When it misses', critical: 'On a critical hit',
  step: 'Walking', defeated: 'Being defeated', select: 'Being picked', heal: 'The “+N” when it heals',  // (a unit type's own heal label is UNIT_KEY_LABEL's)
  use: 'When it is used', healScene: 'The “+N” when it heals, in the battle scene only',
}
/** The same keys when the owner is a unit type (they describe what the unit does or has done to it). */
export const UNIT_KEY_LABEL: Record<string, string> = {
  step: 'Walking', hit: 'Its attack hits', miss: 'Its attack misses', critical: 'Its critical hit', defeated: 'Being defeated', select: 'Being picked', heal: 'Being healed',
}

export type MapMusicKey = 'music' | 'playerTurnMusic' | 'enemyTurnMusic' | 'prepMusic'
export const MAP_MUSIC_KEYS: MapMusicKey[] = ['music', 'playerTurnMusic', 'enemyTurnMusic', 'prepMusic']
export const MAP_MUSIC_LABEL: Record<MapMusicKey, string> = { music: 'Music', playerTurnMusic: 'Music on your turn', enemyTurnMusic: 'Music on the enemy’s turn', prepMusic: 'Music on the preparation screen' }
