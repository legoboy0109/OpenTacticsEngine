// Small sample maps for previews and palettes. They are drawn by the real map renderer (drawMap), so a preview can never
// look different from the canvas. A preview may use a draft look (what the picker has chosen so far) via `doc`.
import { effectiveLook } from '../model/graphicsLooks'
import { gameSides } from '../model/graphicsUsage'
import { drawMap, type Scene } from '../screens/maps/editor/drawMap'
import { sizeOf } from '../model/structures'
import type { MapFile, MapSide, MapStructure, ProjectDoc } from '../model/types'
import { buildArtScene, type ImageSource, type StyleView } from './scene'

export interface PreviewSpec { scene: Scene; cols: number; rows: number; scale: number; /** First square drawn (thumbnails show the middle square). */ origin: { x: number; y: number } }

/** A road with a dead end, a bend, a T and a crossing ('#' = the terrain, '.' = something else). */
export const JOIN_SAMPLE = ['..#....', '..#.##.', '.####..', '..##...', '.......']

const SIDE_COLORS = ['#d23b3b', '#2f6fdb']
const mapOf = (cols: number, rows: number, legend: Record<string, string>, tiles: string[], sides: MapSide[] = [], units: MapFile['units'] = [], structures: MapStructure[] = []): MapFile =>
  ({ name: 'preview', grid: { width: cols, height: rows }, legend, tiles, sides, units, structures })

function sceneFor(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, map: MapFile): Scene {
  return {
    map, terrain: new Map((doc.terrain ?? []).map((t) => [t.id, t])), types: new Map((doc.units ?? []).map((u) => [u.id, u])),
    structures: new Map((doc.structures ?? []).map((s) => [s.id, s])),
    flagged: new Set(), art: buildArtScene(doc, styleId, src), style,
  }
}
const otherTerrain = (doc: ProjectDoc, id: string): string => (doc.terrain ?? []).find((t) => t.id !== id)?.id ?? id

/** What a terrain looks like in context: joining pictures get a road-like shape, random mixes a field, the rest a small patch. */
export function terrainPreview(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, id: string, scale = 36): PreviewSpec {
  const look = effectiveLook(doc.graphics, 'terrain', id, styleId)
  const join = !!look?.join
  const mix = (look?.variants?.length ?? 0) > 1
  const legend = { a: id, b: otherTerrain(doc, id) }
  const tiles = join ? JOIN_SAMPLE.map((r) => r.replace(/#/g, 'a').replace(/\./g, 'b')) : mix ? Array(5).fill('aaaaa') : Array(3).fill('aaa')
  const cols = tiles[0].length, rows = tiles.length
  return { scene: sceneFor(doc, src, style, styleId, mapOf(cols, rows, legend, tiles)), cols, rows, scale, origin: { x: 0, y: 0 } }
}

/** One square of a terrain for the palette: the middle of a row of three, so joining pictures show a straight piece. */
export function terrainThumb(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, id: string, size = 28): PreviewSpec {
  const map = mapOf(3, 1, { a: id }, ['aaa'])
  return { scene: sceneFor(doc, src, style, styleId, map), cols: 1, rows: 1, scale: size, origin: { x: 1, y: 0 } }
}

/** The first two sides of the game (their colours), or two stand-ins. Side ids are kept so per-side pictures show. */
export function previewSides(doc: ProjectDoc): MapSide[] {
  const seen = new Map<string, MapSide>()
  for (const m of Object.values(doc.maps)) for (const s of m?.sides ?? []) if (!seen.has(s.id)) seen.set(s.id, s)
  const real = [...seen.values()].slice(0, 2)
  const out = real.map((s) => ({ ...s, color: s.color ?? '#888888' }))
  while (out.length < 2) out.push({ id: `side${out.length + 1}`, name: `Side ${out.length + 1}`, color: SIDE_COLORS[out.length] })
  return out
}

/** A unit on its coloured base for two sides, on the game's first terrain; tall pictures get room above. */
export function unitPreview(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, id: string, scale = 48, sideIds?: string[]): PreviewSpec {
  const sides = sideIds?.length
    ? sideIds.map((sid, i) => ({ id: sid, name: sid, color: gameSides(doc).find((s) => s.id === sid)?.color ?? SIDE_COLORS[i % 2] }))
    : previewSides(doc)
  const probe = buildArtScene(doc, styleId, src)
  const extra = Math.min(2, probe?.tallRows ?? 0)
  const terrainId = doc.terrain?.[0]?.id ?? ''
  const cols = sides.length
  const tiles = Array(1 + extra).fill('a'.repeat(cols))
  const units = sides.map((s, i) => ({ type: id, side: s.id, x: i, y: extra }))
  const map = mapOf(cols, 1 + extra, { a: terrainId }, tiles, sides, units)
  return { scene: { ...sceneFor(doc, src, style, styleId, map), art: probe }, cols, rows: 1 + extra, scale, origin: { x: 0, y: 0 } }
}

/** One unit for the palette (side colour from the map being edited). */
export function unitThumb(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, id: string, side: MapSide | undefined, size = 32): PreviewSpec {
  const probe = buildArtScene(doc, styleId, src)
  const extra = Math.min(2, probe?.tallRows ?? 0)
  const sd = side ?? { id: 'preview', name: 'Preview', color: '#888888' }
  const terrainId = doc.terrain?.[0]?.id ?? ''
  const map = mapOf(1, 1 + extra, { a: terrainId }, Array(1 + extra).fill('a'), [sd], [{ type: id, side: sd.id, x: 0, y: extra }])
  return { scene: { ...sceneFor(doc, src, style, styleId, map), art: probe }, cols: 1, rows: 1 + extra, scale: Math.floor(size / (1 + extra)), origin: { x: 0, y: 0 } }
}

/** Rows a structure's picture sticks out above its footprint (from the piece's proportions), at most `max`. */
function stickOut(probe: ReturnType<typeof buildArtScene>, id: string, w: number, h: number, max = 4): number {
  const pics = probe?.structures.get(id)
  if (!pics) return 0
  const rows = Math.max(...[pics.base, ...pics.sides.values()].map((r) => Math.ceil((w * r.ch) / r.cw)))
  return Math.min(max, Math.max(0, rows - h))
}

/** A structure standing on a patch of the first terrain at its real footprint, owned by each of `sideIds` (default: the first
 *  side and nobody), so the owner frame and the room a tall picture needs above are both visible. */
export function structurePreview(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, id: string, scale = 40, sideIds?: Array<string | undefined>): PreviewSpec {
  const def = (doc.structures ?? []).find((s) => s.id === id)
  const { w, h } = sizeOf(def)
  const owners = sideIds?.length ? sideIds : [previewSides(doc)[0].id, undefined]
  const sides = [...previewSides(doc), ...gameSides(doc).filter((g) => owners.includes(g.id)).map((g, i) => ({ id: g.id, name: g.name, color: g.color ?? SIDE_COLORS[i % 2] }))]
  const probe = buildArtScene(doc, styleId, src)
  const extra = stickOut(probe, id, w, h)
  const cols = owners.length * (w + 1) + 1, rows = h + extra + 1
  const terrainId = doc.terrain?.[0]?.id ?? ''
  const placed = owners.map((side, i) => ({ type: id, x: 1 + i * (w + 1), y: extra, ...(side ? { side } : {}) }))
  const map = mapOf(cols, rows, { a: terrainId }, Array(rows).fill('a'.repeat(cols)), sides, [], placed)
  const fit = Math.max(14, Math.min(scale, Math.floor(300 / cols), Math.floor(260 / rows)))
  return { scene: { ...sceneFor(doc, src, style, styleId, map), art: probe }, cols, rows, scale: fit, origin: { x: 0, y: 0 } }
}

/** One structure for the palette, at its real footprint (unowned or in the colour of `side`); tall pictures get room above. */
export function structureThumb(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, id: string, side: MapSide | undefined, size = 40): PreviewSpec {
  const def = (doc.structures ?? []).find((s) => s.id === id)
  const { w, h } = sizeOf(def)
  const probe = buildArtScene(doc, styleId, src)
  const extra = stickOut(probe, id, w, h, 2)
  const rows = h + extra
  const terrainId = doc.terrain?.[0]?.id ?? ''
  const map = mapOf(w, rows, { a: terrainId }, Array(rows).fill('a'.repeat(w)), side ? [side] : [], [], [{ type: id, x: 0, y: extra, ...(side ? { side: side.id } : {}) }])
  return { scene: { ...sceneFor(doc, src, style, styleId, map), art: probe }, cols: w, rows, scale: Math.max(4, Math.floor(size / Math.max(w, rows))), origin: { x: 0, y: 0 } }
}

/** Draws a preview onto a canvas whose backing store is already sized. */
export function drawPreview(g: CanvasRenderingContext2D, spec: PreviewSpec, dpr: number, time: number): void {
  const w = spec.cols * spec.scale, h = spec.rows * spec.scale
  drawMap(g, spec.scene, { dpr, w, h, view: { scale: spec.scale, x: -spec.origin.x * spec.scale, y: -spec.origin.y * spec.scale }, hover: null, cursor: null, selected: null, ghost: null, time })
}
