// The six rule-gap keys of the loader: area shapes "ring" and "field", an ability range from 0 without an area, `arc` in an inflict chance,
// a map's "abilityPoints", a strike's "reach": "equipped", and guests (a unit's "guest" and the story command "guestJoins").
// Each editor check must say what the loader refuses; each model helper must write no defaults and keep keys it does not know.
import { describe, expect, it } from 'vitest'
import { setArea, setEffectKind, setRange, setTarget } from '../../src/model/abilities'
import { AREA_SHAPES, areaOf, setAreaShape, setAreaSize, setAreaVertical } from '../../src/model/abilityArea'
import { areaChecks } from '../../src/model/abilityAreaChecks'
import { abilityChecks } from '../../src/model/abilityChecks'
import { setStrikeReach, strikeReachChecks, strikeReachEquipped } from '../../src/model/abilityStrikeReach'
import { abilitySummary } from '../../src/model/abilityText'
import { behaviourChecks } from '../../src/model/behaviour'
import { mapDifficultyChecks } from '../../src/model/difficultyChecks'
import { computerMoves, guestChecks, guestJoinsChecks, isGuest, setGuest } from '../../src/model/guests'
import { mapApChecks, mapApPerBattle, setMapApPerBattle } from '../../src/model/mapAbilityPoints'
import { mapWarnings } from '../../src/model/mapChecks'
import { storyChecks } from '../../src/model/storyChecks'
import { commandKind, COMMAND_KINDS } from '../../src/model/storyTypes'
import { newCommand } from '../../src/model/storyDefaults'
import { commandText } from '../../src/model/storySummary'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import type { Ability } from '../../src/model/srpgTypes'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const fx = (): ProjectDoc => switchOnFeature(loadDemo('demo-srpg'), 'abilities')
const heal = (extra: Partial<Ability> = {}): Ability => ({ id: 'mend', name: 'Mend', target: 'ally', effect: { heal: 10 }, ...extra })
const withAbility = (d: ProjectDoc, a: Ability): ProjectDoc => ({ ...d, abilities: [a] })
/** The map under test: the document keeps maps as MapFile | null. */
const mapOf = (d: ProjectDoc, id: string): MapFile => d.maps[id] as MapFile
const probs = (d: ProjectDoc, a: Ability) => abilityChecks(withAbility(d, a)).map((c) => c.text).filter((t) => !t.includes('nobody has'))

describe('area shapes ring and field', () => {
  it('lists the six shapes', () => {
    expect([...AREA_SHAPES].sort()).toEqual(['burst', 'cross', 'field', 'line', 'radius', 'ring'])
  })
  it('a ring has a radius that carries over from a radius or cross; a field has none', () => {
    const a = setAreaSize(setAreaShape(setArea(heal(), true), 'radius'), 4)
    const ring = setAreaShape(a, 'ring')
    expect(ring.area).toEqual({ shape: 'ring', radius: 4 })
    expect(areaOf(ring)).toEqual({ shape: 'ring', size: 4, vertical: undefined })
    expect(setAreaSize(ring, 99).area).toEqual({ shape: 'ring', radius: 8 })
    expect(setAreaShape(setAreaShape(setArea(heal(), true), 'line'), 'ring').area).toEqual({ shape: 'ring', radius: 2 })
    const field = setAreaShape(ring, 'field')
    expect(field.area).toEqual({ shape: 'field' })
    expect(setAreaSize(field, 3)).toBe(field)
  })
  it('changing to a field drops the vertical reach, and a field takes none', () => {
    const tall = setAreaVertical(setAreaShape(setArea(heal(), true), 'radius'), 2)
    expect(setAreaShape(tall, 'field').area).toEqual({ shape: 'field' })
    const field = setAreaShape(setArea(heal(), true), 'field')
    expect(setAreaVertical(field, 1)).toBe(field)
  })
  it('says what the loader refuses about a field and a ring', () => {
    const d = fx()
    const t = (area: Record<string, unknown>) => areaChecks(d, heal({ area })).join(' | ')
    expect(t({ shape: 'field' })).toBe('')
    expect(t({ shape: 'field', radius: 2 })).toMatch(/“field” covers the whole map and takes no radius/)
    expect(t({ shape: 'field', vertical: 1 })).toMatch(/“field” covers the whole map at every height/)
    expect(t({ shape: 'ring' })).toMatch(/“ring” needs a radius from 1 to 8/)
    expect(t({ shape: 'ring', radius: 9 })).toMatch(/radius of the area must be a whole number from 1 to 8/)
    expect(t({ shape: 'ring', radius: 3 })).toBe('')
    expect(t({ shape: 'burst', radius: 1 })).toMatch(/“radius”, “cross” and “ring”/)
    expect(t({ shape: 'star' })).toMatch(/burst, line, radius, cross, ring or field/)
  })
  it('words the effect', () => {
    const d = fx()
    expect(abilitySummary(d, heal({ area: { shape: 'ring', radius: 2 } }))).toMatch(/a ring of 2 squares around the square it aims at/)
    expect(abilitySummary(d, heal({ area: { shape: 'field' } }))).toMatch(/every ally \(itself too\) in the whole map/)
  })
})

describe('a range that starts at 0 without an area', () => {
  const d = fx()
  it('is allowed on a heal, a cure and a condition on an ally or a unit', () => {
    expect(probs(d, heal({ range: [0, 2] }))).toEqual([])
    expect(probs(d, heal({ target: 'unit', range: [0, 3] }))).toEqual([])
    const cure: Ability = { id: 'c', name: 'Clear', target: 'ally', effect: { cure: 'all' }, range: [0, 1] }
    expect(probs(d, cure)).toEqual([])
    const cond = d.conditions?.[0]?.id ?? 'x'
    const give: Ability = { id: 'g', name: 'Give', target: 'unit', inflict: { condition: cond }, range: [0, 2] }
    expect(probs(d, give).filter((t) => /reach/.test(t))).toEqual([])
  })
  it('is refused on an enemy target, and the message says where it is allowed', () => {
    const cond = d.conditions?.[0]?.id ?? 'x'
    const foe: Ability = { id: 'g', name: 'Hex', target: 'enemy', inflict: { condition: cond }, range: [0, 2] }
    const msg = probs(d, foe).join(' | ')
    expect(msg).toMatch(/two whole numbers from 1 to 20/)
    expect(msg).toMatch(/may start at 0 only on a healing, curing or condition ability aimed at an ally or any unit, or with an area/)
  })
  it('is refused on a push, which has a fixed range, only by the existing check, and a strike has none', () => {
    expect(areaChecks(d, heal({ range: [0, 21] })).join('')).toMatch(/from 0 to 20/)
    expect(areaChecks(d, heal({ range: [3, 2] })).join('')).toMatch(/the first not above the second/)
    expect(areaChecks(d, heal({ range: [0, 1], target: 'enemy' } as Partial<Ability>)).join('')).toMatch(/from 1 to 20/)
  })
  it('setRange takes 0 only where the loader does, and a new target pulls it back to 1', () => {
    expect(setRange(heal(), 0, 2).range).toEqual([0, 2])
    expect(setRange({ id: 'p', name: 'P', target: 'unit', effect: { push: 1 } }, 0, 2).range).toBeUndefined()
    const cond: Ability = { id: 'g', name: 'G', target: 'unit', inflict: { condition: 'a' } }
    expect(setRange(cond, 0, 1).range).toEqual([0, 1])
    expect(setRange(setTarget(cond, 'enemy'), 0, 1).range).toBeUndefined()  // 1 to 1 is the default, not written
    expect(setTarget({ ...cond, range: [0, 2] }, 'enemy').range).toEqual([1, 2])
    expect(setTarget(heal({ range: [0, 2] }), 'unit').range).toEqual([0, 2])
  })
  it('turning an area off keeps a 0 only where it is still allowed', () => {
    const a = setArea(setRange(setArea(heal(), true), 0, 1), false)
    expect(a.range).toEqual([0, 1])
    const cond: Ability = { id: 'g', name: 'G', target: 'enemy', inflict: { condition: 'a' } }
    expect(setArea(setRange(setArea(cond, true), 0, 1), false).range).toBeUndefined()  // [1, 1] after the 0 is refused
  })
  it('changing the effect to one that cannot start at 0 pulls the range back', () => {
    const r = setEffectKind(d, heal({ range: [0, 2] }), 'summon')
    expect(r.range).toEqual([1, 2])
  })
})

describe('arc in an inflict chance', () => {
  it('the help text names arc, and a chance formula that reads it is written as given', () => {
    // The editor keeps a chance as text: nothing rejects an unknown word, so arc passes untouched.
    const a: Ability = { id: 'g', name: 'G', target: 'enemy', inflict: { condition: 'a', chance: 'a.mag + arc * 20' } }
    expect(probs(fx(), a).filter((t) => /chance|arc/i.test(t))).toEqual([])
  })
})

describe('a map\'s own ability points', () => {
  const doc = (teaches: boolean): ProjectDoc => {
    const d = fx()
    const id = Object.keys(d.maps)[0]
    const items = (d.items ?? []).map((i, k) => (k === 0 && teaches ? { ...i, teaches: [{ ability: 'x', points: 5 }] } : { ...i, teaches: undefined }))
    return { ...d, items: items as ProjectDoc['items'], game: { ...d.game!, rules: { ...(d.game!.rules ?? {}), abilityPoints: { perBattle: 10 } } }, maps: { ...d.maps, [id]: mapOf(d, id) } }
  }
  it('sets, replaces and removes the number; the block goes with the last key', () => {
    const d = doc(true)
    const id = Object.keys(d.maps)[0]
    const a = setMapApPerBattle(d, id, 25)
    expect(mapOf(a, id).abilityPoints).toEqual({ perBattle: 25 })
    expect(mapApPerBattle(mapOf(a, id))).toBe(25)
    expect(mapOf(setMapApPerBattle(a, id, 12345.6), id).abilityPoints).toEqual({ perBattle: 9999 })
    expect(mapOf(setMapApPerBattle(a, id, -4), id).abilityPoints).toEqual({ perBattle: 0 })
    expect('abilityPoints' in mapOf(setMapApPerBattle(a, id, undefined), id)).toBe(false)
    expect(setMapApPerBattle(d, 'nope', 5)).toBe(d)
    expect(mapApPerBattle(mapOf(d, id))).toBeUndefined()
  })
  it('says what the loader refuses', () => {
    const d = doc(true)
    const m = (ap: unknown): MapFile => ({ ...mapOf(d, Object.keys(d.maps)[0]), abilityPoints: ap as MapFile['abilityPoints'] })
    expect(mapApChecks(d, m({ perBattle: 5 }))).toEqual([])
    expect(mapApChecks(d, { ...m(undefined), abilityPoints: undefined })).toEqual([])
    expect(mapApChecks(d, m({ perBattle: 5, name: 'x' })).join(' ')).toMatch(/unknown setting “name”. Only “perBattle”/)
    expect(mapApChecks(d, m({})).join(' ')).toMatch(/need “perBattle”/)
    expect(mapApChecks(d, m({ perBattle: 10000 })).join(' ')).toMatch(/whole number from 0 to 9999/)
    expect(mapApChecks(d, m({ perBattle: 1.5 })).join(' ')).toMatch(/whole number from 0 to 9999/)
    expect(mapApChecks(d, m([]))).toHaveLength(1)
    expect(mapApChecks(doc(false), m({ perBattle: 5 })).join(' ')).toMatch(/no item teaches anything/)
  })
  it('shows up in the map warnings', () => {
    const d = doc(false)
    const id = Object.keys(d.maps)[0]
    const withAp = setMapApPerBattle(d, id, 5)
    expect(mapWarnings(withAp, mapOf(withAp, id)).some((w) => /no item teaches anything/.test(w.text))).toBe(true)
  })
})

describe('a strike that reaches as the weapon in hand', () => {
  const strike = (extra: Record<string, unknown> = {}): Ability => ({ id: 's', name: 'Slash', effect: { strike: 'sword', ...extra } })
  it('switches on and off, keeping the weapon', () => {
    const on = setStrikeReach(strike(), true)
    expect(on.effect).toEqual({ strike: 'sword', reach: 'equipped' })
    expect(strikeReachEquipped(on)).toBe(true)
    const off = setStrikeReach(on, false)
    expect(off.effect).toEqual({ strike: 'sword' })
    expect('reach' in off.effect!).toBe(false)
    expect(setStrikeReach(heal(), true)).toEqual(heal())
  })
  it('says what the loader refuses', () => {
    expect(strikeReachChecks(strike())).toEqual([])
    expect(strikeReachChecks(strike({ reach: 'equipped' }))).toEqual([])
    expect(strikeReachChecks(strike({ reach: 'far' })).join('')).toMatch(/can only be “equipped”/)
    expect(strikeReachChecks({ id: 'h', name: 'Mend', effect: { heal: 5, reach: 'equipped' } }).join('')).toMatch(/goes only with a strike/)
  })
  it('is part of the ability checks and the summary', () => {
    const d = fx()
    const a = strike({ reach: 'far' })
    expect(abilityChecks(withAbility(d, a)).some((c) => /can only be “equipped”/.test(c.text))).toBe(true)
    expect(abilitySummary(d, strike({ reach: 'equipped' }))).toMatch(/from as far as the weapon in hand reaches/)
    expect(abilitySummary(d, strike())).toMatch(/aiming and reaching as the weapon does/)
  })
})

describe('guests', () => {
  const doc = (): { d: ProjectDoc; id: string; aiSide: string; humanSide: string } => {
    const d = fx()
    const id = Object.keys(d.maps)[0]
    const sides = mapOf(d, id).sides ?? []
    return { d, id, aiSide: sides.find((s) => s.controller === 'ai')!.id, humanSide: sides.find((s) => s.controller !== 'ai')!.id }
  }
  const withUnit = (d: ProjectDoc, id: string, u: Record<string, unknown>): ProjectDoc =>
    ({ ...d, maps: { ...d.maps, [id]: { ...mapOf(d, id), units: [{ type: d.units![0].id, x: 0, y: 0, ...u }] as MapFile['units'] } } })

  it('switches the key on and off', () => {
    const u = { side: 'a', x: 0, y: 0 }
    expect(setGuest(u, true)).toEqual({ ...u, guest: true })
    expect('guest' in setGuest(setGuest(u, true), false)).toBe(false)
    expect(isGuest({ guest: true })).toBe(true)
    expect(isGuest({})).toBe(false)
  })
  it('the computer moves a unit of a computer side and a guest', () => {
    const { d, id, aiSide, humanSide } = doc()
    const m = mapOf(d, id)
    expect(computerMoves(m, { side: aiSide })).toBe(true)
    expect(computerMoves(m, { side: humanSide })).toBe(false)
    expect(computerMoves(m, { side: humanSide, guest: true })).toBe(true)
  })
  it('refuses a guest on a computer side and in a game with action points', () => {
    const { d, id, aiSide, humanSide } = doc()
    const ok = withUnit(d, id, { side: humanSide, guest: true })
    expect(guestChecks(ok, mapOf(ok, id))).toEqual([])
    const bad = withUnit(d, id, { side: aiSide, guest: true })
    expect(guestChecks(bad, mapOf(bad, id))[0].text).toMatch(/is the computer’s, so its units are the computer’s already/)
    expect(guestChecks(bad, mapOf(bad, id))[0].cell).toEqual({ x: 0, y: 0 })
    const ap: ProjectDoc = { ...ok, game: { ...ok.game!, rules: { ...(ok.game!.rules ?? {}), actionPoints: { perTurn: 3 } } } as ProjectDoc['game'] }
    expect(guestChecks(ap, mapOf(ap, id)).map((g) => g.text).join(' ')).toMatch(/not available in a game with action points/)
    const notBool = withUnit(d, id, { side: humanSide, guest: 'yes' })
    expect(guestChecks(notBool, mapOf(notBool, id))[0].text).toMatch(/must be on or off/)
  })
  it('checks the unit an addUnit command brings in', () => {
    const { d, id, aiSide } = doc()
    const m = { ...mapOf(d, id), events: [{ id: 'e1', when: { turn: 1 }, do: [{ addUnit: { type: d.units![0].id, side: aiSide, x: 1, y: 1, guest: true } }] }] } as MapFile
    expect(guestChecks(d, m)[0].text).toMatch(/Event “e1”, command 1, the unit it adds: a guest is a unit on a side a person plays/)
  })
  it('lets a guest on a human side have a behaviour and a difficulty, and still refuses them on an ordinary one', () => {
    const { d, id, humanSide } = doc()
    const dif: ProjectDoc = { ...d, game: { ...d.game!, difficulties: { list: [{ id: 'easy', name: 'Easy' }, { id: 'hard', name: 'Hard' }] } } as ProjectDoc['game'] }
    const mk = (guest: boolean) => withUnit(dif, id, { side: humanSide, behaviour: 'hold', difficulty: ['hard'], ...(guest ? { guest: true } : {}) })
    const plain = mk(false)
    expect(behaviourChecks(plain, id, mapOf(plain, id)).map((c) => c.text).join(' ')).toMatch(/this unit is on a side a person plays/)
    expect(mapDifficultyChecks(plain, id, mapOf(plain, id)).map((c) => c.text).join(' ')).toMatch(/is on a human side/)
    const guest = mk(true)
    expect(behaviourChecks(guest, id, mapOf(guest, id)).map((c) => c.text).join(' ')).not.toMatch(/side a person plays/)
    expect(mapDifficultyChecks(guest, id, mapOf(guest, id)).map((c) => c.text).join(' ')).not.toMatch(/human side/)
  })
})

describe('the guestJoins story command', () => {
  it('is a command kind of its own, with a starting filter and a sentence', () => {
    expect(COMMAND_KINDS).toContain('guestJoins')
    expect(COMMAND_KINDS.indexOf('guestJoins')).toBeLessThan(COMMAND_KINDS.indexOf('background'))
    expect(commandKind({ guestJoins: { unit: 'ann' } })).toBe('guestJoins')
    const d = fx()
    const id = Object.keys(d.maps)[0]
    const c = newCommand({ doc: d, map: mapOf(d, id) } as never, 'guestJoins')
    expect(c.guestJoins).toBeTypeOf('object')
    const ctx = { doc: d, map: mapOf(d, id) } as never
    expect(commandText(ctx, { guestJoins: { side: mapOf(d, id).sides![0].id } })).toMatch(/^Makes .* an ordinary unit of the player/)
  })
  it('takes no other key', () => {
    expect(guestJoinsChecks({ guestJoins: { unit: 'ann' } })).toEqual([])
    expect(guestJoinsChecks({ guestJoins: { unit: 'ann' }, hide: true })[0]).toMatch(/takes only the unit filter; remove “hide”/)
    expect(guestJoinsChecks({ guestJoins: null as never })[0]).toMatch(/pick which guests/)
    expect(guestJoinsChecks({ hide: true })).toEqual([])
  })
  it('is checked inside the map story checks', () => {
    const d = fx()
    const id = Object.keys(d.maps)[0]
    const m = { ...mapOf(d, id), events: [{ id: 'e1', when: { turn: 1 }, do: [{ guestJoins: { side: mapOf(d, id).sides![0].id }, hide: true }] }] } as MapFile
    const next = { ...d, maps: { ...d.maps, [id]: m } }
    expect(storyChecks(next, id).map((c) => c.text).join(' ')).toMatch(/takes only the unit filter/)
  })
})
