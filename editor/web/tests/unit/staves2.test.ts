// The Staff form's second stage (docs/SRPG.md section 8, "Staves: stage two"): warp, pull, swap, repair, light, an item's `repairable`.
// Uses the engine's fixture tests/fixtures/srpg-staves2, which the real loader accepts.
import { describe, expect, it } from 'vitest'
import { updateItem, setItemKind, setUnlimited } from '../../src/model/items'
import { itemChecks } from '../../src/model/itemChecks'
import { setAreaMode, setHit, setStaffTarget } from '../../src/model/staff'
import {
  healOf, lightOf, moveOf, repairOf, repairableShown, setHealKind, setHealOn, setLightOn, setLightRadius, setLightTurns, setMoveKind, setRepairKind, setRepairOn,
  setRepairUses, setRepairable, setWarpFormula, setWarpMax, setWarpMin, setWarpMode, unknownEffectCount,
} from '../../src/model/staffEffects'
import { staffChecks } from '../../src/model/staffChecks'
import { keptEffectCount } from '../../src/model/staffFx'
import type { ProjectDoc } from '../../src/model/types'
import { loadStaves2Fixture } from './fixtures'

const fx = (): ProjectDoc => loadStaves2Fixture()
const item = (d: ProjectDoc, id: string) => d.items!.find((i) => i.id === id)!
const blocking = (d: ProjectDoc) => staffChecks(d).filter((c) => c.blocks).map((c) => c.text)
const all = (d: ProjectDoc) => staffChecks(d).map((c) => c.text).join('\n')
const staves = (d: ProjectDoc) => d.items!.filter((i) => i.kind === 'staff')
/** A blank staff that heals, in a game of the fixture's. */
const base = (extra: Record<string, unknown> = {}): ProjectDoc => updateItem(fx(), 'mend_staff', { group: 'staff', range: 3, effects: [{ heal: 8 }], ...extra })

describe('the fixture', () => {
  it('has no refused staff and no item problem', () => {
    expect(blocking(fx())).toEqual([])
    expect(itemChecks(fx())).toEqual([])
  })
  it('reads every effect kind', () => {
    expect(moveOf(item(fx(), 'warp_staff'))).toMatchObject({ kind: 'warp', warp: { mode: 'upto', max: 4 } })
    expect(moveOf(item(fx(), 'banish_staff'))).toMatchObject({ kind: 'warp', warp: { mode: 'between', min: 3, max: 5 } })
    expect(moveOf(item(fx(), 'recall_staff')).kind).toBe('pull')
    expect(moveOf(item(fx(), 'swap_staff')).kind).toBe('swap')
    expect(repairOf(item(fx(), 'fix_staff'))).toMatchObject({ on: true, kind: 'full' })
    expect(repairOf(item(fx(), 'patch_staff'))).toMatchObject({ on: true, kind: 'uses', uses: 2 })
    expect(lightOf(item(fx(), 'lantern_staff'))).toEqual({ on: true, radius: 2, turns: 2 })
    expect(lightOf(item(fx(), 'spark_staff'))).toEqual({ on: true, radius: 1, turns: 1 })
    expect(healOf(item(fx(), 'kindly_pull_staff'))).toMatchObject({ on: true, kind: 'number', value: 6 })
    expect(moveOf(item(fx(), 'kindly_pull_staff')).kind).toBe('pull')
    expect(healOf(item(fx(), 'warp_staff')).on).toBe(false)
  })
  it('round-trips: setting every part to what it already is changes nothing (the same document comes back)', () => {
    const d = fx()
    for (const s of staves(d)) {
      const m = moveOf(s), r = repairOf(s), h = healOf(s), l = lightOf(s)
      expect(setMoveKind(d, s.id, m.kind)).toBe(d)
      expect(setRepairOn(d, s.id, r.on)).toBe(d)
      expect(setHealOn(d, s.id, h.on)).toBe(d)
      expect(setLightOn(d, s.id, l.on)).toBe(d)
      if (h.on) expect(setHealKind(d, s.id, h.kind)).toBe(d)
      if (r.on) expect(setRepairKind(d, s.id, r.kind)).toBe(d)
      expect(keptEffectCount(s)).toBe(0)
    }
  })
})

describe('the order of the effects does not matter', () => {
  it('a heal listed after a pull is found and edited where it stands', () => {
    const d = updateItem(fx(), 'kindly_pull_staff', { effects: [{ pull: true }, { heal: 6 }] })
    expect(healOf(item(d, 'kindly_pull_staff'))).toMatchObject({ on: true, value: 6 })
    expect(moveOf(item(d, 'kindly_pull_staff')).kind).toBe('pull')
    expect(item(setHealKind(d, 'kindly_pull_staff', 'full'), 'kindly_pull_staff').effects).toEqual([{ pull: true }, { heal: 'full' }])
    expect(item(setMoveKind(d, 'kindly_pull_staff', 'swap'), 'kindly_pull_staff').effects).toEqual([{ swap: true }, { heal: 6 }])
    expect(blocking(d)).toEqual([])
  })
  it('turning a part on appends it, turning it off removes only it', () => {
    let d = setRepairOn(base(), 'mend_staff', true)
    expect(item(d, 'mend_staff').effects).toEqual([{ heal: 8 }, { repair: 'full' }])
    d = setHealOn(d, 'mend_staff', false)
    expect(item(d, 'mend_staff').effects).toEqual([{ repair: 'full' }])
  })
})

describe('the parts and their legal combinations', () => {
  it('warp: every reach shape', () => {
    let d = setMoveKind(updateItem(fx(), 'recall_staff', { effects: [{ heal: 5 }] }), 'recall_staff', 'warp')
    expect(item(d, 'recall_staff').effects).toEqual([{ heal: 5 }, { warp: { range: 3 } }])
    d = setWarpMode(d, 'recall_staff', 'between')
    expect(item(d, 'recall_staff').effects![1]).toEqual({ warp: { range: [1, 3] } })
    d = setWarpMin(d, 'recall_staff', 2)
    d = setWarpMax(d, 'recall_staff', 50)
    expect(item(d, 'recall_staff').effects![1]).toEqual({ warp: { range: [2, 20] } })
    d = setWarpMode(d, 'recall_staff', 'formula')
    expect(typeof (item(d, 'recall_staff').effects![1] as any).warp.range).toBe('string')
    d = setWarpFormula(d, 'recall_staff', 'a.mag')
    d = setWarpMode(d, 'recall_staff', 'upto')
    expect(item(d, 'recall_staff').effects![1]).toEqual({ warp: { range: 3 } })
    expect(blocking(d)).toEqual([])
  })
  it('one move kind only: choosing another replaces it in place', () => {
    let d = fx()
    for (const k of ['pull', 'swap', 'warp'] as const) {
      d = setMoveKind(d, 'recall_staff', k)
      expect(item(d, 'recall_staff').effects).toHaveLength(1)
      expect(moveOf(item(d, 'recall_staff')).kind).toBe(k)
    }
    expect(blocking(d)).toEqual([])
  })
  it('heal with a move and heal with a repair are allowed', () => {
    for (const make of [(d: ProjectDoc) => setMoveKind(d, 'mend_staff', 'warp'), (d: ProjectDoc) => setMoveKind(d, 'mend_staff', 'pull'), (d: ProjectDoc) => setMoveKind(d, 'mend_staff', 'swap'), (d: ProjectDoc) => setRepairOn(d, 'mend_staff', true)]) {
      const d = make(base())
      expect(item(d, 'mend_staff').effects).toHaveLength(2)
      expect(blocking(d)).toEqual([])
    }
  })
  it('repair is not put with a move, either way round (the edit is simply not made)', () => {
    const withRepair = setRepairOn(base(), 'mend_staff', true)
    expect(setMoveKind(withRepair, 'mend_staff', 'warp')).toBe(withRepair)
    const withMove = setMoveKind(base(), 'mend_staff', 'warp')
    expect(setRepairOn(withMove, 'mend_staff', true)).toBe(withMove)
  })
  it('repair: all uses, or a number of uses within 1 to 999', () => {
    let d = setRepairKind(setRepairOn(base(), 'mend_staff', true), 'mend_staff', 'uses')
    expect(item(d, 'mend_staff').effects![1]).toEqual({ repair: 3 })
    d = setRepairUses(d, 'mend_staff', 5000)
    expect(item(d, 'mend_staff').effects![1]).toEqual({ repair: 999 })
    d = setRepairKind(d, 'mend_staff', 'full')
    expect(item(d, 'mend_staff').effects![1]).toEqual({ repair: 'full' })
  })
  it('a warp, pull, swap or repair brings the area back to one unit in the same step, and the area cannot be widened after', () => {
    const wide = updateItem(base(), 'mend_staff', { area: { radius: 2 } })
    for (const d of [setMoveKind(wide, 'mend_staff', 'pull'), setRepairOn(wide, 'mend_staff', true)]) {
      expect(item(d, 'mend_staff')).not.toHaveProperty('area')
      expect(setAreaMode(d, 'mend_staff', 'radius')).toBe(d)
      expect(setAreaMode(d, 'mend_staff', 'all')).toBe(d)
      expect(blocking(d)).toEqual([])
    }
    // Healing alone may still have an area.
    expect(item(setAreaMode(base(), 'mend_staff', 'all'), 'mend_staff').area).toBe('all')
  })
  it('pull and swap cannot be used on "only the user" (either way); warp and repair can', () => {
    const self = setStaffTarget(base(), 'mend_staff', 'self')
    expect(setMoveKind(self, 'mend_staff', 'pull')).toBe(self)
    expect(setMoveKind(self, 'mend_staff', 'swap')).toBe(self)
    expect(blocking(setRepairOn(setMoveKind(self, 'mend_staff', 'none'), 'mend_staff', true))).toEqual([])
    const warpSelf = setMoveKind(self, 'mend_staff', 'warp')
    expect(moveOf(item(warpSelf, 'mend_staff')).kind).toBe('warp')
    expect(blocking(warpSelf)).toEqual([])
    const pulled = setMoveKind(base(), 'mend_staff', 'pull')
    expect(setStaffTarget(pulled, 'mend_staff', 'self')).toBe(pulled)
    expect(item(setStaffTarget(setMoveKind(base(), 'mend_staff', 'warp'), 'mend_staff', 'self'), 'mend_staff')).not.toHaveProperty('range')
  })
  it('an enemy can be moved but not healed', () => {
    const foe = setStaffTarget(setHealOn(setMoveKind(base(), 'mend_staff', 'warp'), 'mend_staff', false), 'mend_staff', 'foe')
    expect(item(foe, 'mend_staff').target).toBe('foe')
    expect(setHealOn(foe, 'mend_staff', true)).toBe(foe)
    expect(setStaffTarget(base(), 'mend_staff', 'foe')).toEqual(base())
    expect(blocking(foe)).toEqual([])
  })
  it('the last effect cannot be switched off', () => {
    const d = fx()
    expect(setMoveKind(d, 'warp_staff', 'none')).toBe(d)
    expect(setRepairOn(d, 'fix_staff', false)).toBe(d)
    expect(setHealOn(d, 'mend_staff', false)).toBe(d)
    expect(item(setHealOn(setMoveKind(d, 'mend_staff', 'pull'), 'mend_staff', false), 'mend_staff').effects).toEqual([{ pull: true }])
  })
  it('at most four effects', () => {
    const d = updateItem(fx(), 'mend_staff', { effects: [{ heal: 1 }, { mystery: 1 }, { mystery2: 1 }, { mystery3: 1 }] })
    expect(setMoveKind(d, 'mend_staff', 'warp')).toBe(d)
    expect(setRepairOn(d, 'mend_staff', true)).toBe(d)
  })
})

describe('a light staff', () => {
  it('switching light on removes the other effects, target, area, chance and conditions in one step; the reach stays', () => {
    const busy = updateItem(base(), 'mend_staff', { target: 'team', area: 'all', hit: 70, if: [{ hp: { op: '<', percent: 50 } }], xp: 5 })
    const d = setLightOn(busy, 'mend_staff', true)
    const s = item(d, 'mend_staff')
    expect(s.effects).toEqual([{ light: { radius: 2, turns: 1 } }])
    for (const k of ['target', 'area', 'hit', 'if']) expect(s).not.toHaveProperty(k)
    expect(s.range).toBe(3)
    expect(s.xp).toBe(5)
    expect(blocking(d)).toEqual([])
  })
  it('a staff that was used on its user gets a reach (a light picks a square)', () => {
    const d = setLightOn(setStaffTarget(base(), 'mend_staff', 'self'), 'mend_staff', true)
    expect(item(d, 'mend_staff').range).toBe(2)
    expect(blocking(d)).toEqual([])
  })
  it('radius and turns stay within 1 to 10; a file with no turns keeps none until it is edited', () => {
    let d = fx()
    d = setLightRadius(d, 'spark_staff', 99)
    expect(item(d, 'spark_staff').effects).toEqual([{ light: { radius: 10 } }])
    d = setLightTurns(d, 'spark_staff', 0)
    expect(item(d, 'spark_staff').effects).toEqual([{ light: { radius: 10, turns: 1 } }])
    expect(item(setLightTurns(d, 'spark_staff', 40), 'spark_staff').effects).toEqual([{ light: { radius: 10, turns: 10 } }])
  })
  it('switching it off leaves a small heal; every other part is then unavailable while it is on', () => {
    const lit = fx()
    expect(item(setLightOn(lit, 'lantern_staff', false), 'lantern_staff').effects).toEqual([{ heal: 10 }])
    expect(setHealOn(lit, 'lantern_staff', true)).toBe(lit)
    expect(setMoveKind(lit, 'lantern_staff', 'warp')).toBe(lit)
    expect(setRepairOn(lit, 'lantern_staff', true)).toBe(lit)
    expect(setStaffTarget(lit, 'lantern_staff', 'ally')).toBe(lit)
    expect(setAreaMode(lit, 'lantern_staff', 'all')).toBe(lit)
    expect(setHit(lit, 'lantern_staff', '50')).toBe(lit)
  })
  it('is refused while the staff has an effect the editor does not know (nothing is dropped)', () => {
    const odd = updateItem(base(), 'mend_staff', { effects: [{ heal: 4 }, { wizardry: 'door' }] })
    expect(unknownEffectCount(item(odd, 'mend_staff'))).toBe(1)
    expect(setLightOn(odd, 'mend_staff', true)).toBe(odd)
  })
})

describe('what the form does not understand is kept', () => {
  it('an unknown effect, a second heal and a second move stay when other parts are edited', () => {
    const odd = updateItem(base(), 'mend_staff', { effects: [{ wizardry: 'door' }, { heal: 4 }, { heal: 9 }, { pull: true }, { swap: true }], mystery: 3 })
    expect(keptEffectCount(item(odd, 'mend_staff'))).toBe(3)
    let d = setHealKind(odd, 'mend_staff', 'full')
    d = setMoveKind(d, 'mend_staff', 'warp')
    expect(item(d, 'mend_staff').effects).toEqual([{ wizardry: 'door' }, { heal: 'full' }, { heal: 9 }, { warp: { range: 3 } }, { swap: true }])
    expect(item(d, 'mend_staff').mystery).toBe(3)
  })
})

describe('repairable', () => {
  it('on is the key absent, off writes false', () => {
    const off = setRepairable(fx(), 'iron_blade', false)
    expect(item(off, 'iron_blade').repairable).toBe(false)
    expect(item(setRepairable(off, 'iron_blade', true), 'iron_blade')).not.toHaveProperty('repairable')
  })
  it('the switch shows for an item with uses once the game has a repair staff, or when the item has the flag', () => {
    const d = fx()
    expect(repairableShown(d, item(d, 'iron_blade'))).toBe(true)
    expect(repairableShown(d, item(d, 'short_sword'))).toBe(false) // unlimited uses
    expect(repairableShown(d, item(d, 'glass_blade'))).toBe(true)
    const noRepair = { ...d, items: d.items!.filter((i) => !['fix_staff', 'patch_staff'].includes(i.id)) }
    expect(repairableShown(noRepair, item(noRepair, 'iron_blade'))).toBe(false)
    expect(repairableShown(noRepair, item(noRepair, 'glass_blade'))).toBe(true)
  })
  it('goes away with the uses (the loader refuses it on an item that never wears)', () => {
    expect(item(setUnlimited(fx(), 'glass_blade', true), 'glass_blade')).not.toHaveProperty('repairable')
    expect(item(setItemKind(fx(), 'glass_blade', 'accessory'), 'glass_blade')).not.toHaveProperty('repairable')
  })
  it('checks: the flag on an item with no uses is refused', () => {
    expect(itemChecks(updateItem(fx(), 'short_sword', { repairable: false })).map((c) => c.text).join('\n')).toContain('unlimited uses')
    expect(itemChecks(updateItem(fx(), 'iron_blade', { repairable: 'no' as unknown as boolean })).map((c) => c.text).join('\n')).toContain('must be on or off')
  })
})

describe('the checks name every stage-two refusal', () => {
  const bad = (effects: unknown[], extra: Record<string, unknown> = {}) => blocking(updateItem(fx(), 'mend_staff', { effects: effects as any, ...extra })).join('\n')
  it('warp', () => {
    expect(bad([{ warp: 3 }])).toContain('needs how far from the user')
    expect(bad([{ warp: {} }])).toContain('needs how far from the user')
    expect(bad([{ warp: { range: 3, zzz: 1 } }])).toContain('a warp has no “zzz”')
    expect(bad([{ warp: { range: 21 } }])).toContain('from 1 to 20')
    expect(bad([{ warp: { range: [4, 2] } }])).toContain('“from” cannot be above “to”')
    expect(bad([{ warp: { range: ' ' } }])).toContain('formula is empty')
    expect(bad([{ warp: { range: true } }])).toContain('must be a number, two numbers')
    expect(bad([{ warp: { range: [1, 3] } }])).toBe('')
  })
  it('pull and swap', () => {
    expect(bad([{ pull: 1 }])).toContain('“pull” must be switched on')
    expect(bad([{ swap: false }])).toContain('“swap” must be switched on')
  })
  it('repair', () => {
    expect(bad([{ repair: 0 }])).toContain('from 1 to 999')
    expect(bad([{ repair: 1000 }])).toContain('from 1 to 999')
    expect(bad([{ repair: 'half' }])).toContain('all uses or a number of uses')
  })
  it('light', () => {
    const l = (light: unknown, extra: Record<string, unknown> = {}) => bad([{ light }], { target: undefined, ...extra })
    expect(l(5)).toContain('needs a radius')
    expect(l({})).toContain('radius must be a whole number from 1 to 10')
    expect(l({ radius: 11 })).toContain('from 1 to 10')
    expect(l({ radius: 2, turns: 0 })).toContain('turns must be a whole number from 1 to 10')
    expect(l({ radius: 2, turns: 11 })).toContain('turns must be a whole number from 1 to 10')
    expect(l({ radius: 2, zzz: 1 })).toContain('a light has no “zzz”')
    expect(l({ radius: 2 })).toBe('')
  })
  it('an unknown effect, and an effect that is not one thing', () => {
    expect(bad([{ wizardry: 'door' }])).toContain('“wizardry”, which a staff cannot do')
    expect(bad([{ heal: 1, warp: { range: 2 } }])).toContain('must be one thing')
    expect(bad([])).toContain('1 to 4 effects')
    expect(bad([{ heal: 1 }, { heal: 1 }, { heal: 1 }, { heal: 1 }, { heal: 1 }])).toContain('1 to 4 effects')
  })
  it('combinations', () => {
    expect(bad([{ warp: { range: 2 } }, { pull: true }])).toContain('only one of warp, pull and swap')
    expect(bad([{ pull: true }, { swap: true }])).toContain('only one of warp, pull and swap')
    expect(bad([{ repair: 1 }, { repair: 2 }])).toContain('only one repair')
    expect(bad([{ light: { radius: 1 } }, { light: { radius: 2 } }], { target: undefined })).toContain('only one light')
    expect(bad([{ repair: 1 }, { pull: true }])).toContain('repairing cannot be put together with warp, pull or swap')
    expect(bad([{ heal: 1 }, { pull: true }])).toBe('')
    expect(bad([{ heal: 1 }, { repair: 1 }])).toBe('')
  })
  it('a light with anything else, or with a target, area, chance or conditions', () => {
    expect(bad([{ light: { radius: 1 } }, { heal: 2 }], { target: undefined })).toContain('cannot be put together with another effect')
    expect(bad([{ light: { radius: 1 } }], { target: 'ally' })).toContain('cannot say who it is used on')
    expect(bad([{ light: { radius: 1 } }], { target: undefined, area: 'single' })).toContain('has no area')
    expect(bad([{ light: { radius: 1 } }], { target: undefined, hit: 50 })).toContain('chance of missing')
    expect(bad([{ light: { radius: 1 } }], { target: undefined, if: [] })).toContain('no unit for “Only if”')
  })
  it('warp, pull, swap and repair need one unit; pull and swap not "only the user"', () => {
    expect(bad([{ warp: { range: 2 } }], { area: 'all' })).toContain('a warp works on the one unit picked')
    expect(bad([{ pull: true }], { area: { radius: 1 } })).toContain('a pull works on the one unit picked')
    expect(bad([{ swap: true }], { area: 'all' })).toContain('a swap works on the one unit picked')
    expect(bad([{ repair: 1 }], { area: 'all' })).toContain('repairing works on the one unit picked')
    expect(bad([{ warp: { range: 2 } }], { area: 'single' })).toBe('')
    expect(bad([{ pull: true }], { target: 'self', range: undefined })).toContain('a pull needs another unit')
    expect(bad([{ swap: true }], { target: 'self', range: undefined })).toContain('a swap needs another unit')
    expect(bad([{ warp: { range: 2 } }], { target: 'self', range: undefined })).toBe('')
    expect(bad([{ repair: 'full' }], { target: 'self', range: undefined })).toBe('')
  })
})

describe('worth a look', () => {
  it('a light staff in a game where no map has fog', () => {
    expect(all(fx())).not.toContain('no map of this game has fog')
    const d = fx()
    const noFog = { ...d, maps: Object.fromEntries(Object.entries(d.maps).map(([k, m]) => [k, m ? { ...m, fog: undefined } : m])) } as ProjectDoc
    const text = staffChecks(noFog).filter((c) => !c.blocks).map((c) => c.text).join('\n')
    expect(text).toContain('Lantern Staff: it lights up squares in fog of war, but no map of this game has fog')
    expect(text).toContain('Spark Staff')
    expect(blocking(noFog)).toEqual([])
  })
  it('a repair staff when no item can be repaired', () => {
    expect(all(fx())).not.toContain('no item that can be repaired')
    const d = fx()
    const none = { ...d, items: d.items!.map((i) => (i.uses !== undefined ? { ...i, repairable: false } : i)) }
    const text = staffChecks(none).filter((c) => !c.blocks).map((c) => c.text).join('\n')
    expect(text).toContain('Fix Staff: it repairs items, but the game has no item that can be repaired')
    expect(blocking(none)).toEqual([])
  })
})
