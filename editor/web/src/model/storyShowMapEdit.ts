// What the "Show another map" form offers and what a new command starts as (docs/EVENTS.md "Showing another map").
import type { Option } from '../ui/Select'
import type { Command } from './storyTypes'
import type { ProjectDoc } from './types'

/** The maps the game could show from the map `ownId`: every other map of game.json (cut-scene maps first: they exist for this). */
export function showMapOptions(doc: ProjectDoc, ownId: string): Option[] {
  const others = (doc.game?.maps ?? []).filter((m) => m.id !== ownId)
  return [...others.filter((m) => m.cutScene), ...others.filter((m) => !m.cutScene)].map((m) => ({ value: m.id, label: `${m.name || m.id}${m.cutScene ? ' (story scenes only)' : ''}` }))
}

/** A new "Show another map": the first map offered (a cut-scene map when the game has one), the usual camera. */
export const newShowMap = (doc: ProjectDoc, ownId: string): Command => ({ showMap: showMapOptions(doc, ownId)[0]?.value ?? '' })
