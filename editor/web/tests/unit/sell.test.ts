// Selling items back in the editor's model (docs/EVENTS.md "Preparation"): the shop's "buys", an item's "sellable", the game's
// sell fraction, and the checks that mirror what the game loader refuses.
import { describe, expect, it } from 'vitest'
import { itemChecks } from '../../src/model/itemChecks'
import { setShop } from '../../src/model/prep'
import { prepChecks } from '../../src/model/prepChecks'
import { gameHasBuyingShop, sellPercent, sellChecks, setSellPercent, setSellable, setShopBuys, shopBuys } from '../../src/model/sell'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadPrepFixture } from './fixtures'

const doc = (): ProjectDoc => loadPrepFixture()
const orchard = (d: ProjectDoc): MapFile => d.maps.orchard!

describe('the shop buys switch', () => {
  it('is on by default and writes buys: false only when off', () => {
    const m = orchard(doc())
    expect(shopBuys(m)).toBe(true)
    const off = setShopBuys(m, false)
    expect(off.prep!.shop).toEqual({ sells: ['potion', 'tonic'], buys: false })
    expect(shopBuys(off)).toBe(false)
    expect(setShopBuys(off, true).prep!.shop).toEqual({ sells: ['potion', 'tonic'] })  // the default is not written
  })
  it('is kept while the list of items changes, and leaves a map without a shop alone', () => {
    const off = setShopBuys(orchard(doc()), false)
    expect(setShop(off, ['ring']).prep!.shop).toEqual({ sells: ['ring'], buys: false })
    expect(setShop(off, null).prep!.shop).toBeUndefined()
    const none = doc().maps.hilltop!
    expect(setShopBuys(none, false)).toBe(none)
  })
  it('a shop that buys exists in the fixture, and not once every shop is switched off', () => {
    const d = doc()
    expect(gameHasBuyingShop(d)).toBe(true)
    const off = { ...d, maps: { ...d.maps, orchard: setShopBuys(orchard(d), false) } }
    expect(gameHasBuyingShop(off)).toBe(false)
  })
  it('a "buys" that is not on or off is flagged, as the loader refuses it', () => {
    const d = doc()
    const bad = { ...d, maps: { ...d.maps, orchard: { ...orchard(d), prep: { ...orchard(d).prep!, shop: { sells: ['potion'], buys: 'yes' as unknown as boolean } } } } }
    expect(prepChecks(bad, 'orchard').map((c) => c.text).join('\n')).toContain('must be on or off')
    expect(prepChecks(d, 'orchard')).toEqual([])
  })
})

describe('an item that cannot be sold', () => {
  it('writes sellable: false, and removes the key when it can be sold again', () => {
    const off = setSellable(doc(), 'potion', false)
    expect(off.items!.find((i) => i.id === 'potion')).toMatchObject({ sellable: false })
    const on = setSellable(off, 'potion', true)
    expect('sellable' in on.items!.find((i) => i.id === 'potion')!).toBe(false)
  })
  it('flags a value that is not on or off', () => {
    const d = doc()
    const bad = { ...d, items: d.items!.map((i) => (i.id === 'ring' ? { ...i, sellable: 'no' as unknown as boolean } : i)) }
    expect(sellChecks(bad).map((c) => c.text)).toEqual(['Ring: whether it can be sold must be on or off.'])
    expect(itemChecks(bad).map((c) => c.text)).toContain('Ring: whether it can be sold must be on or off.')
    expect(sellChecks(d)).toEqual([])
  })
})

describe('the share a shop pays', () => {
  it('is 50 by default, is written only when changed, and an all-default block disappears again', () => {
    const d = doc()
    expect(sellPercent(d)).toBe(50)
    const thirty = setSellPercent(d, 30)
    expect(thirty.game!.rules!.items).toMatchObject({ sellPercent: 30, inventorySize: 3 })
    expect(sellPercent(thirty)).toBe(30)
    expect('sellPercent' in setSellPercent(thirty, 50).game!.rules!.items!).toBe(false)
  })
  it('stays within 0 to 100 and whole', () => {
    const d = doc()
    expect(sellPercent(setSellPercent(d, 150))).toBe(100)
    expect(sellPercent(setSellPercent(d, -5))).toBe(0)
    expect(sellPercent(setSellPercent(d, 33.6))).toBe(34)
  })
  it('flags a hand-written value outside 0 to 100 or not whole', () => {
    const d = doc()
    for (const v of [101, -1, 12.5, '40']) {
      const bad = { ...d, game: { ...d.game!, rules: { ...d.game!.rules, items: { ...d.game!.rules!.items, sellPercent: v as unknown as number } } } }
      expect(sellChecks(bad).map((c) => c.text).join('\n')).toContain('whole number from 0 to 100')
    }
    expect(sellChecks(setSellPercent(d, 0))).toEqual([])
  })
})
