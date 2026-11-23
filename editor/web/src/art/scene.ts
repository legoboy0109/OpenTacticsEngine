// What the canvases need to draw art: every terrain's and unit's look for one visual style, with its image already
// decoded. Built from the document + the image library; rebuilt when either changes (cheap: a few lookups).
import { findSheet, unitMarkerOn } from '../model/graphics'
import { effectiveLook } from '../model/graphicsLooks'
import { inheritsGeneralArt } from '../model/styleView'
import { lookCells, lookShape, type LookShape } from '../model/pickCell'
import type { Badge, Game, Look, ProjectDoc, Sheet } from '../model/types'
import type { LoadedImage } from '../store/art'

/** A look whose image is loaded and which can be drawn. */
export interface Resolved { look: Look; shape: LookShape; cw: number; ch: number; img: CanvasImageSource; iw: number; ih: number }
export interface ResolvedUnit { base: Resolved; sides: Map<string, Resolved> }

/** A unit badge (docs/GRAPHICS.md "Unit badges") with its image decoded: where in the image, who wears it, and where it sits on the unit's square. */
export interface ResolvedBadge {
  badge: Badge
  img: CanvasImageSource
  sx: number; sy: number; sw: number; sh: number
  /** Unit types and characters that have the badge's tag (a unit type's tag or a character's tag). */
  types: Set<string>
  characters: Set<string>
}

export interface ArtScene {
  marker: boolean
  /** Unit badges, in drawing order, and the game's tile size (what their size and offset are measured in). */
  badges: ResolvedBadge[]
  tileSize: number
  terrain: Map<string, Resolved>
  units: Map<string, ResolvedUnit>
  /** Structure id -> its picture (and per-owner pictures); drawn across the whole footprint. */
  structures: Map<string, ResolvedUnit>
  /** Terrain id -> ids that count as "the same" for joining (itself included). */
  joinSame: Map<string, Set<string>>
  /** Most extra rows above its square any sprite sticks out (tall sprites), from the cell proportions. */
  tallRows: number
  /** Some drawn picture is an animation: the canvas keeps redrawing. */
  animated: boolean
}

export interface ImageSource { image(file: string): LoadedImage }

export function resolveLook(doc: ProjectDoc, src: ImageSource, look: Look | null | undefined): Resolved | null {
  if (!look) return null
  const sheet: Sheet | undefined = findSheet(doc, look.sheet)
  if (!sheet) return null
  const im = src.image(sheet.file)
  if (im.status !== 'ready' || !im.img) return null
  return { look, shape: lookShape(look), cw: sheet.cellWidth, ch: sheet.cellHeight, img: im.img, iw: im.width, ih: im.height }
}

/** The terrains a joining look counts as "the same": itself and joinWith, or (joinExcept) everything but the listed ones. */
function joinSet(doc: ProjectDoc, id: string, look: Look): Set<string> {
  if (look.joinExcept) return new Set((doc.terrain ?? []).map((t) => t.id).filter((x) => x === id || !look.joinExcept!.includes(x)))
  return new Set([id, ...(look.joinWith ?? [])])
}

/** Null when the game has no images at all: every canvas then keeps its plain placeholder look (and its fast path). */
export function buildArtScene(doc: ProjectDoc, style: string | undefined, src: ImageSource): ArtScene | null {
  const g = doc.graphics
  if (!g || g.sheets.length === 0) return null
  const scene: ArtScene = { marker: unitMarkerOn(doc), badges: [], tileSize: doc.game?.view?.tileSize ?? 24, terrain: new Map(), units: new Map(), structures: new Map(), joinSame: new Map(), tallRows: 0, animated: false }
  const inherit = inheritsGeneralArt(doc.game, style)
  const note = (r: Resolved) => { if (r.shape === 'animated' || (r.shape === 'join' && lookCells(r.look).length > 1)) scene.animated = true }
  for (const t of doc.terrain ?? []) {
    const r = resolveLook(doc, src, effectiveLook(g, 'terrain', t.id, style, inherit))
    if (!r) continue
    scene.terrain.set(t.id, r)
    note(r)
    if (r.shape === 'join') scene.joinSame.set(t.id, joinSet(doc, t.id, r.look))
  }
  for (const u of doc.units ?? []) {
    const entry = effectiveLook(g, 'units', u.id, style, inherit)
    const base = resolveLook(doc, src, entry)
    if (!base) continue
    const sides = new Map<string, Resolved>()
    for (const [side, l] of Object.entries(entry?.sides ?? {})) { const r = resolveLook(doc, src, l); if (r) sides.set(side, r) }
    scene.units.set(u.id, { base, sides })
    for (const r of [base, ...sides.values()]) {
      note(r)
      scene.tallRows = Math.max(scene.tallRows, Math.ceil(r.ch / r.cw) - 1)
    }
  }
  for (const b of g.badges ?? []) {
    const sheet = findSheet(doc, b.sheet)
    const im = sheet ? src.image(sheet.file) : null
    if (!sheet || !im || im.status !== 'ready' || !im.img || typeof b.tag !== 'string') continue
    const whole = !Array.isArray(b.cell)
    const types = new Set((doc.units ?? []).filter((u) => (u.tags ?? []).includes(b.tag)).map((u) => u.id))
    const characters = new Set((doc.characters ?? []).filter((c) => ((c as { tags?: string[] }).tags ?? []).includes(b.tag) || types.has((c as { type?: string }).type ?? '')).map((c) => c.id))
    scene.badges.push({
      badge: b, img: im.img, types, characters,
      sx: whole ? 0 : b.cell![0] * sheet.cellWidth, sy: whole ? 0 : b.cell![1] * sheet.cellHeight,
      sw: whole ? im.width : sheet.cellWidth, sh: whole ? im.height : sheet.cellHeight,
    })
  }
  for (const s of doc.structures ?? []) {
    const entry = effectiveLook(g, 'structures', s.id, style, inherit)
    const base = resolveStructure(doc, src, entry)
    if (!base) continue
    const sides = new Map<string, Resolved>()
    for (const [side, l] of Object.entries(entry?.sides ?? {})) { const r = resolveStructure(doc, src, l); if (r) sides.set(side, r) }
    scene.structures.set(s.id, { base, sides })
    for (const r of [base, ...sides.values()]) note(r)
  }
  return scene
}

/** Structures only draw one piece or an animation (the file format refuses anything else). */
const resolveStructure = (doc: ProjectDoc, src: ImageSource, look: Look | null | undefined): Resolved | null => {
  const r = resolveLook(doc, src, look)
  return r && (r.shape === 'still' || r.shape === 'animated') ? r : null
}

/** The picture of a structure owned by `side` (or by nobody: undefined): its own for that side, else the general one. */
export const structurePicture = (art: ArtScene | null, type: string, side: string | undefined): Resolved | null => {
  const s = art?.structures.get(type)
  return s ? (side !== undefined ? s.sides.get(side) : undefined) ?? s.base : null
}

/** The picture of a unit of one side: its own for that side, else the general one; null = draw the placeholder block. */
export const unitPicture = (art: ArtScene | null, type: string, side: string): Resolved | null => {
  const u = art?.units.get(type)
  return u ? u.sides.get(side) ?? u.base : null
}

// ---- visual style as the canvas shows it ----
export interface StyleView { background?: string; showGlyphs: boolean; terrainColors: Record<string, string>; structureColors: Record<string, string> }
export const PLAIN_STYLE: StyleView = { showGlyphs: true, terrainColors: {}, structureColors: {} }

export function styleView(game: Game | null, styleId: string | undefined): StyleView {
  const s = styleId === undefined ? undefined : game?.styles?.find((x) => x.id === styleId)
  if (!s) return PLAIN_STYLE
  return { background: s.background, showGlyphs: s.showGlyphs !== false, terrainColors: s.terrainColors ?? {}, structureColors: s.structureColors ?? {} }
}
