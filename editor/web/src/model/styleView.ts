// A visual style's kind of view (docs/VIEWS.md) and the sizes of the isometric one. Mirrors engine/core/data/loader.cpp
// (`loadStyles`), which stays the source of truth for the ranges.
import { patchObj } from './util'
import type { Game, IsoSizes, Style } from './types'

/** 'future' = "3d" (full 3D): allowed in a game file, drawn by no player yet, so the editor keeps it untouched. */
export type ViewKind = 'top-down' | 'isometric' | 'isometric-3d' | 'future'

/** The kinds a style can be switched between here; both isometric ones share the same sizes. */
export type EditableKind = Exclude<ViewKind, 'future'>
export const isIsometric = (k: ViewKind): boolean => k === 'isometric' || k === 'isometric-3d'

export const viewKind = (s: Pick<Style, 'view'> | undefined | null): ViewKind =>
  s?.view === 'isometric' ? 'isometric' : s?.view === 'isometric-3d' ? 'isometric-3d' : s?.view === '3d' ? 'future' : 'top-down'

/** True for a style that starts with the general (top-down) pictures: its own kind of view is top-down. */
export const inheritsGeneralArt = (game: Game | null, styleId: string | undefined): boolean =>
  styleId === undefined || viewKind(game?.styles?.find((s) => s.id === styleId)) === 'top-down'

export type IsoKey = 'tileWidth' | 'tileHeight' | 'heightStep' | 'unitWidth'
export const ISO_KEYS: IsoKey[] = ['tileWidth', 'tileHeight', 'heightStep', 'unitWidth']
/** Allowed range of each size; `even` = must be an even number. */
export const ISO_RANGE: Record<IsoKey, { min: number; max: number; even: boolean }> = {
  tileWidth: { min: 8, max: 512, even: true },
  tileHeight: { min: 4, max: 256, even: true },
  heightStep: { min: 0, max: 128, even: false },
  unitWidth: { min: 4, max: 512, even: false },
}

/** The size used when a key is empty, from the sizes around it (loader: 2 x tile size, then halves). */
export function isoDefaults(game: Game | null, iso: IsoSizes | undefined): Required<IsoSizes> {
  const tileWidth = iso?.tileWidth ?? 2 * (game?.view?.tileSize ?? 24)
  const tileHeight = iso?.tileHeight ?? Math.floor(tileWidth / 2)
  return { tileWidth, tileHeight, heightStep: iso?.heightStep ?? Math.floor(tileHeight / 2), unitWidth: iso?.unitWidth ?? Math.floor(tileWidth / 2) }
}

/** The style with another kind of view; sizes belong to the isometric kinds only, so they leave with it. */
export function withView(s: Style, kind: EditableKind): Style {
  if (kind !== 'top-down') return patchObj(s, { view: kind })
  return patchObj(s, { view: undefined, isometric: undefined })
}

/** Sets or clears (undefined) one isometric size; an empty `isometric` object is left out of the file. */
export function withIsoSize(s: Style, key: IsoKey, value: number | undefined): Style {
  const next = patchObj(s.isometric ?? {}, { [key]: value })
  return patchObj(s, { isometric: Object.keys(next).length ? next : undefined })
}
