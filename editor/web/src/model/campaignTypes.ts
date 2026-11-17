// data/campaign.json as the editor holds it (docs/EVENTS.md "Campaign"). Every interface carries the index signature of
// `Extra`, so a key the editor does not know survives load -> edit -> save like everywhere else.
import type { Scene } from './campaignScene'
import type { Extra } from './types'

export type FallenRule = 'gone' | 'return'
export type DefeatRule = 'retry' | 'end'
/** "none" (the loader's default, written as no key), "all", or the maps that can also be played on their own. */
export type FreeBattles = 'none' | 'all' | string[]

export interface CampaignRules extends Extra { fallen?: FallenRule; onDefeat?: DefeatRule }

/** One party of the campaign (docs/EVENTS.md "Parties"): its own roster, storage and funds under the one story. The first is the starting party. */
export interface Party extends Extra { id: string; name?: string }

export interface Chapter extends Extra {
  id: string
  name?: string
  /** A map id of game.json ("" while none is chosen: the loader refuses it until one is). */
  map: string
  /** Scenes: the scene subset of the story commands (docs/EVENTS.md "Scenes"), as a plain list or `{ music, commands }`
   *  (campaignScene.ts). `goto` only in `after`. */
  before?: Scene
  after?: Scene
  /** Default: the following chapter in the list. */
  next?: string
  /** A party id: that party becomes the active one when the chapter starts (before its scene). Optional. */
  party?: string
}

export interface Campaign extends Extra {
  name?: string
  rules?: CampaignRules
  freeBattles?: FreeBattles
  /** The campaign's parties (docs/EVENTS.md "Parties"); no key = one roster for the whole story. */
  parties?: Party[]
  chapters: Chapter[]
}

export type SceneKey = 'before' | 'after'
export const SCENE_KEYS: SceneKey[] = ['before', 'after']

export const chapterName = (c: Chapter): string => (c.name?.trim() ? c.name : c.id)
