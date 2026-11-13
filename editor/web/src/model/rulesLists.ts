// Resources (Funds...) and movement types (Foot, Wheels...).
import { withGame } from './doc'
import { makeId } from './ids'
import { cleanMoveType, cleanResource } from './references'
import { moveItem, updateById } from './util'
import type { ProjectDoc } from './types'

export const newResourceId = (doc: ProjectDoc, name: string): string =>
  makeId(name, (doc.game?.resources ?? []).map((r) => r.id), { fallback: 'resource' })

export function addResource(doc: ProjectDoc, name: string, id: string = newResourceId(doc, name)): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, resources: [...(g.resources ?? []), { id, name }] }))
}

export function renameResource(doc: ProjectDoc, id: string, name: string): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, resources: updateById(g.resources ?? [], id, (r) => ({ ...r, name })) }))
}

export function moveResource(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, resources: moveItem(g.resources ?? [], from, to) }))
}

/** Costs, income and starting resources that used it disappear with it. */
export function removeResource(doc: ProjectDoc, id: string): ProjectDoc {
  const cleaned = cleanResource(doc, id)
  return withGame(cleaned, (g) => {
    const rest = (g.resources ?? []).filter((r) => r.id !== id)
    const { resources: _old, ...others } = g
    return (rest.length ? { ...others, resources: rest } : others) as typeof g
  })
}

export const newMoveTypeId = (doc: ProjectDoc, name: string): string =>
  makeId(name, (doc.game?.moveTypes ?? []).map((m) => m.id), { fallback: 'move' })

/** A new type costs 1 on every terrain: that is the engine default, so nothing else is written. */
export function addMoveType(doc: ProjectDoc, name: string, id: string = newMoveTypeId(doc, name)): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, moveTypes: [...g.moveTypes, { id, name }] }))
}

export function renameMoveType(doc: ProjectDoc, id: string, name: string): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, moveTypes: updateById(g.moveTypes, id, (m) => ({ ...m, name })) }))
}

export function moveMoveType(doc: ProjectDoc, from: number, to: number): ProjectDoc {
  return withGame(doc, (g) => ({ ...g, moveTypes: moveItem(g.moveTypes, from, to) }))
}

export function moveTypeRemovalBlock(doc: ProjectDoc): string | null {
  return (doc.game?.moveTypes.length ?? 0) <= 1 ? 'A game needs at least one movement type.' : null
}

/** Units that used it switch to the first remaining type; terrain forgets its cost. */
export function removeMoveType(doc: ProjectDoc, id: string): ProjectDoc {
  if (!doc.game || moveTypeRemovalBlock(doc)) return doc
  const fallback = doc.game.moveTypes.find((m) => m.id !== id)!.id
  const cleaned = cleanMoveType(doc, id, fallback)
  return withGame(cleaned, (g) => ({ ...g, moveTypes: g.moveTypes.filter((m) => m.id !== id) }))
}

/** The type units fall back to when `id` is removed. */
export function fallbackMoveType(doc: ProjectDoc, id: string): string | undefined {
  return doc.game?.moveTypes.find((m) => m.id !== id)?.name
}
