// Who uses which image, which files nobody uses, which pictures would fall outside an image, and the sides of a game.
import { gridOfGame, isHex } from './gridSpec'
import { joinBlockSide, lookCells } from './pickCell'
import { lookUsesSheet, sheetsOf } from './graphics'
import { standPictureLook } from './stand'
import type { Look, LookKind, ProjectDoc } from './types'

export interface ArtUser { kind: LookKind; id: string; name: string }

const nameOf = (doc: ProjectDoc, kind: LookKind, id: string): string =>
  ((kind === 'terrain' ? doc.terrain : kind === 'units' ? doc.units : kind === 'characters' ? doc.characters : doc.structures) ?? []).find((x) => x.id === id)?.name ?? id

/** Every look entry in the document with the thing it belongs to. */
function* allLooks(doc: ProjectDoc): Generator<{ kind: LookKind; id: string; look: Look }> {
  const g = doc.graphics
  if (!g) return
  for (const kind of ['terrain', 'units', 'characters', 'structures'] as const) {
    for (const [id, look] of Object.entries(g[kind] ?? {})) yield { kind, id, look }
    for (const st of Object.values(g.styles ?? {}))
      for (const [id, look] of Object.entries(st[kind] ?? {})) if (look) yield { kind, id, look }
  }
}

/** The terrains, structures and units that draw from an image (base, per-side or in any look), each once, in screen order. */
export function sheetUsers(doc: ProjectDoc, sheetId: string): ArtUser[] {
  const seen = new Set<string>()
  const out: ArtUser[] = []
  for (const { kind, id, look } of allLooks(doc)) {
    const key = `${kind}:${id}`
    if (!lookUsesSheet(look, sheetId) || seen.has(key)) continue
    seen.add(key)
    out.push({ kind, id, name: nameOf(doc, kind, id) })
  }
  const rank = { terrain: 0, structures: 1, units: 2, characters: 3 } as const
  return out.sort((a, b) => rank[a.kind] - rank[b.kind])
}

/** Files in art/ that no image entry of the document points at. */
export const unusedFiles = (doc: ProjectDoc, files: Array<{ file: string }>): string[] => {
  const used = new Set(sheetsOf(doc).map((s) => s.file))
  return files.map((f) => f.file).filter((f) => !used.has(f))
}

/** Things whose pieces would lie outside an image of `cols` x `rows` pieces (their pictures would stop working). */
export function outsideUsers(doc: ProjectDoc, sheetId: string, cols: number, rows: number): ArtUser[] {
  const seen = new Set<string>()
  const out: ArtUser[] = []
  // A joining picture uses a block of 4 x 4 pieces (squares) or 8 x 8 (a hexagon game).
  const side = joinBlockSide(isHex(gridOfGame(doc.game)))
  const bad = (look: Look): boolean => {
    const join = !!look.join
    const own = look.sheet === sheetId && lookCells(look).some(([c, r]) => c + (join ? side : 1) > cols || r + (join ? side : 1) > rows)
    const stand = standPictureLook(look.stand)  // a standing picture too
    return own || Object.values(look.sides ?? {}).some(bad) || (!!stand && bad(stand))
  }
  for (const { kind, id, look } of allLooks(doc)) {
    const key = `${kind}:${id}`
    if (seen.has(key) || !bad(look)) continue
    seen.add(key)
    out.push({ kind, id, name: nameOf(doc, kind, id) })
  }
  return out
}

export interface GameSide { id: string; name: string; color?: string; /** No map has this side any more (it only still has pictures). */ orphan?: boolean }

/** Every side id any map uses (first one seen wins for name and colour), plus ids that only still have pictures. */
export function gameSides(doc: ProjectDoc): GameSide[] {
  const out: GameSide[] = []
  for (const m of Object.values(doc.maps)) {
    for (const s of m?.sides ?? []) if (!out.some((o) => o.id === s.id)) out.push({ id: s.id, name: s.name || s.id, color: s.color })
  }
  const g = doc.graphics
  const extra = new Set<string>()
  const scan = (l: Look | null | undefined) => { for (const k of Object.keys(l?.sides ?? {})) extra.add(k) }
  for (const kind of ['units', 'characters', 'structures'] as const) {
    Object.values(g?.[kind] ?? {}).forEach(scan)
    Object.values(g?.styles ?? {}).forEach((st) => Object.values(st[kind] ?? {}).forEach(scan))
  }
  for (const id of extra) if (!out.some((o) => o.id === id)) out.push({ id, name: id, orphan: true })
  return out
}
