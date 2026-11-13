// Friendly in-editor checks. They are separate from the helper's "Problems":
// the helper reports what stops the game loading, these point at things that
// load fine but are probably not what the author meant.
import { ctxOf } from './mapCtx'
import { terrainAt } from './mapGrid'
import { canEnter } from './mapUnits'
import type { MapFile, ProjectDoc } from './types'

export interface MapWarning { key: string; text: string; cell?: { x: number; y: number } }

export function mapWarnings(doc: ProjectDoc, map: MapFile): MapWarning[] {
  const c = ctxOf(doc)
  if (!c) return []
  const out: MapWarning[] = []
  const sides = map.sides ?? []
  const units = map.units ?? []
  const defeat = doc.game?.rules?.defeat ?? ['noUnits']
  const tOf = (id: string | undefined) => c.terrain.find((t) => t.id === id)

  for (const u of units) {
    const t = tOf(terrainAt(map, u.x, u.y))
    const type = c.unitTypes.find((x) => x.id === u.type)
    if (!canEnter(t, type)) out.push({ key: `blocked:${u.x},${u.y}`, cell: { x: u.x, y: u.y }, text: `${type?.name ?? u.type} at ${u.x + 1},${u.y + 1} stands on ${t?.name ?? 'terrain'}, which it cannot enter. The game will not load until it is moved or the terrain changed.` })
  }
  if (sides.length < 2) out.push({ key: 'few-sides', text: 'A battle needs at least two sides.' })

  const producing = doc.game?.rules?.production?.enabled
  for (const s of sides) {
    const hasUnits = units.some((u) => u.side === s.id)
    const owned = (map.owners ?? []).filter((o) => o.side === s.id)
    const canBuild = !!producing && owned.some((o) => tOf(terrainAt(map, o.x, o.y))?.produces?.length)
    if (!hasUnits && !canBuild) out.push({ key: `no-units:${s.id}`, text: `${s.name} has no units and no way to get any${producing ? ' (it owns no building that makes units)' : ''}.` })
    if (defeat.includes('leaderDead') && hasUnits && !units.some((u) => u.side === s.id && u.leader)) {
      out.push({ key: `no-leader:${s.id}`, text: `${s.name} has no leader, but "its leader is defeated" is a way to lose. Mark one of its units as the leader.` })
    }
    if (defeat.includes('lostHq') && !owned.some((o) => tOf(terrainAt(map, o.x, o.y))?.tags?.includes('hq'))) {
      out.push({ key: `no-hq:${s.id}`, text: `${s.name} owns no headquarters, but "its headquarters is captured" is a way to lose. Set it as the owner of a headquarters square.` })
    }
  }
  return out
}
