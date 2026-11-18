// data/audio.json as the editor holds it (docs/SOUND.md "Step 2 as built"). Every interface carries the index signature of
// `Extra`, so a key the editor does not know survives load -> edit -> save like everywhere else.
import type { Extra } from './types'

/** A piece of music. `loop` missing = true (a jingle says `false`); `loopStart` is seconds, missing = 0; `volume` is a percent, missing = 100. */
export interface MusicTrack extends Extra { file: string; loopStart?: number; volume?: number; loop?: boolean }

/** A sound effect: exactly one of `file` (one sound) or `files` (1 to 16 variants, one is picked each time). */
export interface SoundEffect extends Extra { file?: string; files?: string[]; volume?: number }

/** The game's standard set: what plays when. Every key is optional; "" and a missing key both mean "nothing". */
export interface AudioUse extends Extra {
  titleMusic?: string
  mapMusic?: string
  playerTurnMusic?: string
  enemyTurnMusic?: string
  sceneMusic?: string
  victory?: string
  defeat?: string
  levelUp?: string
  prepMusic?: string
  promotion?: string
  chapterCard?: string
  shopMusic?: string
  bootJingle?: string
  /** Sounds played at times (ms) after a promotion starts, next to the promotion jingle. */
  promotionSounds?: Array<{ atMs: number; sound: string }>
  /** Music by situation (docs/SOUND.md): a side id -> track id ("" = none), and the track that plays while few hostile units are left. */
  phaseMusic?: Record<string, string>
  fewEnemies?: { music?: string; count?: number }
  /** Event key (see STANDARD_SOUND_KEYS) -> sound id. */
  sounds?: Record<string, string>
}

export interface Audio extends Extra {
  music?: Record<string, MusicTrack>
  sounds?: Record<string, SoundEffect>
  use?: AudioUse
}

/** One file of the game's audio/ folder, as the helper lists it. */
export interface AudioFile {
  file: string
  size: number
  format: 'wav' | 'ogg' | 'unknown'
  rate?: number
  channels?: number
  seconds?: number
  /** Why the header could not be read (the file is still listed). */
  error?: string
}
