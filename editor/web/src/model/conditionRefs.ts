// Who uses a status condition, and what deleting one does (docs/SRPG.md section 9): the lines for the delete question, and the clean-up that
// happens in the same edit (one Undo brings it all back). Ids are not renamable in this editor (the form shows them read only, as for skills), so
// there is no rename. What is removed, by the smallest thing that cannot work any more:
//   - a staff's "inflicts" part, its "cures" part (or the id out of its list), a usable item's cure list entry / inflict, a weapon's inflict;
//   - an `immune` entry, an `inflict` effect, in skills and in other conditions; a rule whose "only if" asks about the condition goes whole
//     (dropping the test would make it apply to everyone); a rule left with no effect goes;
//   - the test `{ condition }` in an ability's or a staff's "only if" (the requirement goes, not the ability or staff);
//   - the id in other conditions' "replaces";
//   - story commands / conditions about it (storyRefs, in the maps).
// A staff left with no effect would load as an error, so it heals 10 instead (and an enemy-only staff becomes one for allies).
import { blocksOfCondition } from './conditions'
import type { Usage } from './references'
import { scrubMaps, storyUsages } from './storyRefs'
import { fxKind, fxList } from './staffFx'
import type { Condition } from './srpgTypes'
import type { Extra, Item, ProjectDoc } from './types'

const list = (v: unknown): Extra[] => (Array.isArray(v) ? (v as Extra[]) : [])
const effectsIn = (b: Extra): Extra[] => list(b.do)
const asksAbout = (c: Extra, id: string): boolean => c.condition === id

// --- what uses it -------------------------------------------------------------------------------------------------------------

/** A usable item with no heal, raise, cure, inflict or promotion does nothing (the loader refuses it). */
const usableDoesNothing = (i: Item): boolean =>
  i.heal === undefined && !(i.raise && Object.values(i.raise).some((v) => v !== 0)) && !i.promotes && i.cure === undefined && i.inflict === undefined

function staffLeftEmpty(i: Item, id: string): boolean {
  const left = fxList(i).filter((e) => {
    const k = fxKind(e)
    if (k === 'inflict') return (e.inflict as Extra | undefined)?.condition !== id
    if (k === 'cure') return !(Array.isArray(e.cure) && e.cure.length === 1 && e.cure[0] === id)
    return true
  })
  return left.length === 0
}

export function conditionUsageLines(doc: ProjectDoc, id: string): Usage[] {
  const out: Usage[] = []
  for (const i of doc.items ?? []) {
    if (i.kind === 'staff') {
      const effects = fxList(i)
      const inflicts = effects.some((e) => fxKind(e) === 'inflict' && (e.inflict as Extra | undefined)?.condition === id)
      const cures = effects.some((e) => fxKind(e) === 'cure' && Array.isArray(e.cure) && e.cure.includes(id))
      const tail = (inflicts || cures) && staffLeftEmpty(i, id) ? ' (it heals 10 instead, so the staff still works)' : ''
      if (inflicts) out.push({ section: 'items', text: `Staff “${i.name}” inflicts it (that part is removed)${tail}` })
      if (cures) out.push({ section: 'items', text: `Staff “${i.name}” cures it (it is taken off the list)${tail}` })
      if (list(i.if).some((c) => asksAbout(c, id))) out.push({ section: 'items', text: `Staff “${i.name}” only works when the unit has it (that requirement is removed)` })
    } else if (i.kind === 'usable') {
      const nothingLeft = usableDoesNothing(cleanItem(i, id))
      const tail = nothingLeft ? ' (the item then does nothing: give it an effect, or delete it)' : ''
      if (Array.isArray(i.cure) && i.cure.includes(id)) out.push({ section: 'items', text: `Item “${i.name}” cures it (it is taken off the list)${tail}` })
      if (i.inflict?.condition === id) out.push({ section: 'items', text: `Item “${i.name}” puts it on the unit that uses it (that part is removed)${tail}` })
    }
  }
  for (const w of doc.weapons ?? []) if (w.inflict?.condition === id) out.push({ section: 'weapons', text: `Weapon “${w.name}” may inflict it on a hit (that part is removed)` })
  const blocksUse = (who: string, section: 'skills' | 'conditions', blocks: Extra[]) =>
    blocks.forEach((b, n) => {
      if (list(b.if).some((c) => asksAbout(c, id))) { out.push({ section, text: `${who}, rule ${n + 1}, only applies when the unit has it (the rule is removed)` }); return }
      for (const e of effectsIn(b)) {
        if (Array.isArray(e.immune) && e.immune.includes(id)) out.push({ section, text: `${who} makes its owner immune to it (it is taken off the list)` })
        if ((e.inflict as Extra | undefined)?.condition === id) out.push({ section, text: `${who}, rule ${n + 1}, inflicts it (that effect is removed)` })
      }
    })
  for (const s of doc.skills ?? []) blocksUse(`Skill “${s.name}”`, 'skills', list(s.blocks))
  for (const a of doc.abilities ?? []) if (list(a.if).some((c) => asksAbout(c, id))) out.push({ section: 'abilities', text: `Ability “${a.name}” only works when the unit has it (that requirement is removed)` })
  for (const c of doc.conditions ?? []) {
    if (c.id === id) continue
    if (c.replaces?.includes(id)) out.push({ section: 'conditions', text: `Condition “${c.name}” replaces it (it is taken off the list)` })
    blocksUse(`Condition “${c.name}”`, 'conditions', blocksOfCondition(c))
  }
  out.push(...storyUsages(doc, [{ kind: 'condition', id }]))
  return out
}

// --- the clean-up --------------------------------------------------------------------------------------------------------------

/** A list of blocks without what names `id` (a rule that asked about it goes whole; so does a rule left with no effect). */
export function cleanBlocksOf(blocks: Extra[], id: string): Extra[] {
  return blocks.flatMap((b): Extra[] => {
    if (list(b.if).some((c) => asksAbout(c, id))) return []
    const effects = effectsIn(b).flatMap((e): Extra[] => {
      if (Array.isArray(e.immune) && e.immune.includes(id)) {
        const rest = e.immune.filter((x) => x !== id)
        return rest.length ? [{ ...e, immune: rest }] : []
      }
      return (e.inflict as Extra | undefined)?.condition === id ? [] : [e]
    })
    if (effects.length === effectsIn(b).length && effects.every((e, k) => e === effectsIn(b)[k])) return [b]
    return effects.length ? [{ ...b, do: effects }] : []
  })
}

function cleanItem(i: Item, id: string): Item {
  let out = i
  if (i.kind === 'usable') {
    if (Array.isArray(i.cure) && i.cure.includes(id)) { const rest = i.cure.filter((x) => x !== id); const { cure: _c, ...others } = out; out = rest.length ? { ...others, cure: rest } : others }
    if (i.inflict?.condition === id) { const { inflict: _i, ...others } = out; out = others }
    return out
  }
  if (i.kind !== 'staff') return out
  let effects = fxList(i)
  const had = effects.length
  effects = effects.flatMap((e): Extra[] => {
    const k = fxKind(e)
    if (k === 'inflict') return (e.inflict as Extra | undefined)?.condition === id ? [] : [e]
    if (k === 'cure' && Array.isArray(e.cure) && e.cure.includes(id)) {
      const rest = e.cure.filter((x) => x !== id)
      return rest.length ? [{ cure: rest }] : []
    }
    return [e]
  })
  const tests = list(i.if).filter((c) => !asksAbout(c, id))
  if (effects.length === had && tests.length === list(i.if).length) return i
  const { if: _if, effects: _fx, ...rest } = out
  out = { ...rest, effects: effects.length ? effects : [{ heal: 10 }], ...(tests.length ? { if: tests } : {}) } as Item
  if (effects.length === 0 && out.target === 'foe') { const { target: _t, ...noTarget } = out; out = noTarget as Item }
  return out
}

/** Takes every use of the condition out of the document (not the condition itself). */
export function cleanCondition(doc: ProjectDoc, id: string): ProjectDoc {
  const items = doc.items?.map((i) => cleanItem(i, id)) ?? null
  const weapons = doc.weapons?.map((w) => (w.inflict?.condition === id ? (({ inflict: _i, ...rest }) => rest)(w) : w)) ?? null
  const skills = doc.skills?.map((s) => (JSON.stringify(cleanBlocksOf(list(s.blocks), id)) === JSON.stringify(list(s.blocks)) ? s : { ...s, blocks: cleanBlocksOf(list(s.blocks), id) })) ?? null
  const abilities = doc.abilities?.map((a) => (list(a.if).some((c) => asksAbout(c, id)) ? { ...a, if: list(a.if).filter((c) => !asksAbout(c, id)) } : a)) ?? null
  const conditions = doc.conditions?.map((c): Condition => {
    const cleaned = cleanBlocksOf(blocksOfCondition(c), id)
    const same = JSON.stringify(cleaned) === JSON.stringify(blocksOfCondition(c)) && !c.replaces?.includes(id)
    if (same) return c
    const replaces = c.replaces?.filter((x) => x !== id)
    const { replaces: _r, ...rest } = c
    return { ...rest, blocks: cleaned, ...(replaces?.length ? { replaces } : {}) }
  }) ?? null
  // `skills` / `abilities` / `items` / `weapons` keep their identity when nothing in them changed (an untouched section is not saved again).
  const same = <T,>(a: T[] | null, b: T[] | null): T[] | null => (a && b && a.every((x, k) => x === b[k]) ? a : b)
  return {
    ...doc,
    items: same(doc.items, items), weapons: same(doc.weapons, weapons), skills: same(doc.skills, skills), abilities: same(doc.abilities, abilities), conditions: same(doc.conditions, conditions),
    maps: scrubMaps(doc, [{ kind: 'condition', id }]),
  }
}

export function deleteCondition(doc: ProjectDoc, id: string): ProjectDoc {
  const clean = cleanCondition(doc, id)
  return clean.conditions?.some((c) => c.id === id) ? { ...clean, conditions: clean.conditions.filter((c) => c.id !== id) } : clean
}

