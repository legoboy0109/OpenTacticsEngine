// A map's backdrop (docs/VIEWS.md "Backdrop"): the prerendered picture the 2D isometric view draws in place of its generated
// squares. The Backdrop panel on the map's Map tab edits the image, the origin, the over image and the styles; the pieces cut from the
// over image (`pieces`) are written by converters and tools, so the panel only shows how many there are and keeps them as they are.
// The checks that mirror the loader are in backdropChecks.ts. Pure functions.
import { patchObj } from './util'
import { gridOfGame, isHex } from './gridSpec'
import { viewKind } from './styleView'
import type { MapBackdrop, MapFile, ProjectDoc, Style } from './types'

export const BACKDROP_MAX_ORIGIN = 8192

/** The visual styles a backdrop can be drawn in: those of the 2D isometric kind of view. */
export const isometricStyles = (doc: ProjectDoc): Style[] => (doc.game?.styles ?? []).filter((s) => viewKind(s) === 'isometric')

/** True when the editor offers a backdrop here: a square game with at least one 2D isometric style. */
export const backdropAllowed = (doc: ProjectDoc): boolean => !isHex(gridOfGame(doc.game)) && isometricStyles(doc).length > 0

export const backdropOf = (map: MapFile | null | undefined): MapBackdrop | undefined => map?.backdrop

/** Applies `fn` to the map's backdrop (undefined result removes it). */
function editBackdrop(doc: ProjectDoc, mapId: string, fn: (b: MapBackdrop | undefined) => MapBackdrop | undefined): ProjectDoc {
  const map = doc.maps[mapId]
  if (!map) return doc
  const next = fn(map.backdrop)
  if (JSON.stringify(next) === JSON.stringify(map.backdrop)) return doc
  return { ...doc, maps: { ...doc.maps, [mapId]: patchObj(map, { backdrop: next }) } }
}

/** A first backdrop on `sheet` (the editor never writes a backdrop without its image: the loader refuses it). */
export const addBackdrop = (doc: ProjectDoc, mapId: string, sheet: string): ProjectDoc => editBackdrop(doc, mapId, (b) => b ?? { sheet })
export const removeBackdrop = (doc: ProjectDoc, mapId: string): ProjectDoc => editBackdrop(doc, mapId, () => undefined)

export const setBackdropSheet = (doc: ProjectDoc, mapId: string, sheet: string): ProjectDoc =>
  editBackdrop(doc, mapId, (b) => (b && sheet !== '' ? patchObj(b, { sheet }) : b))

const whole = (n: number) => Math.max(-BACKDROP_MAX_ORIGIN, Math.min(BACKDROP_MAX_ORIGIN, Math.round(Number.isFinite(n) ? n : 0)))
/** Where square 0,0's top corner (at height 0) is in the image, in pixels; [0, 0] is the default so the key goes. */
export function setBackdropOrigin(doc: ProjectDoc, mapId: string, x: number, y: number): ProjectDoc {
  return editBackdrop(doc, mapId, (b) => (b ? patchObj(b, { origin: whole(x) === 0 && whole(y) === 0 ? undefined : [whole(x), whole(y)] }) : b))
}

/** The picture drawn over the units; '' takes it away (and with it the pieces, which only make sense cut from it). */
export const setBackdropOver = (doc: ProjectDoc, mapId: string, over: string): ProjectDoc =>
  editBackdrop(doc, mapId, (b) => (b ? (over === '' ? patchObj(b, { over: undefined, pieces: undefined }) : patchObj(b, { over })) : b))

/** The styles that draw it; an empty list = every 2D isometric style (the key goes: the loader refuses an empty list). */
export const setBackdropStyles = (doc: ProjectDoc, mapId: string, styles: string[]): ProjectDoc =>
  editBackdrop(doc, mapId, (b) => (b ? patchObj(b, { styles: styles.length ? styles : undefined }) : b))

export const pieceCount = (b: MapBackdrop | undefined): number => (Array.isArray(b?.pieces) ? b!.pieces!.length : 0)
