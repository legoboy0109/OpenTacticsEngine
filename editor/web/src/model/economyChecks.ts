// Checks that mirror what the game loader refuses about research, upgrades, repair, fuel, ammunition and supply
// (engine/core/data/loader_research.cpp, loader_upgrades.cpp, loader_supply.cpp), so a maker sees all of them in the
// editor at once before pressing Play (the loader stops at the first one). The wording is plain; the loader's own
// messages still show in the Problems list after a save.
import { carriedAsItem } from './supplyUnits'
import { footprintText } from './structuresEconomy'
import { findLoop, TURNS_MAX } from './research'
import { isHex, gridOfGame } from './gridSpec'
import { ringsOf, sizeOf } from './structures'
import type { Section, ProjectDoc, Technology } from './types'

export interface EconomyCheck { section: Section; text: string }

const isInt = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)
const dupes = (list: string[]): string[] => list.filter((x, i) => list.indexOf(x) !== i)

function costChecks(doc: ProjectDoc, cost: Record<string, number> | undefined, say: (t: string) => void): void {
  const resources = new Set((doc.game?.resources ?? []).map((r) => r.id))
  for (const [id, n] of Object.entries(cost ?? {})) {
    if (!resources.has(id)) say(`the cost names “${id}”, which is not a resource of this game.`)
    else if (typeof n === 'number' && n < 0) say('a cost cannot be negative.')
  }
}

function techChecks(doc: ProjectDoc): EconomyCheck[] {
  const out: EconomyCheck[] = []
  const list = doc.research ?? []
  const unitIds = new Set((doc.units ?? []).map((u) => u.id))
  const stats = new Set((doc.game?.stats ?? []).map((s) => s.id))
  const techIds = new Set(list.map((t) => t.id))
  list.forEach((t: Technology, i) => {
    const say = (text: string) => out.push({ section: 'research', text: `${t.name || t.id || `Technology ${i + 1}`}: ${text}` })
    if (!t.id) say('it has no internal name.')
    else if (list.findIndex((x) => x.id === t.id) !== i) say(`another technology has the same internal name “${t.id}”.`)
    costChecks(doc, t.cost, say)
    if (t.turns !== undefined && (!isInt(t.turns) || t.turns < 0 || t.turns > TURNS_MAX)) say(`the turns must be a whole number from 0 to ${TURNS_MAX} (0 = finished at once).`)
    for (const r of dupes(t.requires ?? [])) say(`“Needs first” lists “${r}” twice.`)
    for (const r of t.requires ?? []) if (!techIds.has(r)) say(`“Needs first” names “${r}”, which is not a technology of this game.`)
    const unlocks = t.unlocks?.units ?? []
    for (const u of unlocks) if (!unitIds.has(u)) say(`“Unlocks” names “${u}”, which is not a unit type of this game.`)
    for (const u of dupes(unlocks)) say(`“Unlocks” lists “${u}” twice.`)
    if (t.bonus) {
      for (const u of t.bonus.types ?? []) if (!unitIds.has(u)) say(`the bonus names “${u}”, which is not a unit type of this game.`)
      for (const u of dupes(t.bonus.types ?? [])) say(`the bonus lists “${u}” twice.`)
      const amounts = t.bonus.stats
      if (!amounts) say('the bonus needs a number for at least one stat.')
      else {
        for (const s of Object.keys(amounts)) if (!stats.has(s)) say(`the bonus names “${s}”, which is not a stat of this game.`)
        if (!Object.values(amounts).some((v) => v !== 0)) say('the bonus must change at least one stat (or be switched off).')
      }
    }
  })
  const loop = findLoop(list)
  if (loop) out.push({ section: 'research', text: `Technologies need each other in a loop: ${loop.map((id) => list.find((t) => t.id === id)?.name ?? id).join(' → ')}.` })
  return out
}

function structureChecks(doc: ProjectDoc): EconomyCheck[] {
  const out: EconomyCheck[] = []
  const list = doc.structures ?? []
  const hex = isHex(gridOfGame(doc.game))
  const techIds = new Set((doc.research ?? []).map((t) => t.id))
  const tags = new Set((doc.units ?? []).flatMap((u) => u.tags ?? []))
  const unitIds = new Set((doc.units ?? []).map((u) => u.id))
  for (const s of list) {
    const say = (text: string) => out.push({ section: 'structures', text: `${s.name}: ${text}` })
    const seen = new Set<string>()
    ;(s.upgrades ?? []).forEach((u, k) => {
      const at = `upgrade ${k + 1}`
      const target = list.find((x) => x.id === u.to)
      if (!target) say(`${at} goes to “${u.to}”, which is not a structure of this game.`)
      else if (target.id === s.id) say(`${at}: a structure cannot be upgraded to itself.`)
      else if (hex ? ringsOf(target) !== ringsOf(s) : sizeOf(target).w !== sizeOf(s).w || sizeOf(target).h !== sizeOf(s).h)
        say(`${at}: ${target.name} covers ${footprintText(target, hex)}${hex ? '' : ' squares'} but ${s.name} covers ${footprintText(s, hex)}. An upgrade keeps the size.`)
      if (seen.has(u.to)) say(`${at}: “${target?.name ?? u.to}” is listed twice.`)
      seen.add(u.to)
      costChecks(doc, u.cost, (t) => say(`${at}: ${t}`))
      if (u.requires !== undefined && !techIds.has(u.requires))
        say(`${at} needs “${u.requires}”, which is not a technology of this game${doc.research?.length ? '' : ' (the game has no research yet)'}.`)
    })
    for (const e of s.repairs ?? []) if (!unitIds.has(e) && !tags.has(e)) say(`it repairs “${e}”, which is neither a unit type nor a label of any unit type.`)
  }
  return out
}

function unitChecks(doc: ProjectDoc): EconomyCheck[] {
  const out: EconomyCheck[] = []
  for (const u of doc.units ?? []) {
    const say = (text: string) => out.push({ section: 'units', text: `${u.name}: ${text}` })
    if (u.fuel !== undefined && (!isInt(u.fuel) || u.fuel < 1 || u.fuel > 999)) say('the fuel must be a whole number from 1 to 999.')
    if (u.fuelPerTurn !== undefined) {
      if (u.fuel === undefined) say('“Loses fuel every turn” needs a fuel tank.')
      else if (!isInt(u.fuelPerTurn) || u.fuelPerTurn < 1 || u.fuelPerTurn > 999) say('the fuel lost every turn must be a whole number from 1 to 999.')
      else if (isInt(u.fuel) && u.fuelPerTurn >= u.fuel) say(`it loses ${u.fuelPerTurn} fuel every turn but holds only ${u.fuel}: it would be destroyed at its first turn. Lose less than the tank holds.`)
    }
  }
  return out
}

function weaponChecks(doc: ProjectDoc): EconomyCheck[] {
  const out: EconomyCheck[] = []
  for (const w of doc.weapons ?? []) {
    if (w.ammo === undefined) continue
    const say = (text: string) => out.push({ section: 'weapons', text: `${w.name}: ${text}` })
    if (!isInt(w.ammo) || w.ammo < 1 || w.ammo > 99) say('the ammunition must be a whole number from 1 to 99.')
    const item = carriedAsItem(doc, w.id)
    if (item) say(`ammunition is for weapons that stay with their unit type; this weapon is carried as the item “${item}”, whose uses already limit it. Set it to unlimited.`)
  }
  return out
}

/** rules.research and rules.fog (engine/core/data/loader_options.cpp): unknown keys, a flag that is not on/off, an ambush choice the game does not know. */
function optionRuleChecks(doc: ProjectDoc): EconomyCheck[] {
  const out: EconomyCheck[] = []
  const rules = doc.game?.rules
  const research = rules?.research
  if (research !== undefined) {
    for (const k of Object.keys(research)) if (k !== 'needsStructure') out.push({ section: 'rules', text: `Research rules: “${k}” is not something this game knows.` })
    if (research.needsStructure !== undefined && typeof research.needsStructure !== 'boolean') out.push({ section: 'rules', text: 'Research rules: “needs a structure” must be on or off.' })
  }
  const fog = rules?.fog
  if (fog !== undefined) {
    for (const k of Object.keys(fog)) if (k !== 'ambush') out.push({ section: 'rules', text: `Fog of war rules: “${k}” is not something this game knows.` })
    if (fog.ambush !== undefined && fog.ambush !== 'endsTurn' && fog.ambush !== 'canAct')
      out.push({ section: 'rules', text: `Fog of war rules: what happens to an ambushed unit must be “endsTurn” or “canAct”, not “${String(fog.ambush)}”.` })
  }
  return out
}

function ruleChecks(doc: ProjectDoc): EconomyCheck[] {
  const rep = doc.game?.rules?.supply?.repair
  if (!rep) return optionRuleChecks(doc)
  const out: EconomyCheck[] = optionRuleChecks(doc)
  if (rep.amount === undefined) out.push({ section: 'rules', text: 'Repair needs a percent of hit points per turn.' })
  else if (!isInt(rep.amount) || rep.amount < 1 || rep.amount > 100) out.push({ section: 'rules', text: 'Repair: the percent of hit points per turn must be from 1 to 100.' })
  if (rep.costShare !== undefined && (!isInt(rep.costShare) || rep.costShare < 0 || rep.costShare > 100)) out.push({ section: 'rules', text: 'Repair: the share of the unit’s price paid must be from 0 to 100.' })
  return out
}

/** Everything the loader would refuse about the economy options, with the screen each belongs to. */
export const economyChecks = (doc: ProjectDoc): EconomyCheck[] =>
  [...techChecks(doc), ...structureChecks(doc), ...unitChecks(doc), ...weaponChecks(doc), ...ruleChecks(doc)]
