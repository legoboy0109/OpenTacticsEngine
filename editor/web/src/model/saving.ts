// The game's saving setting (game.json "saving", docs/SAVES.md). "any" is the default and is written as NO key.
import { withGame } from './doc'
import { patchObj } from './util'
import type { ProjectDoc, SavingMode } from './types'

export const SAVING_MODES: SavingMode[] = ['any', 'continue', 'off']

/** What the game says; a missing key (or one the engine would refuse) reads as "any". */
export function savingMode(doc: ProjectDoc): SavingMode {
  const v = doc.game?.saving
  return v === 'continue' || v === 'off' ? v : 'any'
}

export function setSavingMode(doc: ProjectDoc, mode: SavingMode): ProjectDoc {
  if (!doc.game || savingMode(doc) === mode) return doc
  return withGame(doc, (g) => patchObj(g, { saving: mode === 'any' ? undefined : mode }))
}
