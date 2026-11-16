// Editor-side checks for promotion (docs/SRPG.md "Checks / Problems"): a promotion item no promotion lists, a promotion that
// needs an item nobody hands out, a chain of promotions that comes back to its start (the engine allows it; flagged as a note),
// carried weapons the promoted type cannot wield, plus the loader's refusals in plain words.
import { entryId } from './itemGrants'
import { carriedWeapons } from './itemRules'
import { progressionOn, promotionsOf } from './promotion'
import type { SrpgCheck } from './abilityChecks'
import { forEachCommand } from './storyWalk'
import type { ItemEntry, ProjectDoc, Unit } from './types'

const holds = (list: ItemEntry[] | undefined, id: string) => !!list?.some((e) => entryId(e) === id)

/** Does anything in the game give this item to someone: a starting list, a shop, a map, a side's storage or an event? */
export function itemIsHandedOut(doc: ProjectDoc, id: string): boolean {
  if ((doc.units ?? []).some((u) => holds(u.items, id))) return true
  if ((doc.characters ?? []).some((c) => holds(c.items, id))) return true
  if ((doc.structures ?? []).some((s) => s.sells?.includes(id))) return true
  for (const m of Object.values(doc.maps)) {
    if (!m) continue
    if ((m.units ?? []).some((u) => holds(u.items, id)) || (m.sides ?? []).some((s) => holds(s.storage, id))) return true
    let found = false
    for (const e of m.events ?? []) forEachCommand(e.do, '', (c) => { if (c.giveItem === id || holds(c.addUnit?.items, id)) found = true })
    if (found) return true
  }
  return false
}

/** Unit types a unit type can reach by promoting once or more, as the list of chains that lead back to it. */
function loopsBack(doc: ProjectDoc, start: Unit): string[] | null {
  const seen = new Set<string>()
  const walk = (id: string, path: string[]): string[] | null => {
    for (const p of promotionsOf(doc.units?.find((u) => u.id === id))) {
      if (p.to === start.id) return [...path, p.to]
      if (seen.has(p.to)) continue
      seen.add(p.to)
      const r = walk(p.to, [...path, p.to])
      if (r) return r
    }
    return null
  }
  return walk(start.id, [start.id])
}

const nameOf = (doc: ProjectDoc, id: string) => doc.units?.find((u) => u.id === id)?.name ?? id

function loopChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  const reported = new Set<string>()
  for (const u of doc.units ?? []) {
    const chain = loopsBack(doc, u)
    if (!chain) continue
    const key = [...new Set(chain)].sort().join('|')
    if (reported.has(key)) continue
    reported.add(key)
    out.push({ section: 'units', ids: [...new Set(chain)], text: `Note: ${chain.map((id) => nameOf(doc, id)).join(' → ')} promotes in a circle, so a unit could promote again and again. The game allows it.` })
  }
  return out
}

function weaponChecks(doc: ProjectDoc): SrpgCheck[] {
  if (!carriedWeapons(doc)) return []
  const out: SrpgCheck[] = []
  const canWield = (t: Unit, weaponId: string) => {
    const w = doc.weapons?.find((x) => x.id === weaponId)
    return t.weapons?.includes(weaponId) || (!!w?.group && !!t.wields?.includes(w.group))
  }
  for (const u of doc.units ?? []) {
    for (const p of promotionsOf(u)) {
      const to = doc.units?.find((x) => x.id === p.to)
      if (!to) continue
      for (const e of u.items ?? []) {
        const item = doc.items?.find((i) => i.id === entryId(e))
        if (item?.kind === 'weapon' && item.weapon && canWield(u, item.weapon) && !canWield(to, item.weapon))
          out.push({ section: 'units', ids: [u.id], text: `${u.name} starts with ${item.name}, but a ${to.name} cannot wield it after promoting. It stays in the pack, unused.` })
      }
    }
  }
  return out
}

export function promotionChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  const progression = progressionOn(doc)
  for (const i of doc.items ?? []) {
    if (i.promotes && !(doc.units ?? []).some((u) => promotionsOf(u).some((p) => p.items?.includes(i.id))))
      out.push({ section: 'items', ids: [i.id], text: `${i.name}: no unit type’s promotion uses this item, so it promotes nobody. Add it to a promotion on the Units screen.` })
  }
  for (const u of doc.units ?? []) {
    promotionsOf(u).forEach((p, n) => {
      const where = `${u.name}, promotion ${n + 1}`
      if (!doc.units?.some((x) => x.id === p.to)) out.push({ section: 'units', ids: [u.id], text: `${where}: “${p.to}” is not a unit type of this game.` })
      else if (p.to === u.id) out.push({ section: 'units', ids: [u.id], text: `${where}: a unit type cannot promote into itself.` })
      if (p.atLevel && p.atLevel !== 'off' && !progression) out.push({ section: 'units', ids: [u.id], text: `${where}: promoting on reaching a level needs experience and levels. Turn them on in the Rules screen.` })
      for (const id of p.items ?? []) {
        const item = doc.items?.find((x) => x.id === id)
        if (!item?.promotes) out.push({ section: 'units', ids: [u.id], text: `${where}: “${item?.name ?? id}” is not a promotion item. Switch on “Promotion item” for it on the Items screen.` })
      }
      // Items are the only way in when the promotion is not offered or automatic at a level: then one has to be handed out somewhere.
      const onlyItems = (!p.atLevel || p.atLevel === 'off') && (p.items?.length ?? 0) > 0
      if (onlyItems && !p.items!.some((id) => itemIsHandedOut(doc, id)))
        out.push({ section: 'units', ids: [u.id], text: `${where}: nobody is ever given ${p.items!.length === 1 ? 'its promotion item' : 'any of its promotion items'} (no unit, shop, map or event hands it out), so this promotion cannot happen.` })
    })
  }
  return [...out, ...loopChecks(doc), ...weaponChecks(doc)]
}
