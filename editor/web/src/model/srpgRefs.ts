// New ids, "used by" lines and delete clean-up for the strategy-RPG lists (docs/SRPG.md): characters, skills,
// abilities. Pure functions returning new documents, so a delete and its clean-up are one undo step. What the data
// shapes of the contract hold is covered here; the story forms of later steps (teach / forget a skill, promote,
// support points) add their own references to `storyRefs.ts` when they exist.
import { ids } from './doc'
import { makeId } from './ids'
import type { Usage } from './references'
import { scrubMap, scrubMaps, storyUsages } from './storyRefs'
import type { Learn } from './srpgTypes'
import type { Extra, MapFile, ProjectDoc } from './types'

const mapEntries = (doc: ProjectDoc): Array<[string, MapFile]> => Object.entries(doc.maps).filter((e): e is [string, MapFile] => e[1] !== null)
const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id

// --- new ids -------------------------------------------------------------------------------------------------------

/** A character's id is also the story id of its unit, so it avoids every id a map unit already uses. */
export const newCharacterId = (doc: ProjectDoc, name: string): string => {
  const mapUnitIds = mapEntries(doc).flatMap(([, m]) => (m.units ?? []).flatMap((u) => (typeof u.id === 'string' ? [u.id] : [])))
  return makeId(name, ids(doc.characters), { fallback: 'character', reserved: mapUnitIds })
}
export const newSkillId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.skills), { fallback: 'skill' })
export const newAbilityId = (doc: ProjectDoc, name: string): string => makeId(name, ids(doc.abilities), { fallback: 'ability' })

// --- who refers to what ----------------------------------------------------------------------------------------------

const isIn = (list: string[] | undefined, id: string) => !!list?.includes(id)
const learnsIn = (list: Learn[] | undefined, id: string) => !!list?.some((l) => l.skill === id)
/** Skills that grant an ability: `{ "ability": "<id>" }` effects in their blocks (docs/SRPG.md section 4). */
const grantsAbility = (blocks: Extra[] | undefined, id: string) => !!blocks?.some((b) => Array.isArray(b.do) && (b.do as Extra[]).some((e) => e?.ability === id))

/** Everything that names a character: map units that are the character, support pairs. */
export function characterUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const [mid, m] of mapEntries(doc)) {
    const n = (m.units ?? []).filter((u) => u.character === id).length
    if (n) out.push({ section: 'maps', text: `Map “${mapName(doc, mid)}”: it is placed on the map (the unit goes with it)` })
  }
  out.push(...storyUsages(doc, [{ kind: 'unit', id }]))
  for (const p of doc.supports?.pairs ?? []) if (p.a === id || p.b === id) out.push({ section: 'supports', text: `Supports: the pair “${p.a}” and “${p.b}” (the pair goes with it)` })
  return out
}

/** Who has a skill: unit types, characters, weapons, accessories. */
export function skillUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const u of doc.units ?? []) if (isIn(u.skills, id) || learnsIn(u.learns, id)) out.push({ section: 'units', text: `Unit “${u.name}” has it` })
  for (const c of doc.characters ?? []) if (isIn(c.skills, id) || learnsIn(c.learns, id)) out.push({ section: 'characters', text: `Character “${c.name}” has it` })
  for (const w of doc.weapons ?? []) if (isIn(w.skills, id)) out.push({ section: 'weapons', text: `Weapon “${w.name}” gives it` })
  for (const i of doc.items ?? []) if (isIn(i.skills, id)) out.push({ section: 'items', text: `Item “${i.name}” gives it` })
  out.push(...storyUsages(doc, [{ kind: 'skill', id }]))
  return out
}

/** Who has a menu ability: unit types, characters, skills that grant it. */
export function abilityUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const u of doc.units ?? []) if (isIn(u.abilities, id)) out.push({ section: 'units', text: `Unit “${u.name}” has it` })
  for (const c of doc.characters ?? []) if (isIn(c.abilities, id)) out.push({ section: 'characters', text: `Character “${c.name}” has it` })
  const granting = (doc.skills ?? []).filter((s) => grantsAbility(s.blocks, id))
  for (const s of granting) out.push({ section: 'skills', text: `Skill “${s.name}” grants it` })
  return out
}

// --- clean-up on delete ------------------------------------------------------------------------------------------------

/** A list that ends up empty loses its key (nothing is then the same as no list). */
function setList<T extends object>(o: T, key: string, list: unknown[]): T {
  const c: Record<string, unknown> = { ...(o as Record<string, unknown>) }
  if (list.length) c[key] = list; else delete c[key]
  return c as T
}
/** `o` with `id` taken out of its string list `key` (same object when it is not there). */
function dropId<T extends object>(o: T, key: string, id: string): T {
  const list = (o as Record<string, unknown>)[key] as string[] | undefined
  return list?.includes(id) ? setList(o, key, list.filter((x) => x !== id)) : o
}
function dropLearn<T extends object>(o: T, id: string): T {
  const list = (o as Record<string, unknown>).learns as Learn[] | undefined
  return list?.some((l) => l.skill === id) ? setList(o, 'learns', list.filter((l) => l.skill !== id)) : o
}
/** Same array when nothing changed, so an untouched section keeps its identity (and is not saved again). */
function mapKeep<T>(list: T[] | null, fn: (t: T) => T): T[] | null {
  if (!list) return list
  const next = list.map(fn)
  return next.some((x, i) => x !== list[i]) ? next : list
}

/** Takes the skill out of every unit type, character, weapon and item, and the events that teach, forget or ask for it. */
export function cleanSkill(doc: ProjectDoc, id: string): ProjectDoc {
  return {
    ...doc,
    maps: scrubMaps(doc, [{ kind: 'skill', id }]),
    units: mapKeep(doc.units, (u) => dropLearn(dropId(u, 'skills', id), id)),
    characters: mapKeep(doc.characters, (c) => dropLearn(dropId(c, 'skills', id), id)),
    weapons: mapKeep(doc.weapons, (w) => dropId(w, 'skills', id)),
    items: mapKeep(doc.items, (i) => dropId(i, 'skills', id)),
  }
}

/** Takes the ability out of every unit type and character, and the "grants it" effect out of skills (a rule left
 *  with nothing to do goes too). */
export function cleanAbility(doc: ProjectDoc, id: string): ProjectDoc {
  const strip = (blocks: Extra[]): Extra[] => blocks.flatMap((b) => {
    if (!Array.isArray(b.do) || !(b.do as Extra[]).some((e) => e?.ability === id)) return [b]
    const rest = (b.do as Extra[]).filter((e) => e?.ability !== id)
    return rest.length ? [{ ...b, do: rest }] : []
  })
  return {
    ...doc,
    units: mapKeep(doc.units, (u) => dropId(u, 'abilities', id)),
    characters: mapKeep(doc.characters, (c) => dropId(c, 'abilities', id)),
    skills: mapKeep(doc.skills, (s) => (grantsAbility(s.blocks, id) ? { ...s, blocks: strip(s.blocks!) } : s)),
  }
}

/** Takes the character's placed units off every map and its pairs out of the supports. */
export function cleanCharacter(doc: ProjectDoc, id: string): ProjectDoc {
  const maps: ProjectDoc['maps'] = {}
  // The units go, and with them what events and objectives said about the character's id (it is the unit's story name).
  for (const [mid, m] of Object.entries(doc.maps)) {
    const kept = m && m.units?.some((u) => u.character === id) ? { ...m, units: m.units.filter((u) => u.character !== id) } : m
    maps[mid] = kept && scrubMap(doc, kept, [{ kind: 'unit', id }])
  }
  const pairs = doc.supports?.pairs
  const keepPairs = pairs?.filter((p) => p.a !== id && p.b !== id)
  return { ...doc, maps, supports: doc.supports && pairs && keepPairs && keepPairs.length !== pairs.length ? { ...doc.supports, pairs: keepPairs } : doc.supports }
}

const removeFrom = <K extends 'characters' | 'skills' | 'abilities'>(doc: ProjectDoc, key: K, id: string): ProjectDoc => {
  const list = doc[key]
  if (!list?.some((x) => x.id === id)) return doc
  return { ...doc, [key]: list.filter((x) => x.id !== id) }
}

/** Deletes the item and every place that holds it, in one edit (so one Undo brings it all back). */
export const deleteCharacter = (doc: ProjectDoc, id: string): ProjectDoc => removeFrom(cleanCharacter(doc, id), 'characters', id)
export const deleteSkill = (doc: ProjectDoc, id: string): ProjectDoc => removeFrom(cleanSkill(doc, id), 'skills', id)
export const deleteAbility = (doc: ProjectDoc, id: string): ProjectDoc => removeFrom(cleanAbility(doc, id), 'abilities', id)
