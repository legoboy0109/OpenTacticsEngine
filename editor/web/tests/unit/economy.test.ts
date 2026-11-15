// Upgrades, research, repair, fuel, ammunition and supply in the editor's model: edits, defaults, the clean-up when
// something they name is deleted (one undo step), the checks that mirror the loader, and the story pieces.
import { describe, expect, it, vi } from 'vitest'
import type { Api, PutBody, PutResult, Result } from '../../src/api/client'
import { economyChecks } from '../../src/model/economyChecks'
import { editMap } from '../../src/model/mapCtx'
import { findUsages } from '../../src/model/references'
import {
  addTechnology, deleteTechnology, duplicateTechnology, findLoop, needsFirstBlock, setBonusOn, setBonusStat, setBonusTypes, setTechCost,
  setTechRequires, setTechTurns, setTechUnlocks, turnsOf, updateTechnology,
} from '../../src/model/research'
import { reconcileEconomy, technologyUsageLines, technologyUses } from '../../src/model/researchRefs'
import { removeResource } from '../../src/model/rulesLists'
import { addStat, removeStat } from '../../src/model/stats'
import { addCommand } from '../../src/model/storyEdit'
import { newCommand, newCondition } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText, conditionText } from '../../src/model/storySummary'
import { addStructure, deleteStructure, setSize } from '../../src/model/structures'
import {
  addUpgrade, patchUpgrade, removeUpgrade, setRepairs, setResearchHere, setUpgradeCost, upgradeTargets,
} from '../../src/model/structuresEconomy'
import {
  repairAmount, repairOn, repairShare, setRepairAmount, setRepairOn, setRepairShare, setStructuresResupply, structuresResupply,
} from '../../src/model/supplyRules'
import { carriedAsItem, setAmmo, setFuel, setFuelPerTurn, setSupplies, startingBurn } from '../../src/model/supplyUnits'
import { addItem, updateItem, setItemKind } from '../../src/model/items'
import { setWeaponsMode } from '../../src/model/itemRules'
import { structureUsageLines } from '../../src/model/structuresRefs'
import { deleteUnit, setUnitTags } from '../../src/model/units'
import type { Condition } from '../../src/model/storyTypes'
import type { ProjectDoc, ServerProject } from '../../src/model/types'
import { buildBody } from '../../src/store/autosave'
import { DocumentStore } from '../../src/store/documentStore'
import { loadPlainEconomy } from './fixtures'

/** The economy demo (units infantry, mech, recon, tank, artillery; structures city, factory, hq; resource funds; sides blue and red). */
const base = (): ProjectDoc => loadPlainEconomy()
const mapId = (d: ProjectDoc) => d.game!.maps[0].id

/** A game that uses all of it: two more kinds (a same-size "metropolis" and a research lab), three technologies, repair. */
function rich(): ProjectDoc {
  let d = addStat(base(), 'Guard', 'guard') // a stat that is neither health nor movement, so it can be deleted
  d = addStructure(d, 'Metropolis', 'metro')
  d = addStructure(d, 'Lab', 'lab')
  d = addTechnology(d, 'Tools', 'tools')
  d = addTechnology(d, 'Masonry', 'masonry')
  d = addTechnology(d, 'Armor', 'armor')
  d = setTechRequires(d, 'masonry', ['tools'])
  d = setTechCost(d, 'masonry', 'funds', 4000)
  d = setTechUnlocks(d, 'armor', ['tank', 'mech'])
  d = setBonusOn(d, 'armor', true)
  d = setBonusTypes(d, 'armor', ['tank'])
  d = setResearchHere(d, 'lab', true)
  d = addUpgrade(d, 'city')
  d = patchUpgrade(d, 'city', 0, { to: 'metro', requires: 'masonry' })
  d = setUpgradeCost(d, 'city', 0, 'funds', 3000)
  d = setRepairOn(d, true)
  d = setRepairs(d, 'city', ['tank', 'infantry'])
  return d
}

describe('research list', () => {
  it('the first technology creates the list; deleting the last keeps an empty list (the file is then removed)', () => {
    let d = base()
    expect(d.research).toBeNull()
    expect(addTechnology(d, 'Tools', 'tools').research).toEqual([{ id: 'tools', name: 'Tools' }])
    d = addTechnology(d, 'Tools', 'tools')
    const saved = base()
    expect(buildBody(d, saved, 'r', false)!.research).toEqual([{ id: 'tools', name: 'Tools' }])
    const after = deleteTechnology(d, 'tools')
    expect(after.research).toEqual([])
    expect(buildBody(after, d, 'r', false)!.research).toEqual([])
    // Undo past the first technology sends [] too (null -> nothing to remove when the file never existed).
    expect(buildBody(saved, d, 'r', false)!.research).toEqual([])
    expect(buildBody(saved, saved, 'r', false)).toBeNull()
  })
  it('an edit on a game with no research leaves it null', () => {
    expect(updateTechnology(base(), 'x', { name: 'y' }).research).toBeNull()
  })
  it('turns: 1 is the default and not written, 0 is kept, the range is clamped', () => {
    let d = addTechnology(base(), 'Tools', 'tools')
    expect(turnsOf(d.research![0])).toBe(1)
    d = setTechTurns(d, 'tools', 0)
    expect(d.research![0].turns).toBe(0)
    d = setTechTurns(d, 'tools', 1)
    expect(d.research![0]).not.toHaveProperty('turns')
    expect(setTechTurns(d, 'tools', 500).research![0].turns).toBe(99)
  })
  it('cost, needs-first and unlocks leave no empty keys behind', () => {
    let d = addTechnology(addTechnology(base(), 'A', 'a'), 'B', 'b')
    d = setTechCost(d, 'a', 'funds', 5)
    d = setTechCost(d, 'a', 'funds', undefined)
    d = setTechRequires(d, 'a', ['b'])
    d = setTechRequires(d, 'a', [])
    d = setTechUnlocks(d, 'a', ['tank'])
    d = setTechUnlocks(d, 'a', [])
    expect(d.research![0]).toEqual({ id: 'a', name: 'A' })
  })
  it('a bonus starts valid (+1 of a non-role stat), takes unit types and stat rows, and 0 removes a row', () => {
    let d = setBonusOn(addTechnology(addStat(base(), 'Guard', 'guard'), 'A', 'a'), 'a', true)
    const stat = Object.keys(d.research![0].bonus!.stats!)[0]
    expect(d.game!.roles.hp).not.toBe(stat)
    expect(d.research![0].bonus!.stats![stat]).toBe(1)
    d = setBonusTypes(d, 'a', ['tank'])
    expect(d.research![0].bonus!.types).toEqual(['tank'])
    d = setBonusTypes(d, 'a', [])
    expect(d.research![0].bonus).not.toHaveProperty('types')
    d = setBonusStat(d, 'a', d.game!.roles.hp, -2)
    expect(d.research![0].bonus!.stats![d.game!.roles.hp]).toBe(-2)
    d = setBonusStat(d, 'a', stat, 0)
    expect(d.research![0].bonus!.stats).toEqual({ [d.game!.roles.hp]: -2 })
    expect(setBonusOn(d, 'a', false).research![0]).not.toHaveProperty('bonus')
  })
  it('duplicate keeps everything under a new id', () => {
    const d = duplicateTechnology(rich(), 'masonry', 'masonry_2')
    expect(d.research!.map((t) => t.id)).toEqual(['tools', 'masonry', 'masonry_2', 'armor'])
    expect(d.research![2]).toMatchObject({ requires: ['tools'], cost: { funds: 4000 } })
  })
})

describe('prerequisites cannot loop', () => {
  it('itself and anything that already needs it are refused, with a reason', () => {
    const d = rich()
    expect(needsFirstBlock(d, 'tools', 'tools')).toMatch(/itself/)
    expect(needsFirstBlock(d, 'tools', 'masonry')).toMatch(/loop/) // masonry needs tools
    expect(needsFirstBlock(d, 'masonry', 'tools')).toBeNull()
    expect(needsFirstBlock(d, 'armor', 'masonry')).toBeNull()
    // through a chain: armor <- masonry <- tools; tools cannot need armor
    expect(needsFirstBlock(setTechRequires(d, 'armor', ['masonry']), 'tools', 'armor')).toMatch(/loop/)
  })
  it('findLoop names a loop in a hand-written file and nothing otherwise', () => {
    expect(findLoop(rich().research!)).toBeNull()
    expect(findLoop([{ id: 'a', name: 'A', requires: ['b'] }, { id: 'b', name: 'B', requires: ['a'] }])).toEqual(['a', 'b', 'a'])
  })
})

describe('supply rules', () => {
  it('repair on writes only the amount; defaults are not written; an emptied block disappears', () => {
    let d = setRepairOn(base(), true)
    expect(d.game!.rules!.supply).toEqual({ repair: { amount: 20 } })
    expect(repairOn(d)).toBe(true)
    expect(repairShare(d)).toBe(100)
    d = setRepairAmount(d, 35)
    d = setRepairShare(d, 0)
    expect(d.game!.rules!.supply).toEqual({ repair: { amount: 35, costShare: 0 } })
    expect(repairAmount(d)).toBe(35)
    d = setRepairShare(d, 100)
    expect(d.game!.rules!.supply!.repair).toEqual({ amount: 35 })
    d = setStructuresResupply(d, true)
    expect(structuresResupply(d)).toBe(true)
    d = setRepairOn(d, false)
    expect(d.game!.rules!.supply).toEqual({ structures: true })
    d = setStructuresResupply(d, false)
    expect(d.game!.rules).not.toHaveProperty('supply')
  })
  it('keeps keys it does not know inside the block', () => {
    const d = base()
    const withExtra = { ...d, game: { ...d.game!, rules: { ...d.game!.rules, supply: { repair: { amount: 10, future: 1 }, later: true } } } } as ProjectDoc
    const next = setRepairAmount(withExtra, 15)
    expect(next.game!.rules!.supply).toEqual({ repair: { amount: 15, future: 1 }, later: true })
  })
})

describe('fuel, supplying and ammunition', () => {
  it('fuel off removes the burn too; the burn starts below the tank', () => {
    let d = setFuel(base(), 'tank', 60)
    expect(startingBurn(60)).toBe(5)
    expect(startingBurn(3)).toBe(2)
    expect(startingBurn(1)).toBeNull()
    d = setFuelPerTurn(d, 'tank', 5)
    d = setSupplies(d, 'tank', true)
    expect(d.units!.find((u) => u.id === 'tank')).toMatchObject({ fuel: 60, fuelPerTurn: 5, supplies: true })
    d = setFuel(d, 'tank', undefined)
    const tank = d.units!.find((u) => u.id === 'tank')!
    expect(tank).not.toHaveProperty('fuel')
    expect(tank).not.toHaveProperty('fuelPerTurn')
    expect(setSupplies(d, 'tank', false).units!.find((u) => u.id === 'tank')).not.toHaveProperty('supplies')
  })
  it('ammunition is clamped and removable; a weapon carried as an item is named', () => {
    let d = setAmmo(base(), 'cannon', 6)
    expect(d.weapons!.find((w) => w.id === 'cannon')!.ammo).toBe(6)
    expect(setAmmo(d, 'cannon', 500).weapons!.find((w) => w.id === 'cannon')!.ammo).toBe(99)
    expect(setAmmo(d, 'cannon', undefined).weapons!.find((w) => w.id === 'cannon')).not.toHaveProperty('ammo')
    expect(carriedAsItem(d, 'cannon')).toBeNull()
    d = setWeaponsMode(addItem(d, 'Cannon', 'cannon_item'), 'carried')
    d = setItemKind(d, 'cannon_item', 'weapon')
    d = updateItem(d, 'cannon_item', { weapon: 'cannon' })
    expect(carriedAsItem(d, 'cannon')).toBe('Cannon')
  })
})

describe('upgrades', () => {
  it('only kinds of the same footprint can be picked, not itself, not twice', () => {
    const d = rich()
    expect(upgradeTargets(d, 'city').map((s) => s.id)).toEqual(['factory', 'lab'])
    expect(upgradeTargets(d, 'city', 'metro').map((s) => s.id)).toContain('metro')
    expect(upgradeTargets(d, 'hq').map((s) => s.id)).toEqual([]) // the only 2 x 2 kind
    expect(addUpgrade(d, 'hq')).toBe(d)
    expect(d.structures!.find((s) => s.id === 'city')!.upgrades).toEqual([{ to: 'metro', cost: { funds: 3000 }, requires: 'masonry' }])
  })
  it('removing the last upgrade removes the key', () => {
    expect(removeUpgrade(rich(), 'city', 0).structures!.find((s) => s.id === 'city')).not.toHaveProperty('upgrades')
  })
  it('a size change makes the old target wrong, and the checks say so', () => {
    const d = setSize(rich(), 'metro', 2, 2)
    expect(economyChecks(d).some((c) => c.section === 'structures' && /keeps the size/.test(c.text))).toBe(true)
  })
})

describe('clean-up when something is deleted (reconcileEconomy)', () => {
  const stored = (d: ProjectDoc, id: string) => d.structures!.find((s) => s.id === id)!

  it('a technology: others stop needing it, upgrades stop requiring it, events that use it go', () => {
    let d = rich()
    d = editMap(d, mapId(d), (m) => ({
      ...m,
      events: [
        { id: 'gate', when: { battle: 'start' }, if: [{ researched: 'tools', side: 'blue' }], do: [{ say: 'hi' }] },
        { id: 'gift', when: { battle: 'start' }, do: [{ grantResearch: 'tools', side: 'blue' }, { say: 'kept' }] },
        { id: 'other', when: { battle: 'start' }, do: [{ say: 'untouched' }] },
      ],
    }))
    d = { ...d, structures: d.structures!.map((s) => (s.id === 'city' ? { ...s, upgrades: [{ to: 'metro', requires: 'tools' }] } : s)) }
    const lines = technologyUsageLines(d, 'tools').map((l) => l.text).join('\n')
    expect(lines).toMatch(/City.*upgrade to “Metropolis”/)
    expect(lines).toMatch(/Masonry/)
    expect(lines).toMatch(/gate/)
    expect(technologyUses(d, 'tools')).toEqual(expect.arrayContaining([expect.stringMatching(/Masonry/), expect.stringMatching(/event “gate”/), expect.stringMatching(/event “gift”/)]))
    const after = deleteTechnology(d, 'tools')
    expect(after.research!.map((t) => t.id)).toEqual(['masonry', 'armor'])
    expect(after.research!.find((t) => t.id === 'masonry')).not.toHaveProperty('requires')
    expect(stored(after, 'city').upgrades).toEqual([{ to: 'metro' }])
    const events = after.maps[mapId(after)]!.events!
    expect(events.map((e) => e.id)).toEqual(['gift', 'other'])
    expect(events.find((e) => e.id === 'gift')!.do).toEqual([{ say: 'kept' }])
    expect(events.some((e) => e.id === 'gate')).toBe(false)
  })

  it('a unit type: unlock and bonus lists, repairs; a bonus that loses all its types goes with them', () => {
    const d = rich()
    const after = reconcileEconomy(d, deleteUnit(d, 'tank'))
    expect(after.research!.find((t) => t.id === 'armor')!.unlocks).toEqual({ units: ['mech'] })
    expect(after.research!.find((t) => t.id === 'armor')).not.toHaveProperty('bonus') // it was only for tanks
    expect(stored(after, 'city').repairs).toEqual(['infantry'])
    const lines = findUsages(d, 'unit', 'tank').map((u) => u.text).join('\n')
    expect(lines).toMatch(/Armor.*unlocks/)
    expect(lines).toMatch(/City.*repairs/)
    // the last unlock is gone: the block goes
    const lone = setTechUnlocks(d, 'armor', ['tank'])
    expect(reconcileEconomy(lone, deleteUnit(lone, 'tank')).research!.find((t) => t.id === 'armor')).not.toHaveProperty('unlocks')
  })

  it('a repairs entry that is also a tag of another unit type stays', () => {
    let d = setUnitTags(rich(), 'recon', ['tank'])
    const after = reconcileEconomy(d, deleteUnit(d, 'tank'))
    expect(stored(after, 'city').repairs).toEqual(['tank', 'infantry'])
    d = base()
  })

  it('a structure kind: upgrades to it go (the empty list loses its key)', () => {
    const d = rich()
    expect(structureUsageLines(d, 'metro').map((u) => u.text).join('\n')).toMatch(/City.*upgraded to it/)
    const after = reconcileEconomy(d, deleteStructure(d, 'metro'))
    expect(stored(after, 'city')).not.toHaveProperty('upgrades')
  })

  it('a resource: technology and upgrade costs lose it', () => {
    const d = rich()
    const lines = findUsages(d, 'resource', 'funds').map((u) => u.text).join('\n')
    expect(lines).toMatch(/Masonry.*cost/)
    expect(lines).toMatch(/upgrade to “Metropolis”.*cost/)
    const after = reconcileEconomy(d, removeResource(d, 'funds'))
    expect(after.research!.find((t) => t.id === 'masonry')).not.toHaveProperty('cost')
    expect(stored(after, 'city').upgrades).toEqual([{ to: 'metro', requires: 'masonry' }])
  })

  it('a stat: the bonus loses its row, and the bonus goes when nothing is left', () => {
    let d = rich()
    const stat = Object.keys(d.research!.find((t) => t.id === 'armor')!.bonus!.stats!)[0]
    expect(findUsages(d, 'stat', stat).map((u) => u.text).join('\n')).toMatch(/Armor.*changes it/)
    const after = reconcileEconomy(d, removeStat(d, stat))
    expect(after.research!.find((t) => t.id === 'armor')).not.toHaveProperty('bonus')
    d = setBonusStat(d, 'armor', d.game!.roles.hp, 3)
    const two = reconcileEconomy(d, removeStat(d, stat))
    expect(two.research!.find((t) => t.id === 'armor')!.bonus).toEqual({ types: ['tank'], stats: { [d.game!.roles.hp]: 3 } })
  })

  it('leaves alone what was already dangling, and returns the same object when nothing changed', () => {
    const d = rich()
    const dangling = { ...d, research: d.research!.map((t) => (t.id === 'masonry' ? { ...t, requires: ['ghost'] } : t)) }
    const after = reconcileEconomy(dangling, deleteTechnology(dangling, 'armor'))
    expect(after.research!.find((t) => t.id === 'masonry')!.requires).toEqual(['ghost'])
    expect(reconcileEconomy(d, d)).toBe(d)
    const renamed = updateTechnology(d, 'tools', { name: 'Toolmaking' })
    expect(reconcileEconomy(d, renamed)).toBe(renamed)
  })
})

describe('one undo step through the store', () => {
  type PutFn = (body: PutBody) => Promise<Result<PutResult>>
  const fake = (project: ServerProject, put: PutFn) => ({
    getProject: vi.fn(async () => ({ ok: true, value: project }) as Result<ServerProject>),
    putProject: vi.fn(async (_id: string, body: PutBody) => put(body)),
    listArt: vi.fn(async () => ({ ok: true, value: [] })),
    listModels: vi.fn(async () => ({ ok: true, value: [] })),
    listAudio: vi.fn(async () => ({ ok: true, value: [] })),
  }) as unknown as Api

  it('deleting a used technology cleans everything, and one undo brings it all back', async () => {
    const d = rich()
    const store = new DocumentStore(fake({ ...d, id: 'g', revision: 'r0', problems: [] }, async () => ({ ok: true, value: { revision: 'r1', problems: [] } })), 'g')
    await store.load()
    store.edit((x) => deleteTechnology(x, 'masonry'))
    const after = store.getState().doc
    expect(after.research!.map((t) => t.id)).toEqual(['tools', 'armor'])
    expect(after.structures!.find((s) => s.id === 'city')!.upgrades).toEqual([{ to: 'metro', cost: { funds: 3000 } }])
    store.undo()
    expect(store.getState().doc).toEqual({ ...d })
    expect(store.getState().doc.research!.map((t) => t.id)).toEqual(['tools', 'masonry', 'armor'])
  })

  it('a delete made by another path (a unit) is cleaned by the store itself', async () => {
    const d = rich()
    const store = new DocumentStore(fake({ ...d, id: 'g', revision: 'r0', problems: [] }, async () => ({ ok: true, value: { revision: 'r1', problems: [] } })), 'g')
    await store.load()
    store.edit((x) => deleteUnit(x, 'tank'))
    expect(store.getState().doc.structures!.find((s) => s.id === 'city')!.repairs).toEqual(['infantry'])
    store.undo()
    expect(store.getState().doc).toEqual({ ...d })
  })
})

describe('checks mirror the loader', () => {
  const texts = (d: ProjectDoc, section?: string) => economyChecks(d).filter((c) => !section || c.section === section).map((c) => c.text)

  it('a clean game has none', () => {
    expect(economyChecks(rich())).toEqual([])
    expect(economyChecks(base())).toEqual([])
  })
  it('technologies: cost, turns, needs, unlocks, bonus, loops', () => {
    const d = rich()
    const bad = { ...d, research: [
      { id: 'a', name: 'A', cost: { gold: 5, funds: -1 }, turns: 120, requires: ['zzz', 'b', 'b'], unlocks: { units: ['nope', 'tank', 'tank'] } },
      { id: 'b', name: 'B', requires: ['a'], bonus: { types: ['nope'], stats: { ghost: 1 } } },
      { id: 'c', name: 'C', bonus: { stats: {} } },
      { id: 'd', name: 'D', bonus: { stats: { [d.game!.roles.hp]: 0 } } },
      { id: 'a', name: 'Again' },
      { id: 'x', name: 'X', requires: ['y'] },
      { id: 'y', name: 'Y', requires: ['x'] },
    ] }
    const t = texts(bad, 'research').join('\n')
    for (const part of ['“gold”, which is not a resource', 'cannot be negative', 'from 0 to 99', '“zzz”, which is not a technology', 'lists “b” twice',
      '“nope”, which is not a unit type', 'lists “tank” twice', '“ghost”, which is not a stat', 'at least one stat', 'same internal name', 'in a loop: X → Y → X'])
      expect(t).toContain(part)
  })
  it('structures: unknown, itself, twice, size, cost, unknown technology and repairs entries', () => {
    const d = rich()
    const city = (up: object[], repairs?: string[]) => ({ ...d, structures: d.structures!.map((s) => (s.id === 'city' ? { ...s, upgrades: up, ...(repairs ? { repairs } : {}) } : s)) }) as ProjectDoc
    const t = texts(city([{ to: 'gone' }, { to: 'city' }, { to: 'metro' }, { to: 'metro', cost: { gold: 1 } }, { to: 'hq' }, { to: 'lab', requires: 'ghost' }], ['tank', 'armored', 'wat']), 'structures').join('\n')
    for (const part of ['“gone”, which is not a structure', 'cannot be upgraded to itself', '“Metropolis” is listed twice', '“gold”, which is not a resource', 'keeps the size',
      '“ghost”, which is not a technology', 'it repairs “wat”'])
      expect(t).toContain(part)
    expect(t).not.toContain('“tank”, which')
    expect(texts({ ...city([{ to: 'lab', requires: 'x' }]), research: null }, 'structures').join()).toContain('the game has no research yet')
    // a repairs entry that is a tag of some unit type is fine
    expect(texts(setUnitTags(city([], ['armored']), 'tank', ['armored']), 'structures')).toEqual([])
  })
  it('units, weapons and the repair rule', () => {
    let d = rich()
    d = { ...d, units: d.units!.map((u) => (u.id === 'tank' ? { ...u, fuel: 10, fuelPerTurn: 10 } : u.id === 'mech' ? { ...u, fuelPerTurn: 2 } : u.id === 'recon' ? { ...u, fuel: 0 } : u)) }
    const u = texts(d, 'units').join('\n')
    expect(u).toMatch(/Tank: .*holds only 10/)
    expect(u).toMatch(/needs a fuel tank/)
    expect(u).toMatch(/fuel must be a whole number from 1 to 999/)
    d = { ...d, weapons: d.weapons!.map((w) => (w.id === 'cannon' ? { ...w, ammo: 100 } : w)) }
    expect(texts(d, 'weapons').join()).toMatch(/from 1 to 99/)
    d = setWeaponsMode(addItem(d, 'Gun', 'gun'), 'carried')
    d = updateItem(setItemKind(d, 'gun', 'weapon'), 'gun', { weapon: 'rifle' })
    d = { ...d, weapons: d.weapons!.map((w) => (w.id === 'rifle' ? { ...w, ammo: 3 } : w)) }
    expect(texts(d, 'weapons').join('\n')).toMatch(/carried as the item “Gun”/)
    const rules = (supply: object) => ({ ...d, game: { ...d.game!, rules: { ...d.game!.rules, supply } } }) as ProjectDoc
    expect(texts(rules({ repair: {} }), 'rules')).toEqual(['Repair needs a percent of hit points per turn.'])
    expect(texts(rules({ repair: { amount: 150, costShare: 101 } }), 'rules')).toHaveLength(2)
    expect(texts(rules({ repair: { amount: 10, costShare: 0 } }), 'rules')).toEqual([])
  })
})

describe('story: researched condition and grantResearch command', () => {
  const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps[mapId(d)]! })

  it('start with the first technology and side, and read as plain sentences', () => {
    const d = rich()
    const story = { ...ctx(d), mapId: mapId(d) } as never
    const cond = newCondition(story, 'researched') as Condition
    expect(cond).toEqual({ researched: 'tools', side: 'blue' })
    expect(conditionText(ctx(d), cond)).toBe('side Blue Army has researched “Tools”')
    expect(commandText(ctx(d), newCommand(story, 'grantResearch'))).toBe('Gives side Blue Army the technology “Tools”')
  })
  it('checks name an unknown technology and a missing side', () => {
    const d = rich()
    const m = mapId(d)
    const bad = editMap(d, m, (map) => ({ ...map, events: [{ id: 'e', when: { battle: 'start' }, if: [{ researched: 'ghost', side: 'green' }], do: [{ grantResearch: '', side: 'blue' }, { grantResearch: 'tools', side: 'nobody' }] }] }))
    const t = storyChecks(bad, m).map((c) => c.text).join('\n')
    expect(t).toContain('“ghost” is not a technology')
    expect(t).toContain('“green” is not a side')
    expect(t).toContain('pick a technology')
    expect(t).toContain('“nobody” is not a side')
    const none = storyChecks({ ...bad, research: null }, m).map((c) => c.text).join('\n')
    expect(none).toContain('the game has no research yet')
  })
  it('a command can be added to an event like any other', () => {
    const d = rich()
    const m = mapId(d)
    let x = editMap(d, m, (map) => ({ ...map, events: [{ id: 'e', when: { battle: 'start' }, do: [] }] }))
    x = editMap(x, m, (map) => addCommand(map, 'e', [], { grantResearch: 'tools', side: 'blue' }))
    expect(x.maps[m]!.events![0].do).toEqual([{ grantResearch: 'tools', side: 'blue' }])
  })
})
