// Checks that mirror what the game loader refuses about a map's events and objectives (engine/core/data/loader_events.cpp
// and loader_story_*.cpp), so a maker sees them in the editor before pressing Play. The wording is plain; the loader's
// own messages still show in the Problems list after a save. Plus two hints the loader does not give (a portrait with no
// picture, an `if` with no conditions).
import { sceneProblems } from './sceneChecks'
import { fightProblems } from './storyFight'
import { audioRefProblem } from './audioChecks'
import { mapChangeChecks } from './mapChangeStory'
import { ACTOR_NOBODY, actorWithoutActor, storeItemsProblems, triggerHasActor } from './storyExtras'
import { blockProblems, groupNames } from './behaviour'
import { choiceChecks, storeCounts } from './choiceChecks'
import { inventorySize } from './itemRules'
import { readingProblems } from './readingChecks'
import { addSupportChecks, supportConditionChecks, supportTriggerChecks } from './supportStoryChecks'
import { definedUnitIds, isStoryId, teamsOf } from './storyIds'
import { conditionKind, rectOf, commandKind, triggerKind, type Command, type Condition, type ObjectiveRule, type Objectives, type StoryEvent } from './storyTypes'
import { forEachCommand, walkStory, type Ref } from './storyWalk'
import type { MapFile, ProjectDoc } from './types'

export interface StoryCheck { text: string; eventId?: string }

const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)
const BIG = 1_000_000_000

function refProblem(doc: ProjectDoc, map: MapFile, defined: Set<string>, r: Ref): string | null {
  const known = (list: Array<{ id: string }> | undefined) => !!list?.some((x) => x.id === r.id)
  const missing = (what: string) => (r.id === '' ? `pick ${what}.` : null)
  switch (r.kind) {
    case 'unit': return missing('a unit') ?? (defined.has(r.id) ? null : `“${r.id}” is not a named unit of this map (give a unit that story name in the Units tool, or add it with an “Add a unit” command).`)
    case 'side': return missing('a side') ?? (known(map.sides) ? null : `“${r.id}” is not a side of this map.`)
    case 'team': return missing('a team') ?? (teamsOf(map).some((t) => t.id === r.id) ? null : `“${r.id}” is not a team of this map.`)
    case 'teamOrSide': return missing('a team') ?? (teamsOf(map).some((t) => t.id === r.id) || known(map.sides) && map.sides!.some((s) => s.id === r.id) ? null : `“${r.id}” is not a team or side of this map.`)
    case 'type': return missing('a unit type') ?? (known(doc.units ?? undefined) ? null : `“${r.id}” is not a unit type of this game.`)
    case 'item': return missing('an item') ?? (known(doc.items ?? undefined) ? null : `“${r.id}” is not an item of this game.`)
    case 'resource': return missing('a resource') ?? (known(doc.game?.resources) ? null : `“${r.id}” is not a resource of this game.`)
    case 'switch': return missing('a switch') ?? (known(doc.game?.switches) ? null : `“${r.id}” is not a switch of this game.`)
    case 'variable': return missing('a variable') ?? (known(doc.game?.variables) ? null : `“${r.id}” is not a variable of this game.`)
    case 'difficulty': return missing('a difficulty') ?? (known(doc.game?.difficulties?.list) ? null : `“${r.id}” is not a difficulty of this game${doc.game?.difficulties ? '' : ' (the game has no difficulties yet: add them on the Rules screen)'}.`)
    case 'stat': return missing('a stat') ?? (known(doc.game?.stats) ? null : `“${r.id}” is not a stat of this game.`)
    case 'tech': return missing('a technology') ?? (known(doc.research ?? undefined) ? null : `“${r.id}” is not a technology of this game${doc.research?.length ? '' : ' (the game has no research yet)'}.`)
    case 'skill': return missing('a skill') ?? (known(doc.skills ?? undefined) ? null : `“${r.id}” is not a skill of this game${doc.skills?.length ? '' : ' (the game has no skills yet)'}.`)
    case 'condition': return missing('a condition') ?? (known(doc.conditions ?? undefined) ? null : `“${r.id}” is not a status condition of this game${doc.conditions?.length ? '' : ' (the game has no conditions yet)'}.`)
    case 'music':
    case 'sound': return audioRefProblem(doc, r.kind, r.id)
    case 'pair': return null // supportStoryChecks.ts says it in plainer words
    case 'character': return missing('a character') ?? (known(doc.characters ?? undefined) ? null : `“${r.id}” is not a character of this game.`)
    case 'chapter':
    case 'party':
    case 'map': return null // only campaigns name these (campaignChecks.ts)
    case 'change': return missing('a change') ?? (Object.keys(map.changes ?? {}).includes(r.id) ? null : `“${r.id}” is not one of this map’s changes.`)
    case 'terrain': return missing('a terrain') ?? (known(doc.terrain ?? undefined) ? null : `“${r.id}” is not a terrain of this game.`)
    case 'place': return null // placeChecks.ts says it in plainer words (the loader's rules differ per trigger and command)
    case 'background': return null // sceneChecks.ts says it with the command's own words
    case 'portrait': return null // just text for the player to look up: an id with no picture shows the box without one
    case 'weapon': return null // a staged fight's weapon: storyFight.ts (fightProblems) says it
  }
}

const outside = (map: MapFile, r: { x: number; y: number; w: number; h: number }) =>
  r.x < 0 || r.y < 0 || r.x + r.w > map.grid.width || r.y + r.h > map.grid.height

function ruleChecks(map: MapFile, r: ObjectiveRule, where: string, win: boolean, say: (w: string, t: string) => void): void {
  if (r.reach !== undefined) {
    const a = rectOf(r.reach)
    if (a.w < 1 || a.h < 1 || outside(map, a)) say(where, 'an area does not fit inside the map.')
  }
  for (const k of ['round', 'survive'] as const) {
    const n = r[k]
    if (n !== undefined && (!Number.isInteger(n) || n < 1 || n > 100000)) say(where, 'a number of rounds must be from 1 to 100000.')
  }
  if (r.survive !== undefined && !win) say(where, '“Survive” is a way to win, not to lose.')
}

function objectivesChecks(map: MapFile, o: Objectives, where: string, say: (w: string, t: string) => void): void {
  for (const k of ['win', 'lose'] as const)
    (o[k] ?? []).forEach((r, i) => ruleChecks(map, r, `${where}, “${k === 'win' ? 'Win' : 'Lose'}” entry ${i + 1}`, k === 'win', say))
}

function commandChecks(doc: ProjectDoc, map: MapFile, e: StoryEvent, c: Command, where: string, say: (w: string, t: string) => void, ids: Map<string, number>, stores: Map<string, number>): void {
  const talk = triggerKind(e.when ?? {}) === 'talk'
  for (const t of sceneProblems(doc, c, false)) say(where, t)  // scene presentation (sceneChecks.ts)
  switch (commandKind(c)) {
    case 'say':
      if (!String(c.say ?? '').trim()) say(where, 'the text to say is empty.')
      if (typeof c.portrait === 'string' && c.portrait && !doc.graphics?.portraits?.[c.portrait]) say(where, `the portrait “${c.portrait}” has no picture yet (Art, Portraits).`)
      break
    case 'choice':
      choiceChecks(c, where, stores, say)
      if (typeof c.portrait === 'string' && c.portrait && !doc.graphics?.portraits?.[c.portrait]) say(where, `the portrait “${c.portrait}” has no picture yet (Art, Portraits).`)
      ;(c.answers ?? []).forEach((a, i) => {
        seesChecks(map, a.if, `${where}, answer ${i + 1}`, say)
        supportConditionChecks(doc, a.if, `${where}, answer ${i + 1}`, say)
      })
      break
    case 'addUnit': {
      const u = c.addUnit
      if (!u) break
      if (typeof u.x !== 'number' || typeof u.y !== 'number' || outside(map, { x: u.x, y: u.y, w: 1, h: 1 })) say(where, 'the square is outside the map.')
      if (u.id !== undefined && u.id !== '' && !isStoryId(u.id)) say(where, 'the story name may use only letters, digits, _ and -.')
      else if (u.id && (ids.get(u.id) ?? 0) > 1) say(where, `the story name “${u.id}” is used by more than one unit of the map.`)
      if (Array.isArray(u.items) && u.items.length > inventorySize(doc)) say(where, `${u.items.length} items, but a unit can carry only ${inventorySize(doc)}.`)
      if (typeof u.level === 'number' && u.level < 1) say(where, 'the level must be at least 1.')
      if (u.behaviour !== undefined) blockProblems(u.behaviour, doc, map, (t) => say(where, t), { human: map.sides?.some((s) => s.id === u.side && s.controller !== 'ai') })
      break
    }
    case 'fight':
      for (const t of fightProblems(doc, c, [...ids.keys()])) say(where, t)
      break
    case 'showUnit':
      if (typeof c.x !== 'number' || typeof c.y !== 'number' || outside(map, { x: c.x, y: c.y, w: 1, h: 1 })) say(where, 'the square is outside the map.')
      break
    case 'removeUnit':
      if (c.hide !== undefined && typeof c.hide !== 'boolean') say(where, '“Only hide it” must be on or off.')
      break
    case 'moveUnit':
      if (typeof c.x !== 'number' || typeof c.y !== 'number' || outside(map, { x: c.x, y: c.y, w: 1, h: 1 })) say(where, 'the square is outside the map.')
      break
    case 'focus':
      if (typeof c.focus === 'object' && c.focus && outside(map, { ...rectOf(c.focus), w: 1, h: 1 })) say(where, 'the square is outside the map.')
      break
    case 'giveResource':
      if (!Object.keys(c.giveResource ?? {}).length) say(where, 'choose a resource to give.')
      else if (Object.values(c.giveResource!).some((n) => Math.abs(n) > BIG)) say(where, 'an amount is unreasonably large.')
      break
    case 'setVariable':
    case 'addVariable':
      if (Math.abs(Number(c.to ?? c.by ?? 0)) > BIG) say(where, 'the number is unreasonably large.')
      for (const p of readingProblems(c, false)) say(where, p)
      break
    case 'playMusic':
    case 'stopMusic':
      if (c.fade !== undefined && (typeof c.fade !== 'number' || c.fade < 0 || c.fade > 60)) say(where, 'the fade is a number of seconds from 0 to 60.')
      break
    case 'endTurn':
      if (!talk) say(where, '“End the turn” only works in an event whose “When” is two units talking.')
      break
    case 'objective':
      if (c.objective) objectivesChecks(map, c.objective, where, say)
      break
    case 'if':
      if (!c.if?.length) say(where, 'this “If” has no conditions, so it always happens.')
      break
    case 'addSupport':
      addSupportChecks(doc, c, where, say)
      break
    case 'addCondition': {
      if (typeof c.to !== 'object' || c.to === null) say(where, 'pick which units get the condition.')
      if (c.turns !== undefined) {
        const def = doc.conditions?.find((x) => x.id === c.addCondition)
        if (!Number.isInteger(c.turns) || c.turns < 1 || c.turns > 99) say(where, 'the turns must be a whole number from 1 to 99.')
        else if (def && def.until !== undefined) say(where, `${def.name} does not count turns (it lasts until it is cured or the map ends), so the turns mean nothing. Remove them.`)
      }
      break
    }
    case 'removeCondition':
      if (typeof c.from !== 'object' || c.from === null) say(where, 'pick which units are cured.')
      break
    case 'setBehaviour':
      if (typeof c.units !== 'object' || c.units === null) say(where, 'pick which units get the behaviour.')
      else if (c.units.side !== undefined && map.sides?.some((s) => s.id === c.units!.side && s.controller !== 'ai')) say(where, 'that side is played by a person; only the computer’s units have a behaviour.')
      blockProblems(c.setBehaviour, doc, map, (t) => say(where, t))
      break
    case 'setTerrain': case 'applyChange': case 'revertChange': case 'setFog': case 'setVision':
      mapChangeChecks(map, c, where, say)
      break
    case 'storeItems':
      for (const t of storeItemsProblems(doc, c)) say(where, t)
      break
    case 'switchParty':
    case 'mergeParties':
      say(where, 'party commands are for campaign scenes only (a battle on a map knows one party: the one playing it).')
      break
    case 'wakeGroup':
      if (!c.wakeGroup) say(where, 'pick the group to wake.')
      else if (!groupNames(map).includes(c.wakeGroup)) say(where, `no unit of this map is in a group called “${c.wakeGroup}”.`)
      break
  }
}

/** What the loader refuses about a `sees` condition beyond the names it points at, plus the hint for a map without fog. */
function seesChecks(map: MapFile, list: Condition[] | undefined, where: string, say: (w: string, t: string) => void): void {
  list?.forEach((c, i) => {
    if (conditionKind(c) !== 'sees') return
    const at = `${where}, condition ${i + 1}`
    if (typeof c.team !== 'string') say(at, 'pick the team that must see it.')
    if (c.sees === null || typeof c.sees !== 'object') say(at, 'pick which unit must be seen.')
    if (map.fog !== true) say(at, 'this map has no fog of war, so every team sees every living unit and this is true whenever such a unit is alive. Switch fog on in the map’s settings.')
  })
}

/** Everything wrong in one map's events, objectives and unit names; empty when all is well. */
export function storyChecks(doc: ProjectDoc, mapId: string): StoryCheck[] {
  const map = doc.maps[mapId]
  if (!map || (!map.events && !map.objectives && !(map.units ?? []).some((u) => u.id))) return []
  const out: StoryCheck[] = []
  let current: string | undefined
  const say = (where: string, text: string) => out.push({ text: `${cap(where)}: ${text}`, eventId: current })

  // Story names of units: valid, and each used once among placed and added units.
  const ids = new Map<string, number>()
  const count = (id: string | undefined) => { if (id) ids.set(id, (ids.get(id) ?? 0) + 1) }
  for (const u of map.units ?? []) count(u.id)
  for (const ev of map.events ?? []) forEachCommand(ev.do, '', (c) => count(c.addUnit?.id))
  for (const u of map.units ?? []) {
    if (u.id === undefined) continue
    const where = `the unit at ${u.x + 1}, ${u.y + 1}`
    if (!isStoryId(u.id)) say(where, 'the story name may use only letters, digits, _ and - (up to 64).')
    else if ((ids.get(u.id) ?? 0) > 1) say(where, `the story name “${u.id}” is used by more than one unit.`)
  }

  const stores = storeCounts((map.events ?? []).map((ev) => ev.do))
  const defined = new Set(definedUnitIds(doc, map))
  const seen = new Set<string>()
  for (const e of map.events ?? []) {
    current = e.id
    const where = `event “${e.id}”`
    if (!isStoryId(e.id)) say(where, 'the name may use only letters, digits, _ and - (up to 64).')
    if (seen.has(e.id)) say(where, 'another event has the same name.')
    seen.add(e.id)
    const trig = e.when ?? {}
    const kind = triggerKind(trig)
    if (!kind) say(where, 'it has no “When”.')
    if (kind === 'turn' && (!Number.isInteger(trig.turn) || (trig.turn as number) < 1 || (trig.turn as number) > 100000)) say(`${where}, “When”`, 'the turn must be a whole number from 1 to 100000.')
    if (kind === 'reach') {
      const a = rectOf(trig.reach)
      if (a.w < 1 || a.h < 1 || outside(map, a)) say(`${where}, “When”`, 'the area does not fit inside the map.')
    }
    if (kind === 'talk' && trig.talk && trig.talk === trig.to) say(`${where}, “When”`, 'a unit cannot talk to itself.')
    forEachCommand(e.do, where, (c, w) => commandChecks(doc, map, e, c, w, say, ids, stores))
    seesChecks(map, e.if, `${where}, “Only if”`, say)
    forEachCommand(e.do, where, (c, w) => seesChecks(map, c.if, w, say))
    supportTriggerChecks(doc, trig, `${where}, “When”`, say)
    supportConditionChecks(doc, e.if, `${where}, “Only if”`, say)
    let asksActor = actorWithoutActor(e.if)
    forEachCommand(e.do, where, (c) => { if (actorWithoutActor(c.if)) asksActor = true })
    if (kind && !triggerHasActor(kind) && asksActor) say(where, ACTOR_NOBODY)
    forEachCommand(e.do, where, (c, w) => supportConditionChecks(doc, c.if, w, say))
  }

  // Do the names these events point at exist? (one walk for every kind of name)
  walkStory(map, {
    ref: (r, where) => {
      const problem = refProblem(doc, map, defined, r)
      if (problem) {
        const ev = /^event “(.*?)”/.exec(where)
        current = ev?.[1]
        say(where, problem)
      }
      return r.id
    },
  })
  current = undefined
  if (map.objectives) objectivesChecks(map, map.objectives, 'the objectives', say)
  return out
}
