// A visual style's kind of view (docs/VIEWS.md) and the sizes of the isometric one. Mirrors engine/core/data/loader.cpp
// (`loadStyles`), which stays the source of truth for the ranges.
import { patchObj } from './util'
import type { Game, IsoSizes, Style } from './types'

/** '3d' = full 3D (file value "3d"). */
export type ViewKind = 'top-down' | 'isometric' | 'isometric-3d' | '3d'

/** Every kind of view, in the order the Game screen lists them. */
export const ALL_KINDS: ViewKind[] = ['top-down', 'isometric', 'isometric-3d', '3d']
/** What a hexagon game may use when the helper does not say (an older build): top-down only. */
export const HEX_TOP_DOWN_ONLY: string[] = ['top-down']

/** The kinds of view a game's styles can use. A square game: all four. A hexagon game: those the engine the editor runs with
 *  draws for hexagons (docs/HEX.md; `hexViews` from the helper's /api/info), so the list follows the build. */
export function allowedKinds(hex: boolean, hexViews: readonly string[] | undefined): ViewKind[] {
  if (!hex) return ALL_KINDS
  const offered = hexViews ?? HEX_TOP_DOWN_ONLY
  return ALL_KINDS.filter((k) => offered.includes(k))
}

/** The kinds a style can be switched between here. */
export type EditableKind = ViewKind
/** Only the two isometric kinds have the isometric measures; full 3D does not use them. */
export const isIsometric = (k: ViewKind): boolean => k === 'isometric' || k === 'isometric-3d'

export const viewKind = (s: Pick<Style, 'view'> | undefined | null): ViewKind =>
  s?.view === 'isometric' ? 'isometric' : s?.view === 'isometric-3d' ? 'isometric-3d' : s?.view === '3d' ? '3d' : 'top-down'

/** True for a style that starts with the general (top-down) pictures: its own kind of view is top-down. */
export const inheritsGeneralArt = (game: Game | null, styleId: string | undefined): boolean =>
  styleId === undefined || viewKind(game?.styles?.find((s) => s.id === styleId)) === 'top-down'

export type IsoKey = 'tileWidth' | 'tileHeight' | 'heightStep' | 'unitWidth'
export const ISO_KEYS: IsoKey[] = ['tileWidth', 'tileHeight', 'heightStep', 'unitWidth']
/** The sizes offered for a game: on a hexagon game the squash is fixed, so there is no tileHeight (the loader refuses it). */
export const isoKeysFor = (hex: boolean): IsoKey[] => (hex ? ISO_KEYS.filter((k) => k !== 'tileHeight') : ISO_KEYS)
/** Allowed range of each size; `even` = must be an even number. */
export const ISO_RANGE: Record<IsoKey, { min: number; max: number; even: boolean }> = {
  tileWidth: { min: 8, max: 512, even: true },
  tileHeight: { min: 4, max: 256, even: true },
  heightStep: { min: 0, max: 128, even: false },
  unitWidth: { min: 4, max: 512, even: false },
}

/** The size used when a key is empty, from the sizes around it (loader: 2 x tile size, then halves). On a hexagon game the
 *  height step is a quarter of the width (docs/HEX.md) and tileHeight is not used (its value is the squares' one, ignored). */
export function isoDefaults(game: Game | null, iso: IsoSizes | undefined, hex = false): Required<IsoSizes> {
  const tileWidth = iso?.tileWidth ?? 2 * (game?.view?.tileSize ?? 24)
  const tileHeight = iso?.tileHeight ?? Math.floor(tileWidth / 2)
  const heightStep = iso?.heightStep ?? (hex ? Math.floor(tileWidth / 4) : Math.floor(tileHeight / 2))
  return { tileWidth, tileHeight, heightStep, unitWidth: iso?.unitWidth ?? Math.floor(tileWidth / 2) }
}

/** True for the kinds that start with no art and can show units and structures as 3D models. */
export const is3dKind = (k: ViewKind): boolean => k === 'isometric-3d' || k === '3d'

/** The style with another kind of view; sizes belong to the isometric kinds only, so they leave with it
 *  (top-down and full 3D both drop them). */
export function withView(s: Style, kind: EditableKind): Style {
  const view = kind === 'top-down' ? undefined : kind
  return patchObj(s, kind === 'isometric' || kind === 'isometric-3d' ? { view } : { view, isometric: undefined })
}

/** Sets or clears (undefined) one isometric size; an empty `isometric` object is left out of the file. */
export function withIsoSize(s: Style, key: IsoKey, value: number | undefined): Style {
  const next = patchObj(s.isometric ?? {}, { [key]: value })
  return patchObj(s, { isometric: Object.keys(next).length ? next : undefined })
}
