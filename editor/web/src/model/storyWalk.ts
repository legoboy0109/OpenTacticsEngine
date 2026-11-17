// One walk over everything a map's events and objectives NAME (units, sides, teams, unit types, items, resources,
// switches, variables). The same walk serves four jobs, so the knowledge of where ids sit is written once:
//   - rename a unit's story id (the callback returns a new id),
//   - clean up after a delete (the callback returns DROP; the smallest thing that cannot work any more is removed:
//     an event, a command, an objective entry, one resource of a gift, one item of an added unit),
//   - list what would be removed ("used by", `dropped` gets a sentence for each),
//   - check that every name exists (the callback gets `where`, a plain place such as `event “ambush”, command 2`).
// A walk returns the very same objects when nothing changed, so callers can tell cheaply.
import { entryId } from './itemGrants'
import type { ChoiceAnswer, Command, Condition, Filter, ObjectiveRule, Objectives, StoryEvent, Trigger } from './storyTypes'
import type { ItemEntry, MapFile } from './types'
import { withKeepList } from './storyExtras'

/** `teamOrSide`: a win / lose names a team or, as a shorthand, a side id. */
/** `chapter` and `map` only occur in a campaign (campaignWalk.ts); `goto` in a scene names a chapter. */
export type RefKind = 'unit' | 'character' | 'pair' | 'side' | 'team' | 'teamOrSide' | 'type' | 'item' | 'resource' | 'switch' | 'variable' | 'chapter' | 'map' | 'tech' | 'music' | 'sound' | 'skill' | 'stat' | 'condition' | 'place' | 'portrait' | 'change' | 'terrain' | 'background' | 'difficulty'
export interface Ref { kind: RefKind; id: string }
export const DROP = Symbol('drop')
export type Dropped = typeof DROP

export interface Dropping { what: 'event' | 'command' | 'rule' | 'resource' | 'item' | 'chapterMap' | 'link' | 'freeBattle' | 'setting' | 'difficulty'; text: string; where: string }

export interface Walk {
  /** Called for every place that names something. Return the id to keep there (the same, or a new one) or DROP. */
  ref(r: Ref, where: string): string | Dropped
  /** Called for each thing that is removed; `text` is its plain sentence (from `describe`). */
  dropped?(info: Dropping): void
  describe?: { event?(e: StoryEvent): string; command?(c: Command): string; rule?(r: ObjectiveRule): string }
}

/** Copy of `o` with key `k` set, or `o` itself when the value is already that. */
export function put<T extends object>(o: T, k: string, v: unknown): T {
  const rec = o as Record<string, unknown>
  return rec[k] === v ? o : ({ ...rec, [k]: v } as T)
}

function mapFilter(f: Filter | string, w: Walk, where: string): Filter | string | Dropped {
  if (typeof f === 'string') return w.ref({ kind: 'unit', id: f }, where)
  for (const k of ['unit', 'side', 'team', 'type'] as const) {
    const v = f[k]
    if (typeof v !== 'string') continue
    const r = w.ref({ kind: k, id: v }, where)
    return r === DROP ? DROP : put(f, k, r)
  }
  return f
}

/** Maps the filter held under `key` (if any); DROP when it cannot stay. */
function withFilter<T extends object>(o: T, key: string, w: Walk, where: string): T | Dropped {
  const f = (o as Record<string, unknown>)[key] as Filter | string | undefined
  if (f === undefined || f === null || (typeof f !== 'string' && typeof f !== 'object')) return o
  const r = mapFilter(f, w, where)
  return r === DROP ? DROP : put(o, key, r)
}

export function withRef<T extends object>(o: T, key: string, kind: RefKind, w: Walk, where: string): T | Dropped {
  const v = (o as Record<string, unknown>)[key]
  if (typeof v !== 'string') return o
  const r = w.ref({ kind, id: v }, where)
  return r === DROP ? DROP : put(o, key, r)
}

/** Runs the steps in order on `o`; stops at the first DROP. */
export function chain<T extends object>(o: T, steps: Array<(x: T) => T | Dropped>): T | Dropped {
  let cur = o
  for (const s of steps) {
    const r = s(cur)
    if (r === DROP) return DROP
    cur = r
  }
  return cur
}

function mapTrigger(t: Trigger, w: Walk, where: string): Trigger | Dropped {
  return chain(t, [
    (x) => withRef(x, 'side', 'side', w, where),
    (x) => withFilter(x, 'by', w, where),
    (x) => withFilter(x, 'defeated', w, where),
    (x) => withFilter(x, 'fight', w, where),
    (x) => withFilter(x, 'with', w, where),
    (x) => withFilter(x, 'promoted', w, where),
    (x) => withPair(x, 'support', w, where),
    (x) => withRef(x, 'talk', 'unit', w, where),
    (x) => withRef(x, 'to', 'unit', w, where),
    (x) => withRef(x, 'place', 'place', w, where), // a door, chest or place by its story name (docs/SRPG.md section 10)
  ])
}

function mapCondition(c: Condition, w: Walk, where: string): Condition | Dropped {
  return chain(c, [
    (x) => withRef(x, 'switch', 'switch', w, where),
    (x) => withRef(x, 'variable', 'variable', w, where),
    (x) => withFilter(x, 'alive', w, where),
    (x) => withFilter(x, 'dead', w, where),
    (x) => withFilter(x, 'sees', w, where),
    (x) => withPair(x, 'support', w, where),
    (x) => withRef(x, 'team', 'teamOrSide', w, where), // only a `sees` condition has one (a side id means that side's team)
    (x) => withRef(x, 'carries', 'item', w, where),
    (x) => withRef(x, 'researched', 'tech', w, where),
    (x) => withRef(x, 'hasSkill', 'skill', w, where),
    (x) => withRef(x, 'hasCondition', 'condition', w, where),
    (x) => withFilter(x, 'actor', w, where),
    (x) => withDifficultyCondition(x, w, where),
    (x) => withRef(x, 'side', 'side', w, where), // only a `researched` condition has one
    (x) => withFilter(x, 'unit', w, where),
  ])
}

/** A `difficulty` condition (docs/DIFFICULTY.md): a difficulty id, or { atLeast / atMost: id }. A deleted difficulty takes the condition with it. */
function withDifficultyCondition<T extends object>(o: T, w: Walk, where: string): T | Dropped {
  const v = (o as Record<string, unknown>).difficulty
  if (typeof v === 'string') return withRef(o, 'difficulty', 'difficulty', w, where)
  if (v === null || typeof v !== 'object' || Array.isArray(v)) return o
  const bound = v as Record<string, unknown>
  for (const k of ['atLeast', 'atMost']) {
    const id = bound[k]
    if (typeof id !== 'string') continue
    const r = w.ref({ kind: 'difficulty', id }, where)
    if (r === DROP) return DROP
    return r === id ? o : put(o, 'difficulty', { ...bound, [k]: r })
  }
  return o
}

/** An added unit's "only on" difficulties: a deleted one leaves the list; a list left empty is no limit any more (the unit stays and
 *  appears on every difficulty), which the sentence for the delete question says. */
function mapDifficultyList(list: unknown[], w: Walk, where: string, owner: Command): string[] | undefined {
  const ids = list.filter((x): x is string => typeof x === 'string')
  const kept = ids.filter((id) => w.ref({ kind: 'difficulty', id }, where) !== DROP)
  if (kept.length === ids.length) return list as string[]
  const base = w.describe?.command?.(owner) ?? 'An added unit'
  w.dropped?.({ what: 'difficulty', text: kept.length ? base : `${base} (it then appears on every difficulty)`, where })
  return kept.length ? kept : undefined
}

/** A pair of characters (`[a, b]`, as `addSupport` and the `support` condition / trigger hold it): both must stay. Characters are
 *  named by their own kind of reference ('character'): a pair is not about units standing on this map. */
function withPair<T extends object>(o: T, key: string, w: Walk, where: string): T | Dropped {
  const v = (o as Record<string, unknown>)[key]
  if (!Array.isArray(v) || v.length !== 2 || typeof v[0] !== 'string' || typeof v[1] !== 'string') return o
  const a = w.ref({ kind: 'character', id: v[0] }, where)
  const b = a === DROP ? DROP : w.ref({ kind: 'character', id: v[1] }, where)
  if (a === DROP || b === DROP) return DROP
  // The pair itself: its id is the two characters in alphabetical order, so either order of the file names the same pair. A walk
  // may drop it (the pair was deleted) or answer with another pair's id (the pair now has other characters).
  const id = pairId(a, b)
  const p = w.ref({ kind: 'pair', id }, where)
  if (p === DROP) return DROP
  if (p !== id) { const [x, y] = p.split('.'); return put(o, key, [x, y]) }
  return a === v[0] && b === v[1] ? o : put(o, key, [a, b])
}

/** The id of a support pair as the walk's 'pair' reference names it: both character ids, alphabetical, joined by a dot (ids hold no dot). */
export const pairId = (a: string, b: string): string => (a < b ? `${a}.${b}` : `${b}.${a}`)

/** All conditions must survive: a condition that cannot be kept takes the whole list (and its owner) with it. */
function mapConditions(list: Condition[], w: Walk, where: string): Condition[] | Dropped {
  const out: Condition[] = []
  let changed = false
  for (const c of list) {
    const r = mapCondition(c, w, where)
    if (r === DROP) return DROP
    if (r !== c) changed = true
    out.push(r)
  }
  return changed ? out : list
}

function mapRule(r: ObjectiveRule, w: Walk, where: string): ObjectiveRule | Dropped {
  return chain(r, [(x) => withFilter(x, 'defeated', w, where), (x) => withFilter(x, 'by', w, where)])
}

/** `key` is "win" or "lose": a rule that cannot stay is removed from the list on its own. */
function mapRules(list: ObjectiveRule[], w: Walk, where: string): ObjectiveRule[] {
  const out: ObjectiveRule[] = []
  let changed = false
  for (const rule of list) {
    const r = mapRule(rule, w, where)
    if (r === DROP) { changed = true; w.dropped?.({ what: 'rule', text: w.describe?.rule?.(rule) ?? 'An objective', where }); continue }
    if (r !== rule) changed = true
    out.push(r)
  }
  return changed ? out : list
}

/** An emptied win / lose list loses its key: "no rules" is written as no list. */
export function mapObjectives(o: Objectives, w: Walk, where: string): Objectives {
  let out = o
  for (const k of ['win', 'lose'] as const) {
    const list = o[k]
    if (!Array.isArray(list)) continue
    const next = mapRules(list, w, `${where}, “${k === 'win' ? 'Win' : 'Lose'}”`)
    if (next === list) continue
    if (next.length) out = put(out, k, next)
    else { const { [k]: _gone, ...rest } = out; out = rest as Objectives }
  }
  return out
}

/** An added unit's items: the ones that no longer exist leave its list (the unit stays). */
function mapItems(list: ItemEntry[], w: Walk, where: string, owner: Command): ItemEntry[] {
  const kept = list.filter((e) => w.ref({ kind: 'item', id: entryId(e) }, where) !== DROP)
  if (kept.length !== list.length) w.dropped?.({ what: 'item', text: w.describe?.command?.(owner) ?? 'An added unit', where })
  return kept.length === list.length ? list : kept
}

/** A behaviour block's unit and place names (docs/AI.md): the unit it heads for, the one it prefers, the structure it heads for. A name that cannot stay
 *  takes the command with it, as every other command naming a deleted thing. */
function mapBehaviourBlock<T extends object>(holder: T, key: string, w: Walk, where: string): T | Dropped {
  const b = (holder as Record<string, unknown>)[key]
  if (b === null || typeof b !== 'object' || Array.isArray(b)) return holder
  const o = b as Record<string, unknown>
  const stepped = chain(o, [
    (x) => withRef(x, 'unit', 'unit', w, where),
    (x) => withRef(x, 'place', 'place', w, where),
    (x) => { const p = x.prefer; return p !== null && typeof p === 'object' && 'unit' in p ? (() => { const r = withRef(p as Record<string, unknown>, 'unit', 'unit', w, where); return r === DROP ? DROP : put(x, 'prefer', r) })() : x },
  ])
  return stepped === DROP ? DROP : put(holder, key, stepped)
}

function mapBehaviour(c: Command, w: Walk, where: string): Command | Dropped { return mapBehaviourBlock(c, 'setBehaviour', w, where) }

function mapAddUnit(c: Command, w: Walk, where: string): Command | Dropped {
  const u = c.addUnit
  if (!u || typeof u !== 'object') return c
  const stepped0 = chain(u, [(x) => withRef(x, 'type', 'type', w, where), (x) => withRef(x, 'side', 'side', w, where), (x) => withRef(x, 'character', 'character', w, where)]) // a character joins by its id
  if (stepped0 === DROP) return DROP
  const stepped = mapBehaviourBlock(stepped0 as Record<string, unknown>, 'behaviour', w, where) as typeof stepped0 | Dropped
  if (stepped === DROP) return DROP
  const items = Array.isArray(stepped.items) ? mapItems(stepped.items, w, where, c) : stepped.items
  let next = items === stepped.items ? stepped : { ...stepped, items }
  if (Array.isArray(next.difficulty)) {
    const only = mapDifficultyList(next.difficulty, w, where, c)
    if (only !== next.difficulty) {
      const { difficulty: _old, ...rest } = next
      next = only === undefined ? rest : { ...rest, difficulty: only }
    }
  }
  return next === u ? c : { ...c, addUnit: next }
}

function mapGiveResource(c: Command, w: Walk, where: string): Command | Dropped {
  const amounts = c.giveResource
  if (!amounts || typeof amounts !== 'object') return c
  const kept = Object.entries(amounts).filter(([id]) => w.ref({ kind: 'resource', id }, where) !== DROP)
  if (kept.length === Object.keys(amounts).length) return c
  if (!kept.length) return DROP
  w.dropped?.({ what: 'resource', text: w.describe?.command?.(c) ?? 'A gift', where })
  return { ...c, giveResource: Object.fromEntries(kept) }
}

/** One command, not looking inside `if` branches (mapCommands does that so each inner command can go on its own). */
function mapCommand(c: Command, w: Walk, where: string): Command | Dropped {
  if (c.addUnit !== undefined) return mapAddUnit(c, w, where)
  if (c.giveResource !== undefined) {
    const g = mapGiveResource(c, w, where)
    return g === DROP ? DROP : withRef(g, 'side', 'side', w, where)
  }
  if (c.objective !== undefined && typeof c.objective === 'object') {
    const o = mapObjectives(c.objective, w, where)
    return o === c.objective ? c : { ...c, objective: o }
  }
  if (Array.isArray(c.if)) {
    const conds = mapConditions(c.if, w, where)
    return conds === DROP ? DROP : put(c, 'if', conds)
  }
  return chain(c, [
    (x) => withRef(x, 'speaker', 'unit', w, where),
    (x) => withRef(x, 'portrait', 'portrait', w, where), // a line's own portrait id (a say or a choice); renaming a character's portrait follows here
    (x) => withRef(x, 'store', 'variable', w, where), // a choice's variable
    (x) => withFilter(x, 'removeUnit', w, where),
    (x) => withRef(x, 'moveUnit', 'unit', w, where),
    (x) => withFilter(x, 'changeSide', w, where),
    (x) => withRef(x, 'promote', 'unit', w, where),
    (x) => withPair(x, 'addSupport', w, where),
    (x) => (x.promote !== undefined ? withRef(x, 'to', 'type', w, where) : x), // promote's `to` is a unit type id
    (x) => withRef(x, 'teachSkill', 'skill', w, where),
    (x) => withRef(x, 'forgetSkill', 'skill', w, where),
    (x) => withRef(x, 'addCondition', 'condition', w, where),
    (x) => (x.removeCondition === 'all' ? x : withRef(x, 'removeCondition', 'condition', w, where)),
    (x) => (typeof x.addCondition === 'string' && typeof x.to === 'string' ? withRef(x, 'to', 'unit', w, where) : x), // addCondition's `to`: a unit id
    (x) => (x.setBehaviour !== undefined ? mapBehaviour(x, w, where) : x), // docs/AI.md: who it heads for or prefers
    (x) => (x.setBehaviour !== undefined ? withFilter(x, 'units', w, where) : x),
    (x) => withFilter(x, 'from', w, where),
    (x) => withRef(x, 'side', 'side', w, where),
    (x) => withRef(x, 'giveItem', 'item', w, where),
    (x) => withRef(x, 'takeItem', 'item', w, where),
    (x) => withRef(x, 'grantResearch', 'tech', w, where),
    (x) => withRef(x, 'playMusic', 'music', w, where),
    (x) => withRef(x, 'playSound', 'sound', w, where),
    (x) => withRef(x, 'background', 'background', w, where), // a scene background (null = clear it: not a name)
    (x) => (x.giveItem !== undefined || x.teachSkill !== undefined || (typeof x.to === 'object' && x.to !== null) ? withFilter(x, 'to', w, where) : x), // giveItem's and teachSkill's `to` is a filter: a bare unit id or { unit / side / team / type }
    (x) => withRef(x, 'setSwitch', 'switch', w, where),
    (x) => withRef(x, 'level', 'unit', w, where), // a variable command that reads a unit
    (x) => withRef(x, 'hp', 'unit', w, where),
    (x) => withRef(x, 'maxHp', 'unit', w, where),
    (x) => withRef(x, 'of', 'unit', w, where),
    (x) => withRef(x, 'stat', 'stat', w, where),
    (x) => withRef(x, 'setVariable', 'variable', w, where),
    (x) => withRef(x, 'addVariable', 'variable', w, where),
    (x) => withRef(x, 'win', 'teamOrSide', w, where),
    (x) => withRef(x, 'lose', 'teamOrSide', w, where),
    (x) => (typeof x.focus === 'string' ? withRef(x, 'focus', 'unit', w, where) : x),
    (x) => withRef(x, 'goto', 'chapter', w, where),
    (x) => withRef(x, 'openDoor', 'place', w, where),
    (x) => withRef(x, 'closeDoor', 'place', w, where),
    (x) => withRef(x, 'lockDoor', 'place', w, where),
    (x) => withRef(x, 'unlockDoor', 'place', w, where),
    (x) => withRef(x, 'openChest', 'place', w, where),
    (x) => (x.openChest !== undefined ? withRef(x, 'by', 'unit', w, where) : x), // the unit that gets the contents
    (x) => withRef(x, 'setTerrain', 'terrain', w, where),
    (x) => withRef(x, 'applyChange', 'change', w, where),
    (x) => withRef(x, 'revertChange', 'change', w, where),
    (x) => withFilter(x, 'storeItems', w, where), // a filter in a map event, a roster id in a scene
    (x) => withKeepList(x, w, where), // the items it leaves with the unit
  ])
}

/** A choice's answers: a hiding condition that cannot stay takes its answer with it (the check then says the choice is short of
 *  answers); the commands inside go on one by one. */
function mapAnswers(c: Command, w: Walk, here: string): Command {
  const kept: ChoiceAnswer[] = []
  let changed = false
  ;(c.answers ?? []).forEach((a, k) => {
    const at = `${here}, answer ${k + 1}`
    const conds = Array.isArray(a.if) ? mapConditions(a.if, w, at) : a.if
    if (conds === DROP) { changed = true; w.dropped?.({ what: 'command', text: `Answer “${a.text}” of ${w.describe?.command?.(c) ?? 'a question'}`, where: at }); return }
    const inner = Array.isArray(a.do) ? mapCommands(a.do, w, at) : a.do
    let next = a
    if (conds !== a.if) next = put(next, 'if', conds)
    if (inner !== a.do) next = put(next, 'do', inner)
    if (next !== a) changed = true
    kept.push(next)
  })
  return changed ? { ...c, answers: kept } : c
}

/** `trail` numbers the commands of a nested list after the `if` they sit in: "3" at the top, "3.1" inside it. */
export function mapCommands(list: Command[], w: Walk, where: string, trail = ''): Command[] {
  const out: Command[] = []
  let changed = false
  list.forEach((c, i) => {
    const num = trail ? `${trail}.${i + 1}` : `${i + 1}`
    const here = `${where}, command ${num}`
    const first = mapCommand(c, w, here)
    if (first === DROP) { changed = true; w.dropped?.({ what: 'command', text: w.describe?.command?.(c) ?? 'A command', where: here }); return }
    let r: Command = first
    for (const branch of ['then', 'else'] as const) {
      const inner: Command[] | undefined = r[branch]
      if (!Array.isArray(inner)) continue
      const next = mapCommands(inner, w, where, num)
      if (next !== inner) r = { ...r, [branch]: next }
    }
    if (Array.isArray(r.answers)) r = mapAnswers(r, w, here)
    if (r !== c) changed = true
    out.push(r)
  })
  return changed ? out : list
}

function mapEvents(list: StoryEvent[], w: Walk): StoryEvent[] {
  const out: StoryEvent[] = []
  let changed = false
  for (const e of list) {
    const where = `event “${e.id}”`
    const trig = e.when && typeof e.when === 'object' ? mapTrigger(e.when, w, `${where}, “When”`) : e.when
    const conds = Array.isArray(e.if) ? mapConditions(e.if, w, `${where}, “Only if”`) : e.if
    if (trig === DROP || conds === DROP) { changed = true; w.dropped?.({ what: 'event', text: w.describe?.event?.(e) ?? `Event “${e.id}”`, where }); continue }
    const cmds = Array.isArray(e.do) ? mapCommands(e.do, w, where) : e.do
    let next = e
    if (trig !== e.when) next = put(next, 'when', trig)
    if (conds !== e.if) next = put(next, 'if', conds)
    if (cmds !== e.do) next = put(next, 'do', cmds)
    if (next !== e) changed = true
    out.push(next)
  }
  return changed ? out : list
}

/** Walks the map's events and objectives. Returns the same map when nothing changed. */
export function walkStory(map: MapFile, w: Walk): MapFile {
  let out = map
  if (Array.isArray(map.events)) {
    const next = mapEvents(map.events, w)
    if (next !== map.events) {
      if (next.length) out = { ...out, events: next }
      else { const { events: _gone, ...rest } = out; out = rest as MapFile }
    }
  }
  if (map.objectives && typeof map.objectives === 'object') {
    const next = mapObjectives(map.objectives, w, 'the objectives')
    if (next !== map.objectives) {
      if (Object.keys(next).length) out = { ...out, objectives: next }
      else { const { objectives: _gone, ...rest } = out; out = rest as MapFile }
    }
  }
  return out
}

/** Every command of the list including those inside `if` branches, with where it sits. */
export function forEachCommand(list: Command[] | undefined, where: string, fn: (c: Command, where: string) => void, trail = ''): void {
  if (!Array.isArray(list)) return
  list.forEach((c, i) => {
    const num = trail ? `${trail}.${i + 1}` : `${i + 1}`
    fn(c, `${where}, command ${num}`)
    forEachCommand(c.then, where, fn, num)
    forEachCommand(c.else, where, fn, num)
    ;(c.answers ?? []).forEach((a, k) => forEachCommand(a.do, `${where}, command ${num}, answer ${k + 1}`, fn))
  })
}
