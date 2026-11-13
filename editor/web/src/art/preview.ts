// Small sample maps for previews and palettes. They are drawn by the real map renderer (drawMap), so a preview can never
// look different from the canvas. A preview may use a draft look (what the picker has chosen so far) via `doc`.
import { effectiveLook } from '../model/graphicsLooks'
import { gameSides } from '../model/graphicsUsage'
import { drawMap, type Scene } from '../screens/maps/editor/drawMap'
import type { MapFile, MapSide, ProjectDoc } from '../model/types'
import { buildArtScene, type ImageSource, type StyleView } from './scene'

export interface PreviewSpec { scene: Scene; cols: number; rows: number; scale: number; /** First square drawn (thumbnails show the middle square). */ origin: { x: number; y: number } }

/** A road with a dead end, a bend, a T and a crossing ('#' = the terrain, '.' = something else). */
export const JOIN_SAMPLE = ['..#....', '..#.##.', '.####..', '..##...', '.......']

const SIDE_COLORS = ['#d23b3b', '#2f6fdb']
const mapOf = (cols: number, rows: number, legend: Record<string, string>, tiles: string[], sides: MapSide[] = [], units: MapFile['units'] = []): MapFile =>
  ({ name: 'preview', grid: { width: cols, height: rows }, legend, tiles, sides, units })

function sceneFor(doc: ProjectDoc, src: ImageSource, style: StyleView, styleId: string | undefined, map: MapFile): Scene {
  return {
    map, terrain: new Map((doc.terrain ?? []).map((t) => [t.id, t])), types: new Map((doc.units ?? []).map((u) => [u.id, u])),
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

/** Draws a preview onto a canvas whose backing store is already sized. */
export function drawPreview(g: CanvasRenderingContext2D, spec: PreviewSpec, dpr: number, time: number): void {
  const w = spec.cols * spec.scale, h = spec.rows * spec.scale
  drawMap(g, spec.scene, { dpr, w, h, view: { scale: spec.scale, x: -spec.origin.x * spec.scale, y: -spec.origin.y * spec.scale }, hover: null, cursor: null, selected: null, ghost: null, time })
}
