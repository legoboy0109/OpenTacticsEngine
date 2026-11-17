// The preparation screen in the editor's model (docs/EVENTS.md "Preparation"): switching it on and off, slots, the limit,
// required / forbidden / fixed characters, the shop, the checks (which mirror what the game loader refuses) and resizing.
import { describe, expect, it } from 'vitest'
import { mapWarnings } from '../../src/model/mapChecks'
import { ctxOf, editMap } from '../../src/model/mapCtx'
import { resizeLoss, resizeMap } from '../../src/model/mapResize'
import {
  addFixed, addForbidden, addRequired, disablePrep, enablePrep, removeForbidden, removeRequired, removeSlot, requiredId, requiredSlot,
  setLimit, setRequiredSlot, setShop, slotOverlays, toggleSlot,
} from '../../src/model/prep'
import { prepChecks } from '../../src/model/prepChecks'
import { peopleOf } from '../../src/model/prepRoster'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadPrepFixture } from './fixtures'

const doc = (): ProjectDoc => loadPrepFixture()
const ctx = (d: ProjectDoc) => ctxOf(d)!
const texts = (d: ProjectDoc, id: string) => prepChecks(d, id).map((c) => c.text)
const edit = (d: ProjectDoc, id: string, fn: (m: MapFile) => MapFile): ProjectDoc => editMap(d, id, (m) => fn(m))

describe('the fixture', () => {
  it('has a preparation on the orchard and the hilltop and none on the meadow', () => {
    const d = doc()
    expect(d.maps.meadow!.prep).toBeUndefined()
    expect(d.maps.orchard!.prep!.slots).toHaveLength(4)
    expect(d.maps.orchard!.prep!.limit).toBe(3)
    expect(d.maps.hilltop!.prep!.forbidden).toEqual(['wren'])
  })
  it('is in good order: no checks', () => {
    const d = doc()
    expect(texts(d, 'orchard')).toEqual([])
    expect(texts(d, 'hilltop')).toEqual([])
    expect(texts(d, 'meadow')).toEqual([])  // no preparation: nothing to say
  })
  it('names the people a preparation can pick: the party, then characters nobody places', () => {
    expect(peopleOf(doc()).map((p) => p.id)).toEqual(['sera', 'tomas', 'wren', 'bram', 'guard', 'ilsa'])
  })
})

describe('switching on and off', () => {
  it('on: the squares of the named units of the first human side become the slots, the limit is their number', () => {
    const d = doc()
    const on = edit(d, 'meadow', (m) => enablePrep(m, ctx(d)))
    expect(on.maps.meadow!.prep).toEqual({ slots: [{ x: 0, y: 1 }, { x: 0, y: 2 }, { x: 0, y: 3 }, { x: 1, y: 2 }], limit: 4 })
    expect(enablePrep(on.maps.meadow!, ctx(on))).toBe(on.maps.meadow)  // already on: the same map
  })
  it('on a map with no named units: one free square', () => {
    const d = doc()
    const bare = edit(d, 'meadow', (m) => ({ ...m, units: (m.units ?? []).filter((u) => u.side === 'red') }))
    const on = edit(bare, 'meadow', (m) => enablePrep(m, ctx(bare)))
    expect(on.maps.meadow!.prep!.slots).toEqual([{ x: 0, y: 0 }])
  })
  it('off removes the whole block', () => {
    const d = doc()
    const off = edit(d, 'orchard', disablePrep)
    expect('prep' in off.maps.orchard!).toBe(false)
    expect(disablePrep(off.maps.orchard!)).toBe(off.maps.orchard)
  })
})

describe('slots', () => {
  it('a click on an empty square adds the next slot, a click on a slot removes it', () => {
    const d = doc()
    const orchard = d.maps.orchard!
    const added = toggleSlot(orchard, ctx(d), 5, 2)
    expect(added.error).toBeNull()
    expect(added.added).toBe(true)
    expect(added.map.prep!.slots[4]).toEqual({ x: 5, y: 2 })
    const removed = toggleSlot(added.map, ctx(d), 5, 2)
    expect(removed.added).toBe(false)
    expect(removed.map.prep!.slots).toHaveLength(4)
  })
  it('refuses a square outside, one with a unit that stays, one nobody can stand on, and a map without a preparation', () => {
    const d = doc()
    const orchard = d.maps.orchard!
    expect(toggleSlot(orchard, ctx(d), 20, 2).error).toMatch(/outside the map/)
    expect(toggleSlot(orchard, ctx(d), 2, 5).error).toMatch(/stays where it is placed.*Guard/)  // the fixed guard
    expect(toggleSlot(orchard, ctx(d), 7, 1).error).toMatch(/stays where it is placed/)         // a raider
    expect(toggleSlot(d.maps.meadow!, ctx(d), 5, 2).error).toMatch(/Switch the preparation on first/)
    // A choosable unit's own square is fine: the slots decide where it starts.
    expect(toggleSlot(orchard, ctx(d), 3, 4).error).toBeNull()
    // Nobody can stand on a pond that foot and wing both cannot cross.
    const rock = { ...d, units: d.units!.map((u) => ({ ...u })), terrain: d.terrain!.map((t) => (t.id === 'pond' ? { ...t, moveCost: { foot: 0, wing: 0 } } : t)) } as ProjectDoc
    expect(toggleSlot(rock.maps.orchard!, ctx(rock), 4, 3).error).toMatch(/No kind of unit can stand/)
  })
  it('removing a slot renumbers the later ones, unfixes the required character on it and lowers the limit', () => {
    const d = doc()
    let m = d.maps.orchard!
    m = setRequiredSlot(addRequired(m, 'tomas').map, 'tomas', 3).map  // sera on 1, tomas on 3 (limit 3: two required fit)
    expect(m.prep!.required).toEqual([{ character: 'sera', slot: 1 }, { character: 'tomas', slot: 3 }])
    const without2 = removeSlot(m, 1)
    expect(without2.prep!.slots).toHaveLength(3)
    expect(without2.prep!.required).toEqual([{ character: 'sera', slot: 1 }, { character: 'tomas', slot: 2 }])
    const without1 = removeSlot(m, 2)  // tomas' slot is gone: tomas stays required, any slot
    expect(without1.prep!.required).toEqual([{ character: 'sera', slot: 1 }, 'tomas'])
    expect(removeSlot(removeSlot(m, 3), 2).prep!.limit).toBe(2)  // 2 slots left: the limit follows
    expect(removeSlot(m, 9)).toBe(m)
  })
  it('the slots are drawn as numbered overlays', () => {
    const o = slotOverlays(doc().maps.orchard!)
    expect(o.map((x) => x.label)).toEqual(['1', '2', '3', '4'])
    expect(o.every((x) => x.tone === 'slot' && x.w === 1 && x.h === 1)).toBe(true)
    expect(slotOverlays(doc().maps.meadow!)).toEqual([])
  })
})

describe('limit, required, forbidden, fixed', () => {
  it('the limit stays between the required characters and the slots', () => {
    const m = doc().maps.orchard!
    expect(setLimit(m, 99).prep!.limit).toBe(4)
    expect(setLimit(m, 0).prep!.limit).toBe(1)
    expect(setLimit(m, 2).prep!.limit).toBe(2)
    expect(setLimit(m, 3)).toBe(m)
    const two = addRequired(m, 'tomas').map
    expect(setLimit(two, 1).prep!.limit).toBe(2)  // two are required
  })
  it('requiring a character moves it out of forbidden and fixed; the limit caps how many', () => {
    const d = doc()
    let m = edit(d, 'orchard', (x) => addForbidden(x, 'wren')).maps.orchard!
    expect(m.prep!.forbidden).toEqual(['wren'])
    m = addRequired(m, 'wren').map
    expect(m.prep!.forbidden).toBeUndefined()  // empty lists are not written
    expect(m.prep!.required!.map(requiredId)).toEqual(['sera', 'wren'])
    m = addRequired(m, 'tomas').map
    const refused = addRequired(m, 'bram')
    expect(refused.error).toMatch(/The limit is 3/)
    expect(refused.map).toBe(m)
    expect(removeRequired(m, 'wren').prep!.required!.map(requiredId)).toEqual(['sera', 'tomas'])
  })
  it('one slot can be fixed for one character only', () => {
    let m = addRequired(doc().maps.orchard!, 'tomas').map
    expect(setRequiredSlot(m, 'tomas', 1).error).toMatch(/Slot 1 is already fixed for sera/)
    expect(setRequiredSlot(m, 'tomas', 9).error).toMatch(/does not exist/)
    m = setRequiredSlot(m, 'tomas', 2).map
    expect(requiredSlot(m.prep!.required![1])).toBe(2)
    m = setRequiredSlot(m, 'tomas', undefined).map
    expect(m.prep!.required![1]).toBe('tomas')
  })
  it('forbidden and fixed lists exclude each other and required', () => {
    let m = doc().maps.orchard!
    m = addForbidden(m, 'sera')  // sera was required: now she is not
    expect(m.prep!.required).toBeUndefined()
    expect(m.prep!.forbidden).toEqual(['sera'])
    m = addFixed(m, 'sera')
    expect(m.prep!.forbidden).toBeUndefined()
    expect(m.prep!.fixed).toEqual(['guard', 'sera'])
    expect(removeForbidden(m, 'nobody')).toEqual(m)
  })
})

describe('the shop', () => {
  it('null takes it away, a list keeps it', () => {
    const m = doc().maps.orchard!
    expect(setShop(m, null).prep!.shop).toBeUndefined()
    expect(setShop(m, ['ring']).prep!.shop).toEqual({ sells: ['ring'] })
    expect(setShop(m, []).prep!.shop).toEqual({ sells: [] })
  })
})

describe('checks', () => {
  const orchard = (d: ProjectDoc, fn: (m: MapFile) => MapFile) => edit(d, 'orchard', fn)
  it('no slots, a limit beyond the slots, more required than the limit', () => {
    const d = doc()
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, slots: [] } })), 'orchard').join('|')).toMatch(/no deployment squares/)
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, limit: 6 } })), 'orchard').join('|')).toMatch(/limit is 6 but the map has only 4 slots/)
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, limit: 1, required: ['sera', 'tomas'] } })), 'orchard').join('|')).toMatch(/2 characters are required but only 1 may be deployed/)
  })
  it('a required character the party never has; one fixed to a slot its type cannot stand on', () => {
    const d = doc()
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, required: ['sera', 'ghost'] } })), 'orchard').join('|')).toMatch(/ghost is required here, but no map places/)
    // tomas (an archer, foot) fixed to slot 4, the pond
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, required: ['sera', { character: 'tomas', slot: 4 }] } })), 'orchard').join('|')).toMatch(/Tomas must start on slot 4, but that kind of unit cannot stand on Pond/)
    // wren flies: fine
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, required: ['sera', { character: 'wren', slot: 4 }] } })), 'orchard')).toEqual([])
  })
  it('slots on squares nobody can stand on or where a unit stays; a slot outside', () => {
    const d = doc()
    const onGuard = texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, slots: [...m.prep!.slots, { x: 2, y: 5 }], limit: 3 } })), 'orchard').join('|')
    expect(onGuard).toMatch(/Slot 5 at 3,6 is where Guard stands, and that unit stays/)
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, slots: [...m.prep!.slots, { x: 40, y: 1 }] } })), 'orchard').join('|')).toMatch(/Slot 5 is outside the map/)
    const rock = { ...d, terrain: d.terrain!.map((t) => (t.id === 'pond' ? { ...t, moveCost: { foot: 0, wing: 0 } } : t)) } as ProjectDoc
    expect(texts(rock, 'orchard').join('|')).toMatch(/Slot 4 at 2,1 is on Pond that no kind of unit can stand on/)
  })
  it('fixed that is not placed, forbidden that is placed, the shop', () => {
    const d = doc()
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, fixed: ['nobody'] } })), 'orchard').join('|')).toMatch(/"nobody" is listed as a fixed unit but this map does not place/)
    expect(texts(orchard(d, (m) => ({ ...m, prep: { ...m.prep!, forbidden: ['ilsa'] } })), 'orchard').join('|')).toMatch(/Ilsa is forbidden, but this map places that unit/)
    expect(texts(orchard(d, (m) => setShop(m, ['token'])), 'orchard').join('|')).toMatch(/sells Token, which has no price/)
    expect(texts(orchard(d, (m) => setShop(m, ['unknown'])), 'orchard').join('|')).toMatch(/"unknown", which is not an item/)
    expect(texts(orchard(d, (m) => setShop(m, [])), 'orchard').join('|')).toMatch(/sells nothing/)
  })
  it('outside a campaign: worth a look', () => {
    const d = doc()
    const none = { ...d, campaign: null } as ProjectDoc
    expect(texts(none, 'orchard').join('|')).toMatch(/no campaign, so the preparation screen only works as a free battle/)
    const unused = { ...d, campaign: { ...d.campaign!, chapters: d.campaign!.chapters.filter((c) => c.map !== 'orchard') } } as ProjectDoc
    expect(texts(unused, 'orchard').join('|')).toMatch(/no chapter of the campaign uses it/)
  })
  it('the map editor shows them with the other warnings, with the square when there is one', () => {
    const d = orchard(doc(), (m) => ({ ...m, prep: { ...m.prep!, slots: [...m.prep!.slots, { x: 2, y: 5 }], limit: 3 } }))
    const w = mapWarnings(d, d.maps.orchard!).filter((x) => x.key.startsWith('prep:'))
    expect(w).toHaveLength(1)
    expect(w[0].cell).toEqual({ x: 2, y: 5 })
  })
})

describe('resizing', () => {
  it('slots that fall outside are dropped (the rest follow the map, later numbers close up) and the dialog hears of them', () => {
    const d = doc()
    const orchard = d.maps.orchard!
    const loss = resizeLoss(orchard, d.structures ?? [], 8, 3, 'nw')
    expect(loss.slots).toEqual([3])  // slot 3 is at (0,3): outside 8 x 3
    const small = resizeMap(orchard, d.terrain!, d.structures ?? [], 8, 3, 'nw')
    expect(small.prep!.slots).toEqual([{ x: 0, y: 1 }, { x: 0, y: 2 }, { x: 1, y: 0 }])
    expect(small.prep!.limit).toBe(3)
    const moved = resizeMap(orchard, d.terrain!, d.structures ?? [], 9, 7, 'se')
    expect(moved.prep!.slots[0]).toEqual({ x: 1, y: 2 })
  })
})
