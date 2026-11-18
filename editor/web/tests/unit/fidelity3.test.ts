// "Fidelity 3": an ability that spends its item, summon candidates with weights and a ceiling, a promotion level per character, weather that closes
// a way, several preparation shops, the new sound slots and the small leftovers (model/abilityItem.ts, summonItems.ts, ...).
import { describe, expect, it } from 'vitest'
import { abilityChecks } from '../../src/model/abilityChecks'
import { abilityGivers, setSpendsItem, spendsItemProblems } from '../../src/model/abilityItem'
import { setSummon } from '../../src/model/abilities'
import { addLevelFor, characterPromotionLevel, followCharacterInPromotions, levelForRows, promotionLevelChecks, setCharacterPromotionLevel, setLevelFor } from '../../src/model/promotionLevels'
import { renameCharacter } from '../../src/model/characterRename'
import { deleteCharacter } from '../../src/model/srpgRefs'
import { setWeatherCost, weatherCostOf, weatherCostProblems } from '../../src/model/battleTables'
import { addShop, patchShop, removeShop, setSingleShopMusic, shopChecks, shopsOf, toSeveralShops, toSingleShop } from '../../src/model/prepShops'
import { renameAudioId } from '../../src/model/audioRefs'
import type { MapFile } from '../../src/model/types'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { addSummonItem, MAX_SUMMON_ITEMS, NO_ITEM_VALUE, patchSummonItem, summonItemProblems, summonItemRows } from '../../src/model/summonItems'
import type { Ability } from '../../src/model/srpgTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => switchOnFeature(loadDemo('demo-srpg'), 'abilities')

describe('an ability that spends its item', () => {
  const withWand = (uses: number | undefined): ProjectDoc => {
    const d = base()
    return {
      ...d,
      skills: [...(d.skills ?? []), { id: 'zapper', name: 'Zapper', blocks: [{ when: 'always', do: [{ ability: 'zap' }] }] }],
      items: [...(d.items ?? []), { id: 'wand', name: 'Wand', kind: 'accessory', skills: ['zapper'], ...(uses === undefined ? {} : { uses }) }],
      abilities: [{ id: 'zap', name: 'Zap', target: 'ally', effect: { heal: 5 }, spendsItem: true }],
    }
  }
  it('finds the items that give an ability', () => {
    expect(abilityGivers(withWand(3), 'zap').map((i) => i.id)).toEqual(['wand'])
    expect(abilityGivers(base(), 'zap')).toEqual([])
  })
  it('writes true only, never false', () => {
    const a: Ability = { id: 'x', name: 'X', target: 'self', effect: { heal: 1 } }
    expect(setSpendsItem(a, true).spendsItem).toBe(true)
    expect('spendsItem' in setSpendsItem(setSpendsItem(a, true), false)).toBe(false)
  })
  it('says what the loader refuses', () => {
    expect(spendsItemProblems(withWand(3), withWand(3).abilities![0])).toEqual([])
    expect(spendsItemProblems(withWand(undefined), withWand(undefined).abilities![0])[0]).toMatch(/unlimited uses/)
    const d = base()
    const a: Ability = { id: 'zap', name: 'Zap', target: 'ally', effect: { heal: 5 }, spendsItem: true }
    expect(spendsItemProblems(d, a)[0]).toMatch(/No item gives/)
    expect(abilityChecks({ ...d, abilities: [a] }).some((c) => /No item gives/.test(c.text))).toBe(true)
    const listed = { ...withWand(3), units: (withWand(3).units ?? []).map((u, i) => (i === 0 ? { ...u, abilities: ['zap'] } : u)) }
    expect(spendsItemProblems(listed, listed.abilities![0]).some((p) => /only an item may give it/.test(p))).toBe(true)
  })
})

describe('summon candidates: ceiling, weight, no item, 32 of them', () => {
  const ability = (d: ProjectDoc): Ability => ({ id: 's', name: 'S', target: 'cell', range: [1, 2], effect: { summon: { type: d.units![0].id } } })
  it('writes only numbers that are not the default', () => {
    const d = base()
    let a = addSummonItem(d, ability(d))
    a = patchSummonItem(a, 0, { maxLevel: 6, weight: 3 })
    expect(a.effect!.summon!.items![0]).toMatchObject({ maxLevel: 6, weight: 3 })
    a = patchSummonItem(a, 0, { weight: 1, maxLevel: undefined })
    expect(a.effect!.summon!.items).toEqual([{ item: summonItemRows(a)[0].item }])
  })
  it('the "no item" candidate is written as null and read back', () => {
    const d = base()
    let a = addSummonItem(d, ability(d))
    a = patchSummonItem(a, 0, { item: NO_ITEM_VALUE })
    expect(a.effect!.summon!.items![0].item).toBeNull()
    expect(summonItemRows(a)[0].item).toBe(NO_ITEM_VALUE)
    expect(summonItemProblems(d, a)).toEqual([])
  })
  it('allows 32 candidates and says what the loader refuses', () => {
    const d = base()
    const item = d.items![0].id
    expect(MAX_SUMMON_ITEMS).toBe(32)
    const many = setSummon(ability(d), { items: Array.from({ length: 32 }, () => ({ item })) })
    expect(summonItemProblems(d, many)).toEqual([])
    expect(summonItemProblems(d, setSummon(ability(d), { items: Array.from({ length: 33 }, () => ({ item })) }))[0]).toMatch(/1 to 32/)
    expect(summonItemProblems(d, setSummon(ability(d), { items: [{ item, weight: 0 }] }))[0]).toMatch(/weight/)
    expect(summonItemProblems(d, setSummon(ability(d), { items: [{ item, weight: 1001 }] }))[0]).toMatch(/weight/)
    expect(summonItemProblems(d, setSummon(ability(d), { items: [{ item, minLevel: 5, maxLevel: 4 }] }))[0]).toMatch(/highest level/)
  })
})

describe('a promotion level per character', () => {
  const withPromotion = (): ProjectDoc => {
    const d = base()
    const [from, to] = [d.units![0].id, d.units![1].id]
    return { ...d, characters: [{ id: 'eirik', name: 'Eirik', type: from }], units: d.units!.map((u) => (u.id === from ? { ...u, promotions: [{ to, fromLevel: 8 }] } : u)) }
  }
  it('sets and clears the character level; the key goes with the last value', () => {
    const d = withPromotion()
    const c = d.characters![0].id
    let n = setCharacterPromotionLevel(d, c, 3)
    expect(characterPromotionLevel(n, c)).toBe(3)
    expect(n.characters![0].promotion).toEqual({ fromLevel: 3 })
    n = setCharacterPromotionLevel(n, c, undefined)
    expect('promotion' in n.characters![0]).toBe(false)
  })
  it('adds, sets and removes the rows of a promotion entry', () => {
    const d = withPromotion()
    const from = d.units![0].id
    let n = addLevelFor(d, from, 0)
    expect(levelForRows(n, from, 0)).toEqual([{ character: d.characters![0].id, level: 8 }])
    n = setLevelFor(n, from, 0, d.characters![0].id, 2)
    expect(n.units![0].promotions![0].fromLevelFor).toEqual({ [d.characters![0].id]: 2 })
    n = setLevelFor(n, from, 0, d.characters![0].id, undefined)
    expect('fromLevelFor' in n.units![0].promotions![0]).toBe(false)
  })
  it('follows a renamed character and drops a deleted one', () => {
    const d = withPromotion()
    const from = d.units![0].id, c = d.characters![0].id
    const n = setLevelFor(d, from, 0, c, 2)
    expect(levelForRows(renameCharacter(n, c, 'zz_new').doc, from, 0)).toEqual([{ character: 'zz_new', level: 2 }])
    expect(levelForRows(deleteCharacter(n, c), from, 0)).toEqual([])
    expect(followCharacterInPromotions(d, c, undefined)).toBe(d)
  })
  it('says what the loader refuses', () => {
    const d = withPromotion()
    const from = d.units![0].id
    expect(promotionLevelChecks(d)).toEqual([])
    expect(promotionLevelChecks(setCharacterPromotionLevel(d, d.characters![0].id, 99)).length).toBe(1)
    expect(promotionLevelChecks(setLevelFor(d, from, 0, 'nobody', 2))[0].text).toMatch(/not a character/)
    expect(promotionLevelChecks(setLevelFor(d, from, 0, d.characters![0].id, 0))[0].text).toMatch(/whole number/)
  })
})

describe('weather that closes a way', () => {
  it('"blocked" is written and read back; clearing it removes the entry', () => {
    const d = base()
    const terrain = d.terrain![0].id, mt = d.game!.moveTypes[0].id
    let n = setWeatherCost(d, terrain, 'rain', mt, 'blocked')
    expect(weatherCostOf(n, terrain, 'rain', mt)).toBe('blocked')
    expect(n.terrain![0].weatherCost).toEqual({ rain: { [mt]: 'blocked' } })
    expect(weatherCostProblems(n, terrain)).toEqual([])
    n = setWeatherCost(n, terrain, 'rain', mt, undefined)
    expect(n.terrain![0].weatherCost).toBeUndefined()
  })
  it('a plain number is still clamped to 1..99 and a stray word is a problem', () => {
    const d = base()
    const terrain = d.terrain![0].id, mt = d.game!.moveTypes[0].id
    expect(weatherCostOf(setWeatherCost(d, terrain, 'snow', mt, 500), terrain, 'snow', mt)).toBe(99)
    const bad = { ...d, terrain: d.terrain!.map((t, i) => (i === 0 ? { ...t, weatherCost: { rain: { [mt]: 'closed' as unknown as number } } } : t)) }
    expect(weatherCostProblems(bad, terrain)[0]).toMatch(/whole number from 1 to 99, or closed/)
  })
})

describe('several preparation shops', () => {
  const mapWithShop = (): MapFile => ({ prep: { slots: [{ x: 0, y: 0 }], shop: { sells: ['potion'] } } } as unknown as MapFile)
  it('turns the single shop into the first of a list and back', () => {
    const m = toSeveralShops(mapWithShop())
    expect(m.prep!.shop).toBeUndefined()
    expect(shopsOf(m)).toEqual([{ id: 'shop', name: 'Shop', sells: ['potion'] }])
    const back = toSingleShop(m)
    expect(back.prep!.shops).toBeUndefined()
    expect(back.prep!.shop).toEqual({ sells: ['potion'] })
    expect(toSingleShop(addShop(m))).toEqual(addShop(m))  // two shops: not collapsed
  })
  it('adds shops with free ids and names, patches them and drops defaults', () => {
    let m = toSeveralShops(mapWithShop())
    m = addShop(m)
    expect(shopsOf(m).map((s) => [s.id, s.name])).toEqual([['shop', 'Shop'], ['shop2', 'Shop 2']])
    m = patchShop(m, 1, { buys: false, music: 'theme', name: 'Market' })
    expect(shopsOf(m)[1]).toMatchObject({ buys: false, music: 'theme', name: 'Market' })
    m = patchShop(m, 1, { buys: true, music: undefined })
    expect(shopsOf(m)[1]).toEqual({ id: 'shop2', name: 'Market', sells: [] })
    m = removeShop(removeShop(m, 1), 0)
    expect(m.prep!.shops).toBeUndefined()
  })
  it('music on the single shop', () => {
    const m = setSingleShopMusic(mapWithShop(), 'inn')
    expect(m.prep!.shop).toEqual({ sells: ['potion'], music: 'inn' })
    expect(setSingleShopMusic(m, undefined).prep!.shop).toEqual({ sells: ['potion'] })
  })
  it('says what the loader refuses', () => {
    const d = base()
    const priced = d.items!.find((i) => i.price && Object.keys(i.price).length)!.id
    const good = { ...mapWithShop(), prep: { slots: [{ x: 0, y: 0 }], shops: [{ id: 'a', name: 'A', sells: [priced] }, { id: 'b', name: 'B', sells: [priced] }] } } as unknown as MapFile
    expect(shopChecks(d, good)).toEqual([])
    const bad = { ...good, prep: { ...good.prep!, shop: { sells: [priced] }, shops: [{ id: 'a', name: 'Same', sells: [] }, { id: 'a', name: 'Same', sells: ['nope'], music: 'nothing' }] } } as MapFile
    const texts = shopChecks(d, bad).join('\n')
    expect(texts).toMatch(/both one shop and a list/)
    expect(texts).toMatch(/sells nothing/)
    expect(texts).toMatch(/used twice/)
    expect(texts).toMatch(/is not an item/)
    expect(texts).toMatch(/not a music track/)
  })
  it('a renamed track is followed by the shops that play it', () => {
    const d = base()
    const mapId = Object.keys(d.maps)[0]
    const withAudio = { ...d, audio: { music: { theme: { file: 'audio/t.ogg' } } }, maps: { ...d.maps, [mapId]: { ...d.maps[mapId]!, prep: { slots: [{ x: 0, y: 0 }], shops: [{ id: 'a', name: 'A', sells: ['potion'], music: 'theme' }] } } } } as unknown as ProjectDoc
    const renamed = renameAudioId(withAudio, 'music', 'theme', 'newtheme')
    expect(renamed.maps[mapId]!.prep!.shops![0].music).toBe('newtheme')
  })
})
