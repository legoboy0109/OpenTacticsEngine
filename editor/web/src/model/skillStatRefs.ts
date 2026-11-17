// What deleting a stat does to skills (E2): the stat leaves every "stat bonus" / "growth bonus" effect; an effect left with no
// stat goes, a rule left with no effect goes, a skill left with no rule is deleted (with everything that named it). A chance
// formula that reads the stat can no longer work, so the rule's chance becomes 0 (it never applies) rather than failing at play.
import { deleteCondition } from './conditionRefs'
import { deleteSkill } from './srpgRefs'
import type { Extra, ProjectDoc } from './types'

const reads = (formula: unknown, stat: string): boolean => typeof formula === 'string' && new RegExp(`\\b[ab]\\.${stat}\\b`).test(formula)

function cleanBlock(b: Extra, stat: string): Extra | null {
  const effects = (Array.isArray(b.do) ? (b.do as Extra[]) : []).flatMap((e): Extra[] => {
    for (const k of ['stats', 'growths'] as const) {
      const m = e[k] as Record<string, number> | undefined
      if (m && typeof m === 'object' && stat in m) {
        const { [stat]: _gone, ...rest } = m
        return Object.keys(rest).length ? [{ ...e, [k]: rest }] : []
      }
    }
    return [e]
  })
  if (effects.length === 0) return null
  return { ...b, do: effects, ...(reads(b.chance, stat) ? { chance: 0 } : {}) }
}

export function skillsUseStat(doc: ProjectDoc, stat: string): string[] {
  return (doc.skills ?? []).filter((s) => JSON.stringify(s.blocks ?? []).includes(`"${stat}"`) || (s.blocks ?? []).some((b) => reads(b.chance, stat))).map((s) => s.name)
}

export function cleanStatSkills(doc: ProjectDoc, stat: string): ProjectDoc {
  let out = doc
  for (const s of doc.skills ?? []) {
    const blocks = (s.blocks ?? []).flatMap((b) => { const c = cleanBlock(b, stat); return c ? [c] : [] })
    if (JSON.stringify(blocks) === JSON.stringify(s.blocks ?? [])) continue
    out = blocks.length === 0 ? deleteSkill(out, s.id) : { ...out, skills: out.skills!.map((x) => (x.id === s.id ? { ...x, blocks } : x)) }
  }
  return out
}

/** Conditions whose rules read the stat (their stat bonuses are skill blocks too). */
export function conditionsUseStat(doc: ProjectDoc, stat: string): string[] {
  return (doc.conditions ?? []).filter((c) => JSON.stringify(c.blocks ?? []).includes(`"${stat}"`) || (c.blocks ?? []).some((b) => reads(b.chance, stat))).map((c) => c.name)
}

/** The same clean-up for the rules of conditions; a condition left with no rule is deleted (with everything that named it). */
export function cleanStatConditions(doc: ProjectDoc, stat: string): ProjectDoc {
  let out = doc
  for (const c of doc.conditions ?? []) {
    const blocks = (c.blocks ?? []).flatMap((b) => { const x = cleanBlock(b, stat); return x ? [x] : [] })
    if (JSON.stringify(blocks) === JSON.stringify(c.blocks ?? [])) continue
    out = blocks.length === 0 ? deleteCondition(out, c.id) : { ...out, conditions: out.conditions!.map((x) => (x.id === c.id ? { ...x, blocks } : x)) }
  }
  return out
}
