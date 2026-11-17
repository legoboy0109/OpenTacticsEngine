// A small top-down picture of a map for a level box. It is the map editor's own drawing (drawMap, plain look: terrain colours,
// placed units, structures) on an off-screen canvas, saved once as a picture and cached until the map or what it is drawn from
// changes; so a box costs one <img>. If the drawing fails, a plain one-pixel-per-square miniature of the terrain colours is used.
import { PLAIN_STYLE } from '../../../art/scene'
import { withResolvedTypes } from '../../../model/characterUnits'
import { gridOfGame } from '../../../model/gridSpec'
import { makeGeo } from '../../../model/mapGeo'
import type { MapFile, ProjectDoc } from '../../../model/types'
import { drawMap } from '../../maps/editor/drawMap'
import { parseHex } from '../../maps/editor/drawColors'

export const THUMB_W = 72
export const THUMB_H = 48
const SCALE = 2 // drawn at twice the size so it stays sharp on dense screens

interface Entry { terrain: unknown; units: unknown; structures: unknown; characters: unknown; game: unknown; url: string | null }
const cache = new WeakMap<MapFile, Entry>()

/** A data: URL of the map's picture, or null when the map cannot be drawn (not loaded, no canvas). */
export function mapThumbnail(doc: ProjectDoc, mapId: string): string | null {
  const map = doc.maps[mapId]
  if (!map || !doc.terrain || !doc.units) return null
  const hit = cache.get(map)
  if (hit && hit.terrain === doc.terrain && hit.units === doc.units && hit.structures === doc.structures && hit.characters === doc.characters && hit.game === doc.game) return hit.url
  const url = draw(doc, map)
  cache.set(map, { terrain: doc.terrain, units: doc.units, structures: doc.structures, characters: doc.characters, game: doc.game, url })
  return url
}

function draw(doc: ProjectDoc, map: MapFile): string | null {
  if (typeof document === 'undefined') return null
  const canvas = document.createElement('canvas')
  canvas.width = THUMB_W * SCALE
  canvas.height = THUMB_H * SCALE
  const g = canvas.getContext('2d')
  if (!g) return null
  try {
    const grid = gridOfGame(doc.game)
    const geo = makeGeo(grid, doc.game?.view?.tileSize ?? 24, map.grid.width, map.grid.height)
    // Fitted by hand: the editor's own fit never goes below 2 pixels a square, and a very big map must still fit the picture.
    const scale = Math.min((THUMB_W - 4) / geo.uw, (THUMB_H - 4) / geo.uh)
    const view = { scale, x: (THUMB_W - geo.uw * scale) / 2, y: (THUMB_H - geo.uh * scale) / 2 }
    drawMap(g, {
      map: withResolvedTypes(map, doc.characters), terrain: new Map(doc.terrain!.map((t) => [t.id, t])), types: new Map(doc.units!.map((u) => [u.id, u])),
      structures: new Map((doc.structures ?? []).map((s) => [s.id, s])), flagged: new Set(), art: null, style: PLAIN_STYLE, grid, geo,
    }, { dpr: SCALE, w: THUMB_W, h: THUMB_H, view, hover: null, cursor: null, selected: null, ghost: null, time: 0 })
    return canvas.toDataURL('image/png')
  } catch {
    return miniature(doc, map, g, canvas)
  }
}

/** One rectangle per square in the terrain's colour: cheap and always works (squares; hexagon maps are drawn as squares too). */
function miniature(doc: ProjectDoc, map: MapFile, g: CanvasRenderingContext2D, canvas: HTMLCanvasElement): string | null {
  try {
    const w = map.grid.width, h = map.grid.height
    const s = Math.min(canvas.width / w, canvas.height / h)
    g.fillStyle = '#14161c'
    g.fillRect(0, 0, canvas.width, canvas.height)
    const ox = (canvas.width - w * s) / 2, oy = (canvas.height - h * s) / 2
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const t = doc.terrain?.find((tt) => tt.id === map.legend[map.tiles[y]?.[x] ?? ''])
        const [r, gg, b] = parseHex(t?.color, [136, 136, 136])
        g.fillStyle = `rgb(${r},${gg},${b})`
        g.fillRect(Math.floor(ox + x * s), Math.floor(oy + y * s), Math.ceil(s), Math.ceil(s))
      }
    }
    return canvas.toDataURL('image/png')
  } catch {
    return null
  }
}
