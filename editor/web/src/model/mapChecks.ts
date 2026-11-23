// Friendly in-editor checks. They are separate from the helper's "Problems":
// the helper reports what stops the game loading, these point at things that
// load fine but are probably not what the author meant.
import { audioChecks } from './audioChecks'
import { behaviourChecks } from './behaviour'
import { exitProblems } from './exits'
import { typeIdOf } from './characterUnits'
import { mapDifficultyChecks } from './difficultyChecks'
import { fogChecks } from './fogChecks'
import { changeChecks } from './mapChangeChecks'
import { tileChecks, type ImageSize } from './tileChecks'
import { itemChecks } from './itemChecks'
import { placeChecks } from './placeChecks'
import { mapWeaponChecks } from './mapWeapon'
import { storyChecks } from './storyChecks'
import { cutSceneChecks } from './cutScene'
import { prepChecks } from './prepChecks'
import { ctxOf } from './mapCtx'
import { terrainAt } from './mapGrid'
import { misfits, placedOf, structureAt } from './mapStructures'
import { isHeadquarters } from './structures'
import { canEnter } from './structuresEffective'
import type { MapFile, ProjectDoc } from './types'

export interface MapWarning { key: string; text: string; cell?: { x: number; y: number } }

/** `imageSize`: the width and height of a game image (lets the tile numbers of the tile layer be checked against the tileset). */
export function mapWarnings(doc: ProjectDoc, map: MapFile, imageSize?: ImageSize): MapWarning[] {
  const c = ctxOf(doc)
  if (!c) return []
  const out: MapWarning[] = []
  const sides = map.sides ?? []
  const units = map.units ?? []
  const defeat = doc.game?.rules?.defeat ?? ['noUnits']
  const tOf = (id: string | undefined) => c.terrain.find((t) => t.id === id)
  const sOf = (type: string | undefined) => c.structureTypes.find((s) => s.id === type)

  for (const u of units) {
    const t = tOf(terrainAt(map, u.x, u.y))
    const st = sOf(structureAt(map, c, u.x, u.y)?.type)
    const typeId = typeIdOf(c.characters, u)
    const type = c.unitTypes.find((x) => x.id === typeId)
    // Judged the way the game does: the structure's movement costs replace the terrain's where it sets them.
    if (!canEnter(t, st, type)) out.push({ key: `blocked:${u.x},${u.y}`, cell: { x: u.x, y: u.y }, text: `${type?.name ?? typeId} at ${u.x + 1},${u.y + 1} stands on ${st ? `a ${st.name}` : t?.name ?? 'terrain'}, which it cannot enter. The game will not load until it is moved or the ${st ? 'structure' : 'terrain'} changed.` })
  }
  for (const i of misfits(map, c)) {
    const s = placedOf(map)[i]
    out.push({ key: `misfit:${s.x},${s.y}`, cell: { x: s.x, y: s.y }, text: `${sOf(s.type)?.name ?? s.type} at ${s.x + 1},${s.y + 1} does not fit there any more (it sticks out of the map or overlaps another structure). The game will not load until it is moved or removed.` })
  }
  if (sides.length < 2) out.push({ key: 'few-sides', text: 'A battle needs at least two sides.' })

  const producing = doc.game?.rules?.production?.enabled
  for (const s of sides) {
    const hasUnits = units.some((u) => u.side === s.id)
    const owned = placedOf(map).filter((o) => o.side === s.id)
    const canBuild = !!producing && owned.some((o) => sOf(o.type)?.produces?.length)
    if (!hasUnits && !canBuild) out.push({ key: `no-units:${s.id}`, text: `${s.name} has no units and no way to get any${producing ? ' (it owns no structure that makes units)' : ''}.` })
    if (defeat.includes('leaderDead') && hasUnits && !units.some((u) => u.side === s.id && u.leader)) {
      out.push({ key: `no-leader:${s.id}`, text: `${s.name} has no leader, but "its leader is defeated" is a way to lose. Mark one of its units as the leader.` })
    }
    if (defeat.includes('lostHq') && !owned.some((o) => isHeadquarters(sOf(o.type)))) {
      out.push({ key: `no-hq:${s.id}`, text: `${s.name} owns no headquarters, but "its headquarters is captured" is a way to lose. Place a headquarters structure and set ${s.name} as its owner.` })
    }
  }
  // What the loader would refuse about the items on this map (placed units' items, sides' storage).
  const mapId = Object.keys(doc.maps).find((k) => doc.maps[k] === map)
  itemChecks(doc).filter((x) => mapId !== undefined && x.mapId === mapId).forEach((x, i) => out.push({ key: `items:${i}`, text: x.text }))
  // Doors, chests, places and the computer's goals: what the loader refuses and what is probably not meant (docs/SRPG.md section 10).
  placeChecks(doc).filter((x) => mapId !== undefined && x.mapId === mapId).forEach((x, i) => out.push({ key: `places:${i}`, text: x.text, ...(x.cell ? { cell: x.cell } : {}) }))
  // Map weapons: a placed structure's own uses (docs/SRPG.md "Map weapons").
  mapWeaponChecks(doc).filter((x) => mapId !== undefined && x.mapId === mapId).forEach((x, i) => out.push({ key: `mapweapon:${i}`, text: x.text, ...(x.cell ? { cell: x.cell } : {}) }))
  // The computer units' behaviours (docs/AI.md): what the loader refuses and what is probably not meant.
  if (mapId !== undefined) behaviourChecks(doc, mapId, map).forEach((x, i) => out.push({ key: `behaviour:${i}`, text: x.text, cell: x.cell }))
  exitProblems(map).forEach((t, i) => out.push({ key: `exits:${i}`, text: t }))
  fogChecks(doc).filter((x) => mapId !== undefined && x.mapId === mapId).forEach((x, i) => out.push({ key: `fog:${i}`, text: x.text }))
  // The map's own difficulty numbers and its units' "only on" lists (docs/DIFFICULTY.md).
  if (mapId !== undefined) mapDifficultyChecks(doc, mapId, map).forEach((x, i) => out.push({ key: `difficulty:${i}`, text: x.text, ...(x.cell ? { cell: x.cell } : {}) }))
  // Named map changes and the fog options of the map (model/mapChangeChecks.ts); notes the loader does not give are marked as such.
  if (mapId !== undefined) changeChecks(doc, mapId).forEach((x, i) => out.push({ key: `changes:${i}`, text: x.text }))
  // The tile layer and the tiles changes and commands carry (model/tileChecks.ts).
  if (mapId !== undefined) tileChecks(doc, mapId, imageSize).forEach((x, i) => out.push({ key: `tiles:${i}`, text: x.text }))
  // What the player's sound loader would refuse about this map's music keys.
  audioChecks(doc).filter((x) => mapId !== undefined && x.mapId === mapId).forEach((x, i) => out.push({ key: `audio:${i}`, text: x.text }))
  // What the loader would refuse about this map's story events, objectives and unit story names.
  if (mapId !== undefined) storyChecks(doc, mapId).forEach((x, i) => out.push({ key: `story:${i}`, text: x.text }))
  // A map only story scenes show has no events, objectives or preparation of its own.
  if (mapId !== undefined) cutSceneChecks(doc, mapId).forEach((t, i) => out.push({ key: `cutscene:${i}`, text: t }))
  // The preparation screen: what the loader refuses about it, and what is probably not meant (docs/EVENTS.md "Preparation").
  if (mapId !== undefined) prepChecks(doc, mapId).forEach((x, i) => out.push({ key: `prep:${i}`, text: x.text, ...(x.cell ? { cell: x.cell } : {}) }))
  return out
}
