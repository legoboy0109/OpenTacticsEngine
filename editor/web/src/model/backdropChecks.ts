// What the game's loader refuses about a map's "backdrop" (engine/core/data/loader_map_backdrop.cpp), in plain words, so the maker sees
// the problem before the game says no. `imageSize` (the image library's width and height of a file) lets the over image's size be
// checked against the pieces; without it that one check is skipped.
import { gridOfGame, isHex } from './gridSpec'
import { findSheet } from './graphics'
import { viewKind } from './styleView'
import type { ImageSize } from './tileChecks'
import type { ProjectDoc, Sheet } from './types'

export interface BackdropCheck { mapId: string; text: string }

const MAX_PIECES = 4096
const isWhole = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)
const inRange = (v: unknown, lo: number, hi: number): boolean => isWhole(v) && v >= lo && v <= hi

export function backdropChecks(doc: ProjectDoc, mapId: string, imageSize?: ImageSize): BackdropCheck[] {
  const map = doc.maps[mapId]
  const out: BackdropCheck[] = []
  const raw = (map as { backdrop?: unknown } | null | undefined)?.backdrop
  if (!map || raw === undefined) return out
  const say = (text: string) => out.push({ mapId, text })
  if (raw === null || typeof raw !== 'object' || Array.isArray(raw)) { say('The backdrop is not written as { sheet, origin, over, pieces, styles }. The game will not load until it is fixed or taken away.'); return out }
  const b = raw as Record<string, unknown>
  if (isHex(gridOfGame(doc.game))) say('Backdrops are not supported on hexagon games. The game will not load until the backdrop is taken away.')
  const sheetId = typeof b.sheet === 'string' ? b.sheet : ''
  if (sheetId === '') say('The backdrop needs an image (“sheet”). The game will not load until it has one or the backdrop is taken away.')
  else if (!findSheet(doc, sheetId)) say(`The backdrop uses the image “${sheetId}”, which this game does not have (Art screen). The game will not load until it is added or the backdrop taken away.`)
  let over: Sheet | undefined
  if (b.over !== undefined && b.over !== '') {
    over = typeof b.over === 'string' ? findSheet(doc, b.over) : undefined
    if (!over) say(`The backdrop’s over picture uses the image “${String(b.over)}”, which this game does not have (Art screen).`)
  }
  if (b.origin !== undefined) {
    const o = b.origin
    if (!Array.isArray(o) || o.length !== 2 || !inRange(o[0], -8192, 8192) || !inRange(o[1], -8192, 8192)) say('The backdrop’s origin must be two whole numbers from -8192 to 8192 (the pixel of the picture where the top corner of square 0,0 is).')
  }
  if (b.styles !== undefined) {
    const list = b.styles
    if (!Array.isArray(list) || list.some((x) => typeof x !== 'string')) say('The backdrop’s list of styles must be a list of style names.')
    else if (list.length === 0) say('The backdrop’s list of styles is empty. List at least one style, or leave the list out for every 2D isometric style.')
    else for (const id of list as string[]) {
      const st = doc.game?.styles?.find((s) => s.id === id)
      if (!st) say(`The backdrop is set for the style “${id}”, which this game does not have.`)
      else if (viewKind(st) !== 'isometric') say(`The backdrop is set for the style “${st.name || id}”, which is not of the 2D isometric kind of view. Only that kind draws a backdrop.`)
    }
  }
  if (!(doc.game?.styles ?? []).some((s) => viewKind(s) === 'isometric')) say('Only the 2D isometric kind of view draws a backdrop, and this game has no such style (Game screen, Visual styles). The game will not load until one is added or the backdrop taken away.')
  if (b.pieces !== undefined) checkPieces(map.grid, b, over, imageSize, say)
  return out
}

function checkPieces(grid: { width: number; height: number }, b: Record<string, unknown>, over: Sheet | undefined, imageSize: ImageSize | undefined, say: (t: string) => void): void {
  if (b.over === undefined || b.over === '') { say('The backdrop has pieces, but pieces are cut from the over picture and it has none. The game will not load until an over picture is chosen or the pieces taken away.'); return }
  const list = b.pieces
  if (!Array.isArray(list)) { say('The backdrop’s pieces must be a list.'); return }
  if (list.length < 1 || list.length > MAX_PIECES) { say(`The backdrop must have between 1 and ${MAX_PIECES} pieces (or none at all).`); return }
  const size = over && imageSize ? imageSize(over.file) : undefined
  let reported = 0
  list.forEach((p, i) => {
    if (reported >= 3) return
    const bad = (t: string) => { say(`Backdrop piece ${i + 1}: ${t}`); reported++ }
    if (p === null || typeof p !== 'object') return bad('it must be { rect: [x, y, width, height], square: [x, y] }.')
    const { rect, square } = p as { rect?: unknown; square?: unknown }
    if (rect === undefined || square === undefined) return bad('it needs both a rect and a square.')
    if (!Array.isArray(rect) || rect.length !== 4 || !inRange(rect[0], 0, 4096) || !inRange(rect[1], 0, 4096) || !inRange(rect[2], 1, 4096) || !inRange(rect[3], 1, 4096)) return bad('the rect must be [x, y, width, height]: whole numbers, x and y from 0, width and height at least 1.')
    const [x, y, w, h] = rect as number[]
    if (size && size.width > 0 && (x + w > size.width || y + h > size.height)) return bad(`the rect [${x}, ${y}, ${w}, ${h}] reaches outside the over picture (${size.width} × ${size.height} pixels).`)
    if (!Array.isArray(square) || square.length !== 2 || !inRange(square[0], 0, 9999) || !inRange(square[1], 0, 9999)) return bad('the square must be [x, y]: the square the piece stands on.')
    if ((square[0] as number) >= grid.width || (square[1] as number) >= grid.height) bad(`the square ${square[0]},${square[1]} is outside the map (${grid.width} × ${grid.height}).`)
  })
}
