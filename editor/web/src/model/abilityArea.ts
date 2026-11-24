// An ability's "area" (loader: data/abilities.json): the ability is aimed at a square in range and acts on every unit in the area
// that its "target" accepts (ally = the user's side, enemy = the other sides, unit = everyone). Only heal, cure, heal + cure and
// inflict-only abilities may have one. With an area the range may start at 0 (0 = the user's own square). Defaults are not written;
// keys the editor does not know stay.
import { patchObj } from './util'
import type { Ability } from './srpgTypes'
import type { Extra } from './types'

export type AreaShape = 'burst' | 'radius' | 'cross' | 'line'
export const AREA_SHAPES: AreaShape[] = ['burst', 'radius', 'cross', 'line']
export const AREA_MAX = 8
export const VERTICAL_MAX = 999
/** Where a new shape starts (burst has no size: it is radius 1). */
const START_SIZE: Record<AreaShape, number> = { burst: 1, radius: 2, cross: 2, line: 3 }

export const hasArea = (a: Ability): boolean => a.area !== undefined
const clamp = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, Math.round(n)))
const obj = (a: Ability): Extra => (a.area && typeof a.area === 'object' && !Array.isArray(a.area) ? a.area : {})

/** vertical undefined = no key = no height limit (the engine's -1); 0 = the same level only, so 0 IS written. */
export interface AreaView { shape: AreaShape | ''; size: number | undefined; vertical: number | undefined }
/** What the form shows; shape '' = a shape the editor does not know (a check says so). */
export function areaOf(a: Ability): AreaView | undefined {
  if (a.area === undefined) return undefined
  const o = obj(a)
  const shape = AREA_SHAPES.includes(o.shape as AreaShape) ? (o.shape as AreaShape) : ''
  const size = shape === 'line' ? o.length : shape === 'radius' || shape === 'cross' ? o.radius : undefined
  return { shape, size: typeof size === 'number' ? size : undefined, vertical: typeof o.vertical === 'number' ? o.vertical : undefined }
}

/** Does the effect allow an area? (heal, cure, heal + cure -- all read as 'heal'/'cure' -- or no effect at all.) Takes the kind to stay free of abilities.ts. */
export const kindAllowsArea = (k: string | null): boolean => k === 'heal' || k === 'cure' || k === 'none'

/** The area key with the shape changed: a size of the old shape that fits the new one is kept (radius <-> cross), vertical and unknown keys stay. */
export function setAreaShape(a: Ability, shape: AreaShape): Ability {
  if (a.area === undefined) return a
  const o = { ...obj(a) }
  const was = areaOf(a)
  const keep = was && (was.shape === 'radius' || was.shape === 'cross') && (shape === 'radius' || shape === 'cross') ? was.size : undefined
  delete o.radius
  delete o.length
  o.shape = shape
  if (shape === 'radius' || shape === 'cross') o.radius = keep ?? START_SIZE[shape]
  if (shape === 'line') o.length = START_SIZE.line
  return patchObj(a, { area: o })
}

/** The radius (radius, cross) or the length (line); a burst has no size. */
export function setAreaSize(a: Ability, n: number): Ability {
  const shape = areaOf(a)?.shape
  const o = { ...obj(a) }
  if (shape === 'radius' || shape === 'cross') o.radius = clamp(n, 1, AREA_MAX)
  else if (shape === 'line') o.length = clamp(n, 1, AREA_MAX)
  else return a
  return patchObj(a, { area: o })
}

/** How many levels up and down the area reaches. undefined (an empty box) removes the key = no limit (the loader's default);
 *  0 means "the same level only" and is written. */
export function setAreaVertical(a: Ability, n: number | undefined): Ability {
  if (a.area === undefined) return a
  const o = { ...obj(a) }
  if (n === undefined) delete o.vertical
  else o.vertical = clamp(n, 0, VERTICAL_MAX)
  return patchObj(a, { area: o })
}
