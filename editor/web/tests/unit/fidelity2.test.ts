// "Fidelity 2": an ability with only an inflict, the staged fight command, a range of condition damage, summon follow-ups, the battle scene's
// backdrop / platform tables, weather costs and the scene's choices (model/storyFight.ts, summonItems.ts, battleTables.ts, skillAmount.ts).
import { describe, expect, it } from 'vitest'
import { effectKind, setEffectKind, setTarget, targetsFor } from '../../src/model/abilities'
import { abilityChecks } from '../../src/model/abilityChecks'
import { abilitySummary } from '../../src/model/abilityText'
import { sceneChoiceProblems } from '../../src/model/battleSceneChecks'
import {
  addPlatform, backgroundFrom, dropTerrainFromTables, mapTable, mapTableProblems, platformList, removePlatform, sceneSides, setBackgroundFrom, setMapTableEntry, setMapWeather,
  setPlatformPicture, setSceneSides, setTerrainPlatform, setWeatherCost, weatherCostProblems, weatherCostOf, setDefaultPlatform,
} from '../../src/model/battleTables'
import { addBackground, removeBackground } from '../../src/model/battleArt'
import { amountKind, amountOf, amountRange, amountValue, withAmount } from '../../src/model/skillAmount'
import { addStrike, fightProblems, fightText, moveStrike, newFight, patchStrike, removeStrike, setFightFlag, setFightWeapon, setScripted } from '../../src/model/storyFight'
import { commandKind } from '../../src/model/storyTypes'
import { COMMAND_KINDS, SCENE_FX_KINDS } from '../../src/model/storyTypes'
import { addSummonItem, patchSummonItem, removeSummonItem, summonItemProblems, summonItemRows } from '../../src/model/summonItems'
import { setSummon } from '../../src/model/abilities'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { Ability } from '../../src/model/srpgTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => ({ ...switchOnFeature(loadDemo('demo-srpg'), 'abilities'), conditions: [{ id: 'boost', name: 'Boost', turns: 3, blocks: [{ when: 'always', do: [{ stats: { str: 1 } }] }] }] })
const mapIdOf = (d: ProjectDoc) => Object.keys(d.maps)[0]

describe('an ability with no effect of its own', () => {
  it('is the kind "none" when it has an inflict and no effect, and is valid then', () => {
    const d = base()
    const cond = d.conditions![0].id
    const a: Ability = { id: 'inspire', name: 'Inspire', target: 'ally', inflict: { condition: cond, turns: 4 } }
    expect(effectKind(a)).toBe('none')
    expect(effectKind({ id: 'x', name: 'X', target: 'ally' })).toBeNull()
    const doc = { ...d, abilities: [a] }
    expect(abilityChecks(doc).filter((c) => !c.text.includes('nobody has'))).toEqual([])
    expect(abilitySummary(doc, a)).toMatch(/Gives .* for 4 turns to an ally next to it, and nothing else/)
  })
  it('choosing "No other effect" drops the effect, starts an inflict and keeps a unit target', () => {
    const d = base()
    const start: Ability = { id: 'x', name: 'X', target: 'cell', range: [1, 2], effect: { summon: { type: d.units![0].id } } }
    const a = setEffectKind(d, start, 'none')
    expect(a.effect).toBeUndefined()
    expect(a.inflict).toEqual({ condition: d.conditions![0].id })
    expect(targetsFor('none')).toContain(a.target!)
    expect(a.target).not.toBe('cell')
    expect(setTarget(a, 'cell')).toBe(a)  // a square is not offered
  })
})

describe('the staged fight command', () => {
  const doc = base()
  it('is a command kind of its own, found by its key, and the scene kinds stay last', () => {
    expect(commandKind({ fight: newFight(['a', 'b']) })).toBe('fight')
    expect(COMMAND_KINDS.slice(-SCENE_FX_KINDS.length)).toEqual(SCENE_FX_KINDS)
    expect(COMMAND_KINDS.indexOf('fight')).toBeGreaterThan(-1)
  })
  it('edits like the loader reads it: defaults are not written', () => {
    let f = newFight(['kay', 'bandit'])
    expect(f).toEqual({ attacker: 'kay', defender: 'bandit' })
    f = setScripted(f, true)
    expect(f.strikes).toEqual([{ by: 'attacker', result: 'hit' }, { by: 'defender', result: 'hit' }])
    f = patchStrike(f, 0, { result: 'crit', damage: 5 })
    f = patchStrike(f, 1, { result: 'miss', damage: 3 })
    expect(f.strikes![1]).toEqual({ by: 'defender', result: 'miss' })  // a miss has no damage
    f = addStrike(f)
    expect(f.strikes![2].by).toBe('attacker')
    f = moveStrike(f, 2, 0)
    expect(f.strikes![0].by).toBe('attacker')
    f = removeStrike(f, 0)
    expect(f.strikes).toHaveLength(2)
    f = setFightFlag(setFightFlag(f, 'lethal', false), 'xp', true)
    expect(f.lethal).toBe(false)
    expect(f.xp).toBe(true)
    expect(setFightFlag(f, 'lethal', true).lethal).toBeUndefined()
    expect(setFightFlag(f, 'xp', false).xp).toBeUndefined()
    f = setFightWeapon(f, 'attacker', 'sword')
    expect(f.weapon).toEqual({ attacker: 'sword' })
    expect(setFightWeapon(f, 'attacker', '').weapon).toBeUndefined()
    expect(setScripted(f, false).strikes).toBeUndefined()
  })
  it('says what the loader refuses', () => {
    const ids = ['kay', 'bandit']
    expect(fightProblems(doc, { fight: { attacker: 'kay', defender: 'kay' } }, ids)).toEqual(['a unit cannot fight itself.'])
    expect(fightProblems(doc, { fight: { attacker: 'kay', defender: 'ghost' } }, ids)[0]).toMatch(/not a named unit/)
    expect(fightProblems(doc, { fight: { attacker: 'kay', defender: 'bandit', strikes: [{ by: 'attacker', result: 'miss', damage: 3 }] } }, ids)).toEqual(['strike 1: a miss does no damage.'])
    expect(fightProblems(doc, { fight: { attacker: 'kay', defender: 'bandit', strikes: [] } }, ids)[0]).toMatch(/1 to 24/)
    expect(fightProblems(doc, { fight: { attacker: 'kay', defender: 'bandit', weapon: { attacker: 'nope' } } }, ids)[0]).toMatch(/not a weapon/)
    expect(fightProblems(doc, { fight: { attacker: 'kay', defender: 'bandit', strikes: [{ by: 'attacker', result: 'hit', damage: 0 }] } }, ids)).toEqual([])
  })
  it('reads as a sentence', () => {
    const map = doc.maps[mapIdOf(doc)]!
    const text = fightText({ doc, map }, { fight: { attacker: 'a', defender: 'b', strikes: [{ by: 'attacker', result: 'hit' }], lethal: false, xp: true } })
    expect(text).toMatch(/1 strike written down, nobody dies, gives experience/)
  })
})

describe('condition damage that varies', () => {
  it('reads, writes and tells a range from a number', () => {
    let a = amountOf({ hurt: [1, 3] }, 'hurt')
    expect(a).toMatchObject({ kind: 'range', value: 1, max: 3, canKill: false })
    expect(amountValue(a)).toEqual([1, 3])
    a = { ...a, canKill: true }
    expect(amountValue(a)).toEqual({ amount: [1, 3], canKill: true })
    expect(amountOf({ hurt: { amount: [2, 9], lethal: true } }, 'hurt')).toMatchObject({ kind: 'range', value: 2, max: 9, canKill: true })
    expect(amountOf({ hurt: 5 }, 'hurt').kind).toBe('number')
    expect(amountKind(amountOf({ hurt: 5 }, 'hurt'), 'range')).toMatchObject({ kind: 'range', value: 1, max: 3 })
    expect(amountRange(a, 4, 2)).toMatchObject({ value: 4, max: 5 })  // the highest stays above the lowest
    expect(withAmount({ hurt: 5, to: 'self' }, 'hurt', amountKind(amountOf({ hurt: 5 }, 'hurt'), 'range'))).toEqual({ hurt: [1, 3], to: 'self' })
  })
})

describe('summon follow-ups', () => {
  const summonAbility = (d: ProjectDoc): Ability => ({ id: 's', name: 'S', target: 'cell', range: [1, 2], effect: { summon: { type: d.units![0].id } } })
  it('adds, changes and removes candidates; defaults are not written', () => {
    const d = base()
    let a = addSummonItem(d, summonAbility(d))
    expect(summonItemRows(a)).toHaveLength(1)
    expect(a.effect!.summon!.items).toEqual([{ item: summonItemRows(a)[0].item }])
    a = addSummonItem(d, a)
    expect(summonItemRows(a)[1].minLevel).toBe(2)
    a = patchSummonItem(a, 0, { minLevel: 5 })
    expect(a.effect!.summon!.items![0].minLevel).toBe(5)
    a = setSummon(a, { pick: 'random', replace: true })
    expect(a.effect!.summon).toMatchObject({ pick: 'random', replace: true })
    a = setSummon(a, { pick: 'highest', replace: false })
    expect(a.effect!.summon!.pick).toBeUndefined()
    expect(a.effect!.summon!.replace).toBeUndefined()
    a = removeSummonItem(removeSummonItem(a, 0), 0)
    expect(a.effect!.summon!.items).toBeUndefined()  // no candidates: no key (and no pick)
  })
  it('says what the loader refuses', () => {
    const d = base()
    const a = summonAbility(d)
    expect(summonItemProblems(d, { ...a, effect: { summon: { type: d.units![0].id, pick: 'random' } } })[0]).toMatch(/goes with a list of items/)
    expect(summonItemProblems(d, setSummon(a, { items: [{ item: 'nope' }] }))[0]).toMatch(/not an item/)
    expect(summonItemProblems(d, setSummon(a, { items: [{ item: d.items![0].id, minLevel: 0 }] }))[0]).toMatch(/level must be/)
    expect(abilityChecks({ ...d, abilities: [setSummon(a, { items: [{ item: 'nope' }] })] }).some((c) => /not an item/.test(c.text))).toBe(true)
  })
})

describe('the battle scene tables and weather', () => {
  const withArt = (): ProjectDoc => {
    let d = base()
    const sheet = d.graphics?.sheets?.[0]
    expect(sheet).toBeTruthy()
    d = addBackground(d, 'meadow')
    d = addBackground(d, 'cave')
    d = addPlatform(d, 'grass')
    d = addPlatform(d, 'slab')
    return d
  }
  it('a map table writes only what differs and drops empty tables', () => {
    let d = withArt()
    const m = mapIdOf(d)
    const terrain = d.terrain![0].id
    d = setMapTableEntry(d, m, 'battleBackgrounds', terrain, 'cave')
    d = setMapTableEntry(d, m, 'battleBackgrounds', 'default', 'meadow')
    expect(mapTable(d.maps[m]!, 'battleBackgrounds')).toEqual({ [terrain]: 'cave', default: 'meadow' })
    expect(mapTableProblems(d, d.maps[m]!)).toEqual([])
    d = setMapTableEntry(d, m, 'battleBackgrounds', terrain, '')
    d = setMapTableEntry(d, m, 'battleBackgrounds', 'default', '')
    expect(d.maps[m]!.battleBackgrounds).toBeUndefined()
  })
  it('removing a background, a platform or a terrain clears what named it, on every map', () => {
    let d = withArt()
    const m = mapIdOf(d)
    const terrain = d.terrain![0].id
    d = setMapTableEntry(setMapTableEntry(d, m, 'battleBackgrounds', terrain, 'cave'), m, 'battlePlatforms', terrain, 'slab')
    d = setTerrainPlatform(d, terrain, 'grass')
    d = setDefaultPlatform(d, 'slab')
    expect(removeBackground(d, 'cave').maps[m]!.battleBackgrounds).toBeUndefined()
    const noSlab = removePlatform(d, 'slab')
    expect(noSlab.maps[m]!.battlePlatforms).toBeUndefined()
    expect(noSlab.graphics!.battle!.defaultPlatform).toBeUndefined()
    expect(noSlab.graphics!.battle!.platforms!.slab).toBeUndefined()
    const noTerrain = dropTerrainFromTables(d, terrain)
    expect(noTerrain.maps[m]!.battleBackgrounds).toBeUndefined()
    expect(noTerrain.graphics!.battle!.terrainPlatform).toBeUndefined()
  })
  it('names an unknown terrain or picture in plain words', () => {
    let d = withArt()
    const m = mapIdOf(d)
    d = setMapTableEntry(d, m, 'battlePlatforms', 'lava', 'slab')
    d = setMapTableEntry(d, m, 'battleBackgrounds', 'default', 'ghost')
    const problems = mapTableProblems(d, d.maps[m]!)
    expect(problems.some((p) => /terrain “lava”/.test(p))).toBe(true)
    expect(problems.some((p) => /background “ghost”/.test(p))).toBe(true)
  })
  it('platforms: a far picture, and the default id "default" is refused', () => {
    let d = withArt()
    d = setPlatformPicture(d, 'grass', d.graphics!.sheets[0].id, [1, 0], true)
    expect(platformList(d).find((p) => p.id === 'grass')!.far).toEqual([1, 0])
  })
  it('weather: a map starts with it, a terrain has costs per movement type, empty rows vanish', () => {
    let d = base()
    const m = mapIdOf(d)
    const terrain = d.terrain![0].id
    const mt = d.game!.moveTypes[0].id
    d = setMapWeather(d, m, 'rain')
    expect(d.maps[m]!.weather).toBe('rain')
    expect(setMapWeather(d, m, '').maps[m]!.weather).toBeUndefined()
    d = setWeatherCost(d, terrain, 'rain', mt, 4)
    expect(weatherCostOf(d, terrain, 'rain', mt)).toBe(4)
    expect(d.terrain![0].weatherCost).toEqual({ rain: { [mt]: 4 } })
    expect(weatherCostProblems(d, terrain)).toEqual([])
    d = setWeatherCost(d, terrain, 'rain', mt, 999)
    expect(weatherCostOf(d, terrain, 'rain', mt)).toBe(99)
    d = setWeatherCost(d, terrain, 'rain', mt, undefined)
    expect(d.terrain![0].weatherCost).toBeUndefined()
    const bad = { ...d, terrain: [{ ...d.terrain![0], weatherCost: { rain: { boat: 2, [mt]: 0 } } as never }, ...d.terrain!.slice(1)] }
    const problems = weatherCostProblems(bad, terrain)
    expect(problems.some((p) => /movement type “boat”/.test(p))).toBe(true)
    expect(problems.some((p) => /from 1 to 99/.test(p))).toBe(true)
  })
  it('the game choices: defaults are not written; a check where a choice changes nothing', () => {
    let d = base()
    expect(backgroundFrom(d)).toBe('defender')
    d = setBackgroundFrom(d, 'attacker')
    expect(d.game!.battleBackgroundFrom).toBe('attacker')
    expect(setBackgroundFrom(d, 'defender').game!.battleBackgroundFrom).toBeUndefined()
    expect(sceneSides(d)).toBe('map')
    d = setSceneSides(d, 'playerRight')
    expect(d.game!.battleSceneSides).toBe('playerRight')
    expect(setSceneSides(d, 'map').game!.battleSceneSides).toBeUndefined()
    expect(sceneChoiceProblems(setSceneSides(d, 'map'))).toEqual([])
    const bogus = { ...d, game: { ...d.game!, battleSceneSides: 'center' as never } }
    expect(sceneChoiceProblems(bogus)[0]).toMatch(/must be by the map/)
  })
})
