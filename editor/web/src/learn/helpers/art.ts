// Done-test helpers for the Art lesson: pictures the maker gave to terrain and units the template did not already have.
import { cellsOfTerrain } from '../helpers'
import type { ProjectDoc } from '../../model/types'

/** Ids of terrain that are not among `start`. */
export const newTerrainIds = (doc: ProjectDoc, start: string[]): string[] => (doc.terrain ?? []).map((t) => t.id).filter((id) => !start.includes(id))

/** Ids of unit types that are not among `start`. */
export const newUnitIds = (doc: ProjectDoc, start: string[]): string[] => (doc.units ?? []).map((u) => u.id).filter((id) => !start.includes(id))

/** New terrain that has a "joins with neighbours" picture (a look with `join`). */
export const joinedTerrainIds = (doc: ProjectDoc, start: string[]): string[] => newTerrainIds(doc, start).filter((id) => doc.graphics?.terrain?.[id]?.join !== undefined)

/** Squares of the first map painted with a new terrain that has a joining picture. */
export function joinedCells(doc: ProjectDoc, start: string[]): number {
  return joinedTerrainIds(doc, start).reduce((n, id) => n + cellsOfTerrain(doc, id), 0)
}

/** New unit types that have a picture of their own. */
export const pictureUnitIds = (doc: ProjectDoc, start: string[]): string[] => newUnitIds(doc, start).filter((id) => !!doc.graphics?.units?.[id])
