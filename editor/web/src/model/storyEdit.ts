// Editing a map's events and objectives: pure functions on one MapFile (the screens wrap them in `editMap`).
// An emptied `events` list or `objectives` block leaves its key out; `else` lists that are emptied leave theirs.
import { renameUnitRefs } from './storyRefs'
import { newTrigger, type DefCtx } from './storyDefaults'
import type { ChoiceAnswer, Command, Condition, ObjectiveRule, Objectives, StoryEvent, TriggerKind } from './storyTypes'
import { moveItem, patchObj } from './util'
import type { MapFile } from './types'

// --- generic list operations (events, conditions, commands, objective rules all use them) ---
export const listAdd = <T,>(list: T[] | undefined, item: T, at?: number): T[] => {
  const l = list ?? []
  const i = at === undefined ? l.length : Math.max(0, Math.min(l.length, at))
  return [...l.slice(0, i), item, ...l.slice(i)]
}
export const listRemove = <T,>(list: T[], i: number): T[] => list.filter((_, j) => j !== i)
export const listMove = <T,>(list: T[], from: number, to: number): T[] => moveItem(list, from, to)
export const listPatch = <T extends object>(list: T[], i: number, patch: Record<string, unknown>): T[] =>
  list.map((x, j) => (j === i ? patchObj(x, patch) : x))
export const listReplace = <T,>(list: T[], i: number, item: T): T[] => list.map((x, j) => (j === i ? item : x))

// --- events ---
export const eventIds = (map: MapFile): string[] => (map.events ?? []).map((e) => e.id)

export function newEventId(map: MapFile): string {
  const taken = new Set(eventIds(map))
  for (let n = 1; ; n++) if (!taken.has(`event_${n}`)) return `event_${n}`
}

const setEvents = (map: MapFile, events: StoryEvent[]): MapFile => {
  if (events.length) return { ...map, events }
  const { events: _gone, ...rest } = map
  return rest as MapFile
}

/** A new event that runs when the battle starts and does nothing yet. */
export function addEvent(map: MapFile, c: DefCtx, id: string = newEventId(map)): MapFile {
  return setEvents(map, [...(map.events ?? []), { id, when: newTrigger(c, 'start'), do: [] }])
}

export const deleteEvent = (map: MapFile, id: string): MapFile => setEvents(map, (map.events ?? []).filter((e) => e.id !== id))

export function moveEvent(map: MapFile, from: number, to: number): MapFile {
  return map.events ? { ...map, events: listMove(map.events, from, to) } : map
}

/** Applies `fn` to one event; undefined values in a returned patch are not special here (use patchEvent for that). */
export function editEvent(map: MapFile, id: string, fn: (e: StoryEvent) => StoryEvent): MapFile {
  if (!map.events?.some((e) => e.id === id)) return map
  return { ...map, events: map.events.map((e) => (e.id === id ? fn(e) : e)) }
}

/** `undefined` removes the key. */
export const patchEvent = (map: MapFile, id: string, patch: Record<string, unknown>): MapFile =>
  editEvent(map, id, (e) => patchObj(e, patch))

/** The event's id is its name; nothing else points at an event, so renaming touches only the event. */
export const renameEvent = (map: MapFile, id: string, to: string): MapFile => patchEvent(map, id, { id: to })

/** A different kind of trigger starts from that kind's defaults. An `endTurn` command cannot stay in an event that
 *  is not a talk: it is removed with the change (the maker is told by the list shrinking; undo brings it back). */
export function setTriggerKind(map: MapFile, id: string, kind: TriggerKind, c: DefCtx): MapFile {
  return editEvent(map, id, (e) => {
    const when = newTrigger(c, kind)
    return { ...e, when, do: kind === 'talk' ? e.do : stripEndTurn(e.do) }
  })
}

function stripEndTurn(list: Command[]): Command[] {
  let changed = false
  const out: Command[] = []
  for (const c of list) {
    if (c.endTurn !== undefined) { changed = true; continue }
    const inner = (['then', 'else', 'loop'] as const).filter((b) => Array.isArray(c[b]))
    let next = c
    for (const b of inner) { const l = stripEndTurn(c[b]!); if (l !== c[b]) next = { ...next, [b]: l } }
    if (Array.isArray(c.answers)) {
      const answers = c.answers.map((a) => { const l = Array.isArray(a.do) ? stripEndTurn(a.do) : a.do; return l === a.do ? a : { ...a, do: l } })
      if (answers.some((a, i) => a !== c.answers![i])) next = { ...next, answers }
    }
    if (next !== c) changed = true
    out.push(next)
  }
  return changed ? out : list
}

export const setEventConditions = (map: MapFile, id: string, list: Condition[]): MapFile =>
  patchEvent(map, id, { if: list.length ? list : undefined })

/** "Repeat" is written only when on; off is the default. */
export const setEventRepeat = (map: MapFile, id: string, on: boolean): MapFile => patchEvent(map, id, { repeat: on ? true : undefined })

// --- commands, in the event's list or in the `then` / `else` list of an `if` further down ---
/** `answer0`..`answer3`: the commands of a `choice` command's answer (docs/EVENTS.md "Dialogue choices"). */
export type Branch = 'then' | 'else' | 'loop' | `answer${number}`
/** Where a list of commands is: [] = the event's own "Then" list; each step goes into one `if` command's branch or one `choice` command's answer. */
export type ListPath = Array<[index: number, branch: Branch]>

/** Which answer a branch is (-1 for `then` / `else`). */
export const answerOf = (b: Branch): number => (b.startsWith('answer') ? Number(b.slice(6)) : -1)

/** The commands of one branch of a command. */
export function branchList(c: Command | undefined, b: Branch): Command[] | undefined {
  const k = answerOf(b)
  return k >= 0 ? c?.answers?.[k]?.do : c?.[b as 'then' | 'else' | 'loop']
}

/** The command with the branch's list replaced; an emptied `else` (or an answer's `do`) leaves its key out. */
function withBranch(c: Command, b: Branch, list: Command[]): Command {
  const k = answerOf(b)
  if (k >= 0) return { ...c, answers: (c.answers ?? []).map((a, j) => (j === k ? (list.length ? { ...a, do: list } : (patchObj(a, { do: undefined }) as ChoiceAnswer)) : a)) }
  return b === 'else' && list.length === 0 ? patchObj(c, { else: undefined }) : { ...c, [b]: list }
}

export function getList(cmds: Command[], path: ListPath): Command[] | undefined {
  let cur: Command[] | undefined = cmds
  for (const [i, b] of path) cur = branchList(cur?.[i], b)
  return cur
}

function updateList(cmds: Command[], path: ListPath, fn: (l: Command[]) => Command[]): Command[] {
  if (path.length === 0) return fn(cmds)
  const [[i, b], ...rest] = path
  return cmds.map((c, j) => (j === i ? withBranch(c, b, updateList(branchList(c, b) ?? [], rest, fn)) : c))
}

/** Applies `fn` to the command list at `path` of event `id`. */
export function editCommands(map: MapFile, id: string, path: ListPath, fn: (l: Command[]) => Command[]): MapFile {
  return editEvent(map, id, (e) => ({ ...e, do: updateList(e.do ?? [], path, fn) }))
}

export const addCommand = (map: MapFile, id: string, path: ListPath, cmd: Command, at?: number): MapFile =>
  editCommands(map, id, path, (l) => listAdd(l, cmd, at))
export const removeCommand = (map: MapFile, id: string, path: ListPath, i: number): MapFile =>
  editCommands(map, id, path, (l) => listRemove(l, i))
export const moveCommand = (map: MapFile, id: string, path: ListPath, from: number, to: number): MapFile =>
  editCommands(map, id, path, (l) => listMove(l, from, to))
/** Changes some keys of one command; undefined removes a key. */
export const patchCommand = (map: MapFile, id: string, path: ListPath, i: number, patch: Record<string, unknown>): MapFile =>
  editCommands(map, id, path, (l) => listPatch(l, i, patch))
export const replaceCommand = (map: MapFile, id: string, path: ListPath, i: number, cmd: Command): MapFile =>
  editCommands(map, id, path, (l) => listReplace(l, i, cmd))

/** Where commands can go: the number of `if` levels above a list (0 = the event's own list). */
export const depthOf = (path: ListPath): number => path.length

// --- objectives ---
/** Empty text and empty lists are left out; an objectives block with nothing in it is not written at all. */
export function tidyObjectives(o: Objectives): Objectives {
  const out: Objectives = { ...o }
  if (!out.text?.trim()) delete out.text
  for (const k of ['win', 'lose'] as const) if (!out[k]?.length) delete out[k]
  return out
}

export function setObjectives(map: MapFile, o: Objectives): MapFile {
  const next = tidyObjectives(o)
  if (Object.keys(next).length) return { ...map, objectives: next }
  const { objectives: _gone, ...rest } = map
  return rest as MapFile
}

export const patchObjectives = (map: MapFile, patch: { text?: string; win?: ObjectiveRule[]; lose?: ObjectiveRule[] }): MapFile =>
  setObjectives(map, patchObj(map.objectives ?? {}, patch))

// --- unit story ids ---
/** Gives the placed unit at (x, y) a new story name (or none); everything that named the old one follows. */
export function setUnitStoryId(map: MapFile, x: number, y: number, to: string, v = 0): MapFile {
  const stack = (map.units ?? []).flatMap((u, j) => (u.x === x && u.y === y ? [j] : []))  // per-difficulty variants share a square
  const i = stack.length ? stack[v % stack.length] : -1
  if (i < 0) return map
  const old = map.units![i].id
  const units = map.units!.map((u, j) => (j === i ? patchObj(u, { id: to === '' ? undefined : to }) : u))
  const next = { ...map, units }
  return old && to ? renameUnitRefs(next, old, to) : next
}

/** Gives the unit an `addUnit` command creates a story name (or none); everything that named the old one follows. */
export function setAddedUnitId(map: MapFile, eventId: string, path: ListPath, index: number, to: string): MapFile {
  const event = map.events?.find((e) => e.id === eventId)
  const old = event ? getList(event.do ?? [], path)?.[index]?.addUnit?.id : undefined
  const next = editCommands(map, eventId, path, (l) => l.map((c, j) => (j === index && c.addUnit ? { ...c, addUnit: patchObj(c.addUnit, { id: to === '' ? undefined : to }) } : c)))
  return old && to ? renameUnitRefs(next, old, to) : next
}
