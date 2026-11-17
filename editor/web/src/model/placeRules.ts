// rules.places in game.json (docs/SRPG.md section 10): how doors, chests and places are used. "menu" (the default) = explicit Open / Visit entries in a
// unit's menu; "auto" = it happens when a unit ends a move there. The default is never written, and a block left empty is removed (the game.json of a game
// that does not touch it stays as it was). Unknown keys inside the block are kept.
import { withGame } from './doc'
import type { PlaceActions, ProjectDoc } from './types'

export const placeActions = (doc: ProjectDoc): PlaceActions => (doc.game?.rules?.places?.actions === 'auto' ? 'auto' : 'menu')

export function setPlaceActions(doc: ProjectDoc, actions: PlaceActions): ProjectDoc {
  return withGame(doc, (g) => {
    const block = { ...(g.rules?.places ?? {}) }
    if (actions === 'auto') block.actions = 'auto'
    else delete block.actions
    const rules = { ...(g.rules ?? {}) }
    if (Object.keys(block).length) rules.places = block
    else delete rules.places
    return { ...g, rules }
  })
}
