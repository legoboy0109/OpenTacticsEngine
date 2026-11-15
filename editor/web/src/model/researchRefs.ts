// What other parts of a game hold about research, upgrades and repairs, for the moments that matter:
//  - something they point at was deleted (a technology, unit type, structure kind, resource or stat): `reconcileEconomy`
//    removes what can no longer work. The document store runs it after EVERY edit (like reconcileCampaign), so no delete
//    path can forget it, and it is part of the same undo step. It is ONE pass over the lists: the ids that were there
//    before the edit and are gone after it are collected once, and every place that names one is cleaned from that;
//  - "used by" lines for a delete confirmation and the line on the Research form.
// Only what was there BEFORE and is gone AFTER is touched: a name that was already dangling stays, so the checks can
// point at it instead of the editor silently deleting something.
import { scrubMaps, storyUsages } from './storyRefs'
import { walkStory } from './storyWalk'
import { dropKey } from './util'
import type { Usage } from './references'
import type { ProjectDoc, Structure, Technology, Upgrade } from './types'

interface Gone { techs: Set<string>; units: Set<string>; structures: Set<string>; resources: Set<string>; stats: Set<string> }

const idsOf = (list: Array<{ id: string }> | null | undefined): Set<string> => new Set((list ?? []).map((x) => x.id))
const lost = (a: Set<string>, b: Set<string>): Set<string> => new Set([...a].filter((x) => !b.has(x)))

function goneBetween(before: ProjectDoc, after: ProjectDoc): Gone {
  return {
    techs: lost(idsOf(before.research), idsOf(after.research)),
    units: lost(idsOf(before.units), idsOf(after.units)),
    structures: lost(idsOf(before.structures), idsOf(after.structures)),
    // game.json could not be read (null): nothing is known to be gone.
    resources: before.game && after.game ? lost(idsOf(before.game.resources), idsOf(after.game.resources)) : new Set(),
    stats: before.game && after.game ? lost(idsOf(before.game.stats), idsOf(after.game.stats)) : new Set(),
  }
}
const none = (g: Gone): boolean => Object.values(g).every((s) => s.size === 0)

/** The same object when nothing changed (callers tell by identity). */
function mapSame<T>(list: T[], fn: (x: T) => T): T[] {
  const next = list.map(fn)
  return next.some((x, i) => x !== list[i]) ? next : list
}

/** Number map without the keys in `drop`; the key is removed when it ends up empty. */
function withoutKeys(m: Record<string, number> | undefined, drop: Set<string>): Record<string, number> | undefined {
  let cur = m
  for (const k of drop) cur = dropKey(cur, k)
  return cur
}

function cleanTechnology(t: Technology, g: Gone): Technology {
  const c: Record<string, unknown> = { ...t }
  let changed = false
  const set = (k: string, v: unknown) => { changed = true; if (v === undefined) delete c[k]; else c[k] = v }

  if (t.requires?.some((r) => g.techs.has(r))) { const v = t.requires.filter((r) => !g.techs.has(r)); set('requires', v.length ? v : undefined) }
  const cost = withoutKeys(t.cost, g.resources)
  if (cost !== t.cost) set('cost', cost)
  if (t.unlocks?.units?.some((u) => g.units.has(u))) {
    const units = t.unlocks.units.filter((u) => !g.units.has(u))
    const rest: Record<string, unknown> = { ...t.unlocks }
    if (units.length) rest.units = units; else delete rest.units
    set('unlocks', Object.keys(rest).length ? rest : undefined)
  }
  if (t.bonus) {
    const stats = withoutKeys(t.bonus.stats, g.stats)
    const typesGone = t.bonus.types?.some((u) => g.units.has(u))
    const types = typesGone ? t.bonus.types!.filter((u) => !g.units.has(u)) : t.bonus.types
    // A list that loses ALL its unit types must not turn into "every unit type": the bonus goes instead.
    if (typesGone && types!.length === 0) set('bonus', undefined)
    else if (stats !== t.bonus.stats || typesGone) {
      const moved = Object.values(stats ?? {}).some((v) => v !== 0)
      set('bonus', moved ? { ...t.bonus, stats, ...(typesGone ? { types } : {}) } : undefined)
    }
  }
  return changed ? (c as unknown as Technology) : t
}

function cleanUpgrade(u: Upgrade, g: Gone): Upgrade {
  const cost = withoutKeys(u.cost, g.resources)
  const dropNeeds = u.requires !== undefined && g.techs.has(u.requires)
  if (cost === u.cost && !dropNeeds) return u
  const c: Record<string, unknown> = { ...u }
  if (cost !== u.cost) { if (cost) c.cost = cost; else delete c.cost }
  if (dropNeeds) delete c.requires
  return c as unknown as Upgrade
}

function cleanStructure(s: Structure, g: Gone, tagsAfter: Set<string>): Structure {
  const c: Record<string, unknown> = { ...s }
  let changed = false
  if (s.upgrades?.length) {
    const kept = s.upgrades.filter((u) => !g.structures.has(u.to))
    const next = mapSame(kept, (u) => cleanUpgrade(u, g))
    if (next !== s.upgrades) { changed = true; if (next.length) c.upgrades = next; else delete c.upgrades }
  }
  // An entry is a unit type id or a tag; one that is still a tag of some other unit type stays.
  if (s.repairs?.some((e) => g.units.has(e) && !tagsAfter.has(e))) {
    changed = true
    c.repairs = s.repairs.filter((e) => !(g.units.has(e) && !tagsAfter.has(e)))
  }
  return changed ? (c as unknown as Structure) : s
}

/** Cleans research, upgrades, repairs lists and story references after an edit that deleted a technology, unit type,
 *  structure kind, resource or stat. Returns `after` itself when nothing it holds named something that is gone. */
export function reconcileEconomy(before: ProjectDoc, after: ProjectDoc): ProjectDoc {
  if (before === after) return after
  const g = goneBetween(before, after)
  if (none(g)) return after
  const tagsAfter = new Set((after.units ?? []).flatMap((u) => u.tags ?? []))
  const research = after.research ? mapSame(after.research, (t) => cleanTechnology(t, g)) : after.research
  const structures = after.structures ? mapSame(after.structures, (s) => cleanStructure(s, g, tagsAfter)) : after.structures
  const next: ProjectDoc = { ...after, research, structures }
  // Events that check for or give a deleted technology go with it (the same walk the other story clean-ups use).
  const maps = g.techs.size ? scrubMaps(next, [...g.techs].map((id) => ({ kind: 'tech' as const, id }))) : next.maps
  const out = research === after.research && structures === after.structures && maps === after.maps ? after : { ...next, maps }
  return out
}

// --- "used by" ---

const mapName = (doc: ProjectDoc, id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? id
const nameOfStructure = (doc: ProjectDoc, id: string) => doc.structures?.find((s) => s.id === id)?.name ?? id

/** Where a technology is needed, as short phrases for the line on its form ("City → Metropolis upgrade"). */
export function technologyUses(doc: ProjectDoc, id: string): string[] {
  const out: string[] = []
  for (const s of doc.structures ?? [])
    for (const u of s.upgrades ?? []) if (u.requires === id) out.push(`the upgrade of “${s.name}” to “${nameOfStructure(doc, u.to)}”`)
  for (const t of doc.research ?? []) if (t.requires?.includes(id)) out.push(`the technology “${t.name}”`)
  for (const [mid, m] of Object.entries(doc.maps)) {
    if (!m) continue
    const events = new Set<string>()
    walkStory(m, { ref: (r, where) => {
      const ev = /^event “(.*?)”/.exec(where)
      if (r.kind === 'tech' && r.id === id && ev) events.add(ev[1])
      return r.id
    } })
    for (const e of events) out.push(`the event “${e}” on map “${mapName(doc, mid)}”`)
  }
  return out
}

/** Lines for the delete confirmation of a technology: what is taken out when it goes. */
export function technologyUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const s of doc.structures ?? [])
    for (const u of s.upgrades ?? [])
      if (u.requires === id) out.push({ section: 'structures', text: `Structure “${s.name}”: the upgrade to “${nameOfStructure(doc, u.to)}” needs it (it will need nothing)` })
  for (const t of doc.research ?? []) if (t.requires?.includes(id)) out.push({ section: 'research', text: `Technology “${t.name}” needs it first (that requirement is removed)` })
  out.push(...storyUsages(doc, [{ kind: 'tech', id }]))
  return out
}

/** Lines about economy data for deleting a unit type, a resource, a stat or a structure kind (see references.ts). */
export function economyUsageLines(doc: ProjectDoc, kind: 'unit' | 'resource' | 'stat' | 'structure', id: string): Usage[] {
  const out: Usage[] = []
  switch (kind) {
    case 'unit':
      for (const t of doc.research ?? []) {
        if (t.unlocks?.units?.includes(id)) out.push({ section: 'research', text: `Technology “${t.name}” unlocks it (it is taken off that list)` })
        if (t.bonus?.types?.includes(id)) out.push({ section: 'research', text: t.bonus.types.length === 1 ? `Technology “${t.name}” gives it a bonus (the bonus is removed)` : `Technology “${t.name}” gives it a bonus (it is taken off that list)` })
      }
      for (const s of doc.structures ?? []) if (s.repairs?.includes(id)) out.push({ section: 'structures', text: `Structure “${s.name}” repairs it (it is taken off that list)` })
      break
    case 'resource':
      for (const t of doc.research ?? []) if (t.cost && id in t.cost) out.push({ section: 'research', text: `Technology “${t.name}” has a cost in it` })
      for (const s of doc.structures ?? [])
        for (const u of s.upgrades ?? []) if (u.cost && id in u.cost) out.push({ section: 'structures', text: `Structure “${s.name}”: the upgrade to “${nameOfStructure(doc, u.to)}” has a cost in it` })
      break
    case 'stat':
      for (const t of doc.research ?? []) if (t.bonus?.stats && id in t.bonus.stats) out.push({ section: 'research', text: `Technology “${t.name}” changes it` })
      break
    case 'structure':
      for (const s of doc.structures ?? [])
        for (const u of s.upgrades ?? []) if (u.to === id && s.id !== id) out.push({ section: 'structures', text: `Structure “${s.name}” can be upgraded to it (that upgrade is removed)` })
      break
  }
  return out
}
