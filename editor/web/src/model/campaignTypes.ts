// data/campaign.json as the editor holds it (docs/EVENTS.md "Campaign"). Every interface carries the index signature of
// `Extra`, so a key the editor does not know survives load -> edit -> save like everywhere else.
import type { Command } from './storyTypes'
import type { Extra } from './types'

export type FallenRule = 'gone' | 'return'
export type DefeatRule = 'retry' | 'end'
/** "none" (the loader's default, written as no key), "all", or the maps that can also be played on their own. */
export type FreeBattles = 'none' | 'all' | string[]

export interface CampaignRules extends Extra { fallen?: FallenRule; onDefeat?: DefeatRule }

export interface Chapter extends Extra {
  id: string
  name?: string
  /** A map id of game.json ("" while none is chosen: the loader refuses it until one is). */
  map: string
  /** Scenes: the scene subset of the story commands (docs/EVENTS.md "Scenes"). `goto` only in `after`. */
  before?: Command[]
  after?: Command[]
  /** Default: the following chapter in the list. */
  next?: string
}

export interface Campaign extends Extra {
  name?: string
  rules?: CampaignRules
  freeBattles?: FreeBattles
  chapters: Chapter[]
}

export type SceneKey = 'before' | 'after'
export const SCENE_KEYS: SceneKey[] = ['before', 'after']

export const chapterName = (c: Chapter): string => (c.name?.trim() ? c.name : c.id)
