// Fog of war in the editor's model: settings and their defaults, the checks that mirror the loader, the `sees` story
// condition (defaults, summary, checks, clean-up) and the "what a side sees" computation.
import { describe, expect, it } from 'vitest'
import { fogBlock, FOG_MAX_SIDES, mapHasFog, campaignMapHasFog, setFog, setHides, setVision, setVisionBonus, visionOf } from '../../src/model/fog'
import { fogChecks } from '../../src/model/fogChecks'
import { seenAtStart, teamOfSide } from '../../src/model/fogSight'
import { mapWarnings } from '../../src/model/mapChecks'
import { editMap } from '../../src/model/mapCtx'
import { newCondition } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { conditionKind, type Condition } from '../../src/model/storyTypes'
import { conditionText } from '../../src/model/storySummary'
import { removeSideAndStory } from '../../src/model/storyUnits'
import { deleteUnit } from '../../src/model/units'
import { deleteStructure } from '../../src/model/structures'
import { buildBody } from '../../src/store/autosave'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => loadDemo('demo-economy')
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps[mapId(d)]! })
const withEvents = (d: ProjectDoc, events: unknown[]) => editMap(d, mapId(d), (m) => ({ ...m, events: events as MapFile['events'] }))

describe('settings', () => {
  it('fog on writes true, off removes the key; no change is the same document', () => {
    const d = base()
    const m = mapId(d)
    expect(mapHasFog(d, m)).toBe(false)
    expect(setFog(d, m, false)).toBe(d)
    const on = setFog(d, m, true)
    expect(on.maps[m]!.fog).toBe(true)
    expect(setFog(on, m, true)).toBe(on)
    const off = setFog(on, m, false)
    expect('fog' in off.maps[m]!).toBe(false)
  })
  it('more than 32 sides blocks switching on, with the reason', () => {
    const d = base()
    const m = mapId(d)
    expect(fogBlock(d, m)).toBeNull()
    const sides = Array.from({ length: FOG_MAX_SIDES + 1 }, (_, i) => ({ id: `s${i}`, name: `S${i}` }))
    const many = editMap(d, m, (map) => ({ ...map, sides }))
    expect(fogBlock(many, m)).toMatch(/33 sides.*at most 32/)
    expect(fogBlock(editMap(d, m, (map) => ({ ...map, sides: sides.slice(0, 32) })), m)).toBeNull()
  })
  it('sight: the default is shown as 2 and never written; other numbers are written and kept in 0..20', () => {
    const d = base()
    const u = d.units![0].id
    expect(visionOf(d.units![0])).toBe(2)
    const three = setVision(d, u, 3)
    expect(three.units![0].vision).toBe(3)
    expect(visionOf(three.units![0])).toBe(3)
    expect('vision' in setVision(three, u, 2).units![0]).toBe(false)
    expect('vision' in setVision(three, u, undefined).units![0]).toBe(false)
    expect(setVision(d, u, 0).units![0].vision).toBe(0)
    expect(setVision(d, u, 99).units![0].vision).toBe(20)
    expect(setVision(d, u, -4).units![0].vision).toBe(0)
  })
  it('extra sight and hiding on terrain and structures: zero and off remove the key; 0..10', () => {
    const d = base()
    const t = d.terrain![0].id
    const s = d.structures![0].id
    expect(setVisionBonus(d, 'terrain', t, 3).terrain![0].visionBonus).toBe(3)
    expect(setVisionBonus(d, 'terrain', t, 50).terrain![0].visionBonus).toBe(10)
    expect('visionBonus' in setVisionBonus(setVisionBonus(d, 'terrain', t, 3), 'terrain', t, 0).terrain![0]).toBe(false)
    expect(setVisionBonus(d, 'structure', s, 2).structures![0].visionBonus).toBe(2)
    expect(setHides(d, 'terrain', t, true).terrain![0].hides).toBe(true)
    expect('hides' in setHides(setHides(d, 'structure', s, true), 'structure', s, false).structures![0]).toBe(false)
  })
  it('the settings are saved in the files: buildBody keeps every key', () => {
    let d = base()
    d = setFog(d, mapId(d), true)
    d = setVision(d, d.units![0].id, 4)
    d = setHides(setVisionBonus(d, 'terrain', d.terrain![0].id, 2), 'terrain', d.terrain![0].id, true)
    const body = JSON.stringify(buildBody(d, base(), 'r1', false))
    expect(body).toContain('"fog":true')
    expect(body).toContain('"vision":4')
    expect(body).toContain('"visionBonus":2')
    expect(body).toContain('"hides":true')
  })
  it('a campaign counts as fogged when any chapter map has fog', () => {
    const d = base()
    const m = mapId(d)
    const camp = { ...d, campaign: { chapters: [{ id: 'c1', map: m }] } } as ProjectDoc
    expect(campaignMapHasFog(camp)).toBe(false)
    expect(campaignMapHasFog(setFog(camp, m, true))).toBe(true)
  })
})

describe('checks mirror the loader', () => {
  it('a clean game has none', () => {
    expect(fogChecks(base())).toEqual([])
    expect(fogChecks(setVision(setFog(base(), mapId(base()), true), 'infantry', 5))).toEqual([])
  })
  it('names a sight outside 0..20, a bonus outside 0..10, a hides that is not on / off, fog that is not on / off', () => {
    const d = base()
    const m = mapId(d)
    const bad = {
      ...d,
      units: d.units!.map((u, i) => (i === 0 ? { ...u, vision: 21 } : i === 1 ? { ...u, vision: 1.5 } : u)),
      terrain: d.terrain!.map((t, i) => (i === 0 ? { ...t, visionBonus: 11 } : i === 1 ? { ...t, hides: 'yes' as never } : t)),
      structures: d.structures!.map((s, i) => (i === 0 ? { ...s, visionBonus: -1 } : s)),
    }
    const out = fogChecks(editMap(bad, m, (map) => ({ ...map, fog: 'yes' as never })))
    const text = out.map((c) => `${c.section}: ${c.text}`).join('\n')
    expect(text).toMatch(/units: .*sight must be a whole number from 0 to 20/)
    expect(out.filter((c) => c.section === 'units')).toHaveLength(2)
    expect(text).toMatch(/terrain: .*extra sight must be a whole number from 0 to 10/)
    expect(text).toMatch(/terrain: .*“hides” must be on or off/)
    expect(text).toMatch(/structures: .*extra sight/)
    expect(out.some((c) => c.section === 'maps' && c.mapId === m && /on or off/.test(c.text))).toBe(true)
  })
  it('more than 32 sides with fog shows on the map; without fog it does not', () => {
    const d = base()
    const m = mapId(d)
    const sides = Array.from({ length: 33 }, (_, i) => ({ id: `s${i}`, name: `S${i}` }))
    const many = editMap(d, m, (map) => ({ ...map, sides, units: [], structures: [] }))
    expect(fogChecks(many)).toEqual([])
    const fogged = setFog(many, m, true)
    expect(fogChecks(fogged).map((c) => c.text).join()).toMatch(/at most 32 sides; this map has 33/)
    expect(mapWarnings(fogged, fogged.maps[m]!).some((w) => /at most 32 sides/.test(w.text))).toBe(true)
  })
})

describe('story: the sees condition', () => {
  const sees = (c: Partial<Condition>) => ({ id: 'e', when: { battle: 'start' }, if: [c], do: [{ say: 'hi' }] })

  it('starts as the first team seeing the second side, and reads as a sentence', () => {
    const d = base()
    const cond = newCondition({ ...ctx(d), mapId: mapId(d) } as never, 'sees')
    expect(cond).toEqual({ sees: { side: 'red' }, team: 'blue' })
    expect(conditionKind(cond)).toBe('sees')
    expect(conditionText(ctx(d), { sees: { type: 'tank' }, team: 'blue' })).toBe('team Blue Army sees any Tank')
  })
  it('a good one has no problems on a map with fog; without fog a hint says it is always true', () => {
    const d = setFog(base(), mapId(base()), true)
    const m = mapId(d)
    const ok = withEvents(d, [sees({ sees: { type: 'tank' }, team: 'blue' })])
    expect(storyChecks(ok, m)).toEqual([])
    const noFog = withEvents(base(), [sees({ sees: { type: 'tank' }, team: 'blue' })])
    expect(storyChecks(noFog, m).map((c) => c.text).join()).toMatch(/no fog of war/)
  })
  it('names an unknown team, a missing team, an unknown unit type, in a nested If too', () => {
    const d = setFog(base(), mapId(base()), true)
    const m = mapId(d)
    const bad = withEvents(d, [
      sees({ sees: { type: 'ghost' }, team: 'nobody' }),
      { id: 'n', when: { battle: 'start' }, do: [{ if: [{ sees: {} }], then: [{ say: 'x' }] }] },
    ])
    const t = storyChecks(bad, m).map((c) => c.text).join('\n')
    expect(t).toContain('“nobody” is not a team or side')
    expect(t).toContain('“ghost” is not a unit type')
    expect(t).toContain('pick the team that must see it')
  })
  it('deleting the unit type, the side or the named unit it points at removes the event (checked through the shared walk)', () => {
    const d = setFog(base(), mapId(base()), true)
    const m = mapId(d)
    const keep = { id: 'keep', when: { battle: 'start' }, do: [{ say: 'stay' }] }
    const typed = withEvents(d, [sees({ sees: { type: 'tank' }, team: 'blue' }), keep])
    expect(deleteUnit(typed, 'tank').maps[m]!.events!.map((e) => e.id)).toEqual(['keep'])
    const three = editMap(d, m, (map) => ({ ...map, sides: [...map.sides!, { id: 'green', name: 'Green' }] }))
    const sided = withEvents(three, [sees({ sees: { type: 'tank' }, team: 'red' }), keep])
    expect(removeSideAndStory(sided, m, 'red').maps[m]!.events!.map((e) => e.id)).toEqual(['keep'])
    // Deleting a structure kind leaves a sees untouched (it names none).
    expect(deleteStructure(sided, 'city').maps[m]!.events!.map((e) => e.id)).toContain('e')
  })
})

describe('what a side sees at the start', () => {
  /** A 9 x 5 map of plain squares: blue has one soldier at (1, 2); red one at (7, 2). */
  function field(): ProjectDoc {
    const d = base()
    const m = mapId(d)
    const plain = d.terrain![0].id
    const row = 'p'.repeat(9)
    return editMap(
      { ...d, units: d.units!.map((u) => ({ ...u, vision: 2 })), structures: [] },
      m,
      (map) => ({
        ...map, grid: { ...map.grid, width: 9, height: 5 }, legend: { p: plain }, tiles: Array(5).fill(row), heights: undefined,
        sides: [{ id: 'blue', name: 'Blue' }, { id: 'red', name: 'Red' }],
        units: [{ type: d.units![0].id, side: 'blue', x: 1, y: 2 }, { type: d.units![0].id, side: 'red', x: 7, y: 2 }],
        structures: [], owners: undefined,
      }),
    )
  }
  const at = (seen: Uint8Array, x: number, y: number) => seen[y * 9 + x]
  const count = (seen: Uint8Array) => seen.reduce((n, v) => n + v, 0)

  it('counts squares the way the engine does: a diamond of the unit sight (not a square)', () => {
    const d = field()
    const seen = seenAtStart(d, d.maps[mapId(d)]!, 'blue')
    expect(at(seen, 1, 2)).toBe(1)
    expect(at(seen, 3, 2)).toBe(1) // two steps right
    expect(at(seen, 4, 2)).toBe(0)
    expect(at(seen, 2, 1)).toBe(1) // diagonal = 2 steps
    expect(at(seen, 3, 1)).toBe(0) // 3 steps
    expect(at(seen, 0, 0)).toBe(0)
    expect(count(seen)).toBe(12) // the diamond of radius 2 has 13 squares; the one at x = -1 is outside the map
  })
  it('sight, terrain bonus and structure bonus add up; a vision of 0 sees only its own square', () => {
    let d = field()
    const t = d.terrain![0].id
    const type = d.units![0].id
    d = setVision(d, type, 0)
    expect(count(seenAtStart(d, d.maps[mapId(d)]!, 'blue'))).toBe(1)
    d = setVisionBonus(d, 'terrain', t, 1)
    expect(count(seenAtStart(d, d.maps[mapId(d)]!, 'blue'))).toBe(5) // a bonus of 1 on every square: radius 1 around (1,2)
    d = setVisionBonus(setVision(d, type, 2), 'terrain', t, 2)
    expect(count(seenAtStart(d, d.maps[mapId(d)]!, 'blue'))).toBe(24) // radius 4 around (1,2) on a 9 x 5 map
  })
  it('owned structures see one square around every square of their footprint; other sides’ do not count', () => {
    let d = field()
    d = setVision(d, d.units![0].id, 0)
    const kind = base().structures![0]
    d = { ...d, structures: [{ ...kind, width: 2, height: 1 }] }
    d = editMap(d, mapId(d), (map) => ({ ...map, structures: [{ type: kind.id, x: 4, y: 0, side: 'blue' }] }))
    const seen = seenAtStart(d, d.maps[mapId(d)]!, 'blue')
    expect(at(seen, 3, 0)).toBe(1)
    expect(at(seen, 6, 0)).toBe(1)
    expect(at(seen, 4, 1)).toBe(1)
    expect(at(seen, 4, 2)).toBe(0)
    expect(count(seenAtStart(d, d.maps[mapId(d)]!, 'red'))).toBe(1) // red does not own it
  })
  it('a unit beside a square that hides can see it even with no sight; two sides of one team share sight', () => {
    let d = field()
    d = setVision(d, d.units![0].id, 0)
    d = setHides(d, 'terrain', d.terrain![0].id, true)
    const seen = seenAtStart(d, d.maps[mapId(d)]!, 'blue')
    expect(count(seen)).toBe(5) // its own square and its four neighbours (all hide)
    const allies = editMap(d, mapId(d), (map) => ({ ...map, sides: [{ id: 'blue', name: 'Blue', team: 'x' }, { id: 'red', name: 'Red', team: 'x' }] }))
    expect(teamOfSide(allies.maps[mapId(allies)]!, 'red')).toBe('x')
    expect(count(seenAtStart(allies, allies.maps[mapId(allies)]!, 'blue'))).toBe(10)
  })
})
