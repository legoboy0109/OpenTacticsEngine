// What would go wrong with a map's preparation screen: the things the game loader refuses (it will not load until they are
// fixed) and the things that load but are probably not meant (docs/EVENTS.md "Preparation"). Plain words, each with the
// square it is about when there is one.
import { rosterUnits } from './campaignRoster'
import { storyIdOf, typeIdOf } from './characterUnits'
import { campaignRosterIds } from './prepRoster'
import { ctxOf } from './mapCtx'
import { terrainAt } from './mapGrid'
import { requiredId, requiredSlot, someoneCanStand, stayingAt, typeCanStand, prepSide, isChoosable } from './prep'
import type { ProjectDoc } from './types'

export interface PrepCheck { text: string; cell?: { x: number; y: number } }

export function prepChecks(doc: ProjectDoc, mapId: string): PrepCheck[] {
  const map = doc.maps[mapId]
  const c = ctxOf(doc)
  const prep = map?.prep
  if (!map || !c || !prep) return []
  const out: PrepCheck[] = []
  const nameOf = (id: string) => c.characters.find((p) => p.id === id)?.name ?? [...(map.units ?? []), ...rosterUnits(doc)].find((u) => storyIdOf(u) === id)?.name ?? id
  const slots = prep.slots ?? []
  const limit = prep.limit ?? slots.length
  const required = prep.required ?? []

  if (slots.length === 0) out.push({ text: 'The preparation has no deployment squares. The game will not load until you add one with the Slots tool.' })
  if (limit > slots.length && slots.length > 0) out.push({ text: `The limit is ${limit} but the map has only ${slots.length} slot${slots.length === 1 ? '' : 's'}. The game will not load until the limit is lowered or slots are added.` })
  if (limit < 1) out.push({ text: 'The limit must be at least 1. The game will not load until it is.' })
  if (required.length > Math.max(limit, 0)) out.push({ text: `${required.length} characters are required but only ${limit} may be deployed. The game will not load until the limit is raised or some are not required.` })

  slots.forEach((s, i) => {
    const n = i + 1
    if (s.x < 0 || s.y < 0 || s.x >= map.grid.width || s.y >= map.grid.height) {
      out.push({ text: `Slot ${n} is outside the map. The game will not load until it is removed.` })
      return
    }
    if (!someoneCanStand(map, c, s.x, s.y)) out.push({ cell: s, text: `Slot ${n} at ${s.x + 1},${s.y + 1} is on ${c.terrain.find((t) => t.id === terrainAt(map, s.x, s.y))?.name ?? 'terrain'} that no kind of unit can stand on. The game will not load until it is removed.` })
    const stays = stayingAt(map, prep, s.x, s.y)
    if (stays) out.push({ cell: s, text: `Slot ${n} at ${s.x + 1},${s.y + 1} is where ${stays.name ?? stays.id ?? stays.character ?? 'a unit'} stands, and that unit stays where it is placed. The game will not load until the slot or the unit moves.` })
    if (slots.findIndex((o) => o.x === s.x && o.y === s.y) !== i) out.push({ cell: s, text: `Slot ${n} is on the same square as an earlier slot.` })
  })

  const known = campaignRosterIds(doc)
  for (const r of required) {
    const id = requiredId(r)
    const n = requiredSlot(r)
    if (doc.campaign && !known.has(id) && !(map.units ?? []).some((u) => storyIdOf(u) === id))
      out.push({ text: `${nameOf(id)} is required here, but no map places a unit or character with that name on a human side, so the party never has them. A required character that is not in the party is simply left out.` })
    if (n !== undefined && (n < 1 || n > slots.length)) out.push({ text: `${nameOf(id)} is fixed to slot ${n}, which does not exist. The game will not load until it is fixed.` })
    else if (n !== undefined) {
      const placed = rosterUnits(doc).find((u) => storyIdOf(u) === id)
      const type = placed ? typeIdOf(c.characters, placed) : c.characters.find((p) => p.id === id)?.type ?? ''
      const s = slots[n - 1]
      if (type && !typeCanStand(map, c, type, s.x, s.y)) out.push({ cell: s, text: `${nameOf(id)} must start on slot ${n}, but that kind of unit cannot stand on ${c.terrain.find((t) => t.id === terrainAt(map, s.x, s.y))?.name ?? 'that square'}. The default pick would leave them out.` })
    }
    if (prep.forbidden?.includes(id)) out.push({ text: `${nameOf(id)} is both required and forbidden. The game will not load until one is removed.` })
    if (prep.fixed?.includes(id)) out.push({ text: `${nameOf(id)} is both required and a fixed unit. The game will not load until one is removed.` })
  }
  for (const id of prep.fixed ?? []) {
    if (!(map.units ?? []).some((u) => storyIdOf(u) === id)) out.push({ text: `"${id}" is listed as a fixed unit but this map does not place a unit with that name. The game will not load until it is removed.` })
  }
  for (const id of prep.forbidden ?? []) {
    const placed = (map.units ?? []).find((u) => storyIdOf(u) === id && isChoosable(map, prep, u))
    if (placed) out.push({ cell: placed, text: `${nameOf(id)} is forbidden, but this map places that unit on the preparation's side. The game will not load until it is removed or no longer forbidden.` })
  }
  if (prep.side !== undefined && prepSide(map, prep) !== prep.side) out.push({ text: `The preparation's side "${prep.side}" does not exist on this map.` })
  const sideDef = (map.sides ?? []).find((s) => s.id === prepSide(map, prep))
  if (!sideDef || sideDef.controller === 'ai') out.push({ text: 'The preparation needs a side that a person plays. The game will not load without one.' })

  if (prep.shop) {
    if (prep.shop.sells.length === 0) out.push({ text: 'The preparation shop sells nothing. The game will not load until it sells something or is removed.' })
    for (const id of prep.shop.sells) {
      const item = doc.items?.find((i) => i.id === id)
      if (!item) out.push({ text: `The preparation shop sells "${id}", which is not an item of this game. The game will not load until it is removed.` })
      else if (!item.price || Object.keys(item.price).length === 0) out.push({ text: `The preparation shop sells ${item.name ?? id}, which has no price (Items). The game will not load until it has one or is removed from the shop.` })
    }
  }

  // Outside a campaign there is no party: the choice is only among the units this map places.
  const chapters = doc.campaign?.chapters ?? []
  if (!doc.campaign) out.push({ text: 'This game has no campaign, so the preparation screen only works as a free battle: the choice is among the units placed on this map, and there is no shared storage or shop money.' })
  else if (!chapters.some((ch) => ch.map === mapId)) out.push({ text: 'This map has a preparation screen but no chapter of the campaign uses it. As a free battle the choice is only among the units placed on it.' })
  return out
}
