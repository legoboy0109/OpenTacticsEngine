// The third batch of battle scene editor parts (docs/BATTLE_SCENE.md; docs/EDITOR_UI.md "Battle animations"): staves in the scene, an effect's blend /
// order / speed / hide / fade, a staff item as an effect's weapon, and the stage's range bands. The checks mirror the loader.
import { describe, expect, it } from 'vitest'
import { battleChecks } from '../../src/model/battleChecks'
import { sceneChoiceProblems } from '../../src/model/battleSceneChecks'
import {
  DEFAULT_FADE, DEFAULT_HIDE, DEFAULT_SPEED, effectBlend, effectList, effectOrder, effectProblems, effectWeaponChoices, flightBySpeed, setEffectBlend, setEffectFor,
  setEffectOrder, setEffectTimed, setFlightBySpeed, staffItems, addEffect, updateEffect,
} from '../../src/model/battleEffects'
import { sceneStaves, setSceneStaves } from '../../src/model/battleTables'
import { enableStage } from '../../src/model/battleStage'
import {
  addBand, bandEach, bandProblems, bandsOf, removeBand, setBandFrom, setBandPan, setBandPlatform, setBandPoint, setBandView,
} from '../../src/model/stageBands'
import type { BattleEffectDef, ProjectDoc } from '../../src/model/types'

const doc = (effects?: BattleEffectDef[], stage = false): ProjectDoc => {
  const d = {
    game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
    terrain: [{ id: 'plain', name: 'Plain' }], structures: null, research: null, maps: {}, scripts: {},
    weapons: [{ id: 'sword', name: 'Sword', group: 'blade' }, { id: 'longbow', name: 'Longbow', group: 'bow' }],
    items: [{ id: 'mend_staff', name: 'Mend Staff', kind: 'staff' }, { id: 'tonic', name: 'Tonic', kind: 'usable' }],
    units: [{ id: 'knight', name: 'Knight' }],
    graphics: { sheets: [{ id: 'fx', file: 'art/fx.png', cellWidth: 16, cellHeight: 16 }], battle: effects ? { effects } : undefined },
    campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
  } as unknown as ProjectDoc
  return stage ? enableStage(d) : d
}
const fx = (more: Partial<BattleEffectDef> = {}): BattleEffectDef => ({ weapon: 'longbow', sheet: 'fx', frames: [[0, 0]], ...more })
const problems = (e: BattleEffectDef, stage = false) => effectProblems(doc([e], stage), 0)
const says = (list: string[], part: string) => list.some((t) => t.includes(part))

describe('staves in the scene (game.json)', () => {
  it('writes the key only when it is on', () => {
    const d = doc()
    expect(sceneStaves(d)).toBe(false)
    const on = setSceneStaves(d, true)
    expect(on.game!.battleSceneStaves).toBe(true)
    expect(sceneStaves(on)).toBe(true)
    const off = setSceneStaves(on, false)
    expect('battleSceneStaves' in off.game!).toBe(false)
    expect(setSceneStaves(d, false)).toBe(d)
  })
  it('says a wrong value, and a note when the game has no staves', () => {
    const bogus = { ...doc(), game: { ...doc().game!, battleSceneStaves: 'yes' as never } }
    expect(says(sceneChoiceProblems(bogus), 'must be on or off')).toBe(true)
    expect(sceneChoiceProblems(setSceneStaves(doc(), true))).toEqual([])
    const none = { ...setSceneStaves(doc(), true), items: [] } as unknown as ProjectDoc
    expect(says(sceneChoiceProblems(none), 'no staves')).toBe(true)
  })
})

describe('a staff item as an effect\'s weapon', () => {
  it('is offered next to the weapons, accepted by the checks, and refused when it is not a staff', () => {
    const d = doc()
    expect(staffItems(d)).toEqual([{ id: 'mend_staff', name: 'Mend Staff' }])
    expect(effectWeaponChoices(d).map((w) => w.id)).toEqual(['sword', 'longbow', 'mend_staff'])
    expect(problems(fx({ weapon: 'mend_staff' }))).toEqual([])
    expect(says(problems(fx({ weapon: 'tonic' })), 'does not exist')).toBe(true)
    expect(says(effectProblems(doc([fx({ weapon: 'mend_staff' }), fx({ weapon: 'mend_staff' })]), 1), 'already an effect for mend_staff')).toBe(true)
    // A new effect starts for the first weapon or staff without one; switching "plays for" to a weapon picks the first choice.
    let e = addEffect(doc([fx({ weapon: 'sword' }), fx({ weapon: 'longbow' })]))
    expect(effectList(e)[2].weapon).toBe('mend_staff')
    e = setEffectFor(updateEffect(e, 2, { weapon: undefined, group: 'bow' }), 2, 'weapon')
    expect(effectList(e)[2].weapon).toBe('sword')
  })
})

describe('an effect\'s blend, order and speed', () => {
  it('blend: normal is no key', () => {
    let d = doc([fx()])
    expect(effectBlend(effectList(d)[0])).toBe('normal')
    d = setEffectBlend(d, 0, 'add')
    expect(effectList(d)[0].blend).toBe('add')
    expect(setEffectBlend(d, 0, 'normal')).toEqual(doc([fx()]))
    expect(says(problems(fx({ blend: 'multiply' as never })), 'normal or add')).toBe(true)
  })
  it('order: only for a screen effect; the default is no key', () => {
    let d = doc([fx({ layer: 'screen' })])
    expect(effectOrder(effectList(d)[0])).toBe('backdrop')
    d = setEffectOrder(d, 0, 'front')
    expect(effectList(d)[0].order).toBe('front')
    expect('order' in effectList(setEffectOrder(d, 0, 'backdrop'))[0]).toBe(false)
    expect(problems(fx({ layer: 'screen', order: 'behind' }))).toEqual([])
    expect(says(problems(fx({ order: 'front' })), 'only for an effect that covers the stage')).toBe(true)
    expect(says(problems(fx({ layer: 'screen', order: 'top' as never })), 'order must be')).toBe(true)
  })
  it('speed: a flight by time or by speed, never both; needs a fixed stage and a flying effect', () => {
    let d = doc([fx({ mode: 'travel', travelMs: 300 })], true)
    expect(flightBySpeed(effectList(d)[0])).toBe(false)
    d = setFlightBySpeed(d, 0, true)
    expect(effectList(d)[0]).toMatchObject({ speed: DEFAULT_SPEED })
    expect('travelMs' in effectList(d)[0]).toBe(false)
    expect(effectProblems(d, 0)).toEqual([])
    expect('speed' in effectList(setFlightBySpeed(d, 0, false))[0]).toBe(false)
    expect(says(problems(fx({ mode: 'travel', speed: 200 })), 'needs a fixed stage')).toBe(true)
    expect(says(problems(fx({ mode: 'travel', speed: 200, travelMs: 300 }), true), 'not both')).toBe(true)
    expect(says(problems(fx({ speed: 200 }), true), 'only for a flying effect')).toBe(true)
    expect(says(problems(fx({ mode: 'travel', speed: 0 }), true), 'between 1 and 10000')).toBe(true)
    expect(says(problems(fx({ mode: 'travel', speed: 2.5 }), true), 'between 1 and 10000')).toBe(true)
  })
})

describe('an effect\'s hide and fade windows', () => {
  it('adds and removes rows; an emptied list is no key', () => {
    let d = doc([fx()])
    d = setEffectTimed(d, 0, 'hide', [DEFAULT_HIDE])
    d = setEffectTimed(d, 0, 'fade', [DEFAULT_FADE])
    expect(effectList(d)[0]).toMatchObject({ hide: [{ atMs: 0, ms: 1000, what: ['backdrop'] }], fade: [{ atMs: 0, ms: 300, to: '#000000', alpha: 0.8, holdMs: 300, over: 'all' }] })
    expect(effectProblems(d, 0)).toEqual([])
    d = setEffectTimed(setEffectTimed(d, 0, 'hide', []), 0, 'fade', [])
    expect('hide' in effectList(d)[0] || 'fade' in effectList(d)[0]).toBe(false)
  })
  it('checks mirror the loader', () => {
    const hide = (h: Record<string, unknown>) => problems(fx({ hide: [{ atMs: 0, ms: 100, what: ['backdrop'], ...h }] as never }))
    expect(hide({})).toEqual([])
    expect(says(hide({ atMs: -1 }), 'start must be between 0 and 60000')).toBe(true)
    expect(says(hide({ ms: 5 }), 'length must be between 10 and 60000')).toBe(true)
    expect(says(hide({ what: [] }), 'choose the backdrop and / or the platforms')).toBe(true)
    expect(says(hide({ what: ['fighters'] }), 'choose the backdrop')).toBe(true)
    expect(says(hide({ what: ['backdrop', 'backdrop'] }), 'choose the backdrop')).toBe(true)
    expect(says(hide({ color: 'red' }), 'colour must look like')).toBe(true)
    const fade = (f: Record<string, unknown>) => problems(fx({ fade: [{ atMs: 0, ms: 100, ...f }] as never }))
    expect(fade({})).toEqual([])
    expect(says(fade({ ms: 9 }), 'between 10 and 5000')).toBe(true)
    expect(says(fade({ alpha: 0 }), 'above 0 and at most 1')).toBe(true)
    expect(says(fade({ alpha: 1.2 }), 'above 0 and at most 1')).toBe(true)
    expect(says(fade({ holdMs: -3 }), 'time held')).toBe(true)
    expect(says(fade({ to: 'black' }), 'colour must look like')).toBe(true)
    expect(says(fade({ over: 'fighters' }), 'everything or the backdrop only')).toBe(true)
    expect(says(problems(fx({ hide: Array.from({ length: 33 }, () => DEFAULT_HIDE) })), 'at most 32')).toBe(true)
  })
})

describe('the stage\'s range bands', () => {
  const stage = () => doc(undefined, true)  // 240 x 160, feet [89,104] / [151,104]
  it('adds a band from the first free distance as a copy of the layout at range, sorted by distance', () => {
    let d = stage()
    expect(bandsOf(d)).toEqual([])
    d = addBand(d)
    expect(bandsOf(d)).toEqual([{ from: 4, feet: { left: [89, 104], right: [151, 104] } }])
    d = addBand(d)
    expect(bandsOf(d).map((b) => b.from)).toEqual([4, 5])
    d = setBandFrom(d, 1, 3)
    expect(bandProblems(d)).toEqual([])
    expect(bandsOf(removeBand(removeBand(d, 0), 0))).toEqual([])
    expect('ranges' in (removeBand(removeBand(d, 0), 0).graphics!.battle!.stage ?? {})).toBe(false)  // an empty list is no key
    expect(bandsOf(addBand(doc()))).toEqual([])  // no stage, no band
  })
  it('edits points and the platform places', () => {
    let d = addBand(stage())
    d = setBandPoint(d, 0, 'feet', 'right', 0, 200)
    expect(bandsOf(d)[0].feet.right).toEqual([200, 104])
    d = setBandPlatform(d, 0, true)
    expect(bandsOf(d)[0].platform).toEqual({ left: [57, 96], right: [168, 96] })
    d = setBandPoint(d, 0, 'platform', 'left', 1, 90)
    expect(bandsOf(d)[0].platform!.left).toEqual([57, 90])
    expect('platform' in bandsOf(setBandPlatform(d, 0, false))[0]).toBe(false)
    expect(setBandPoint(addBand(stage()), 0, 'platform', 'left', 0, 5)).toEqual(addBand(stage()))  // no platform places yet
  })
  it('one fighter at a time needs a pan and a world; back to both undoes it', () => {
    let d = addBand(stage())
    d = setBandView(d, 0, 'each')
    const b = bandsOf(d)[0]
    expect(bandEach(b)).toBe(true)
    expect(b.pan).toEqual({ panMs: 250, leftView: 0, rightView: 240 })
    expect(b.feet).toEqual({ left: [89, 104], right: [391, 104] })  // the right fighter is one stage width over, in the right view
    expect(bandProblems(d)).toEqual([])
    d = setBandPan(d, 0, 'panMs', 400)
    expect(bandsOf(d)[0].pan!.panMs).toBe(400)
    d = setBandPan(d, 0, 'rightView', 300)
    expect(bandProblems(d)).toEqual([])  // the foot point 391 is still inside the view 300 to 539
    d = setBandPan(d, 0, 'rightView', 400)
    expect(bandProblems(d).join(' ')).toContain('right fighter')  // ... but not inside 400 to 639
  })
  it('the pan checks mirror the loader', () => {
    let d = setBandView(addBand(stage()), 0, 'each')
    expect(bandProblems(d)).toEqual([])
    const withPan = (p: Record<string, number>) => bandProblems(setBandPan(setBandPan(setBandPan(d, 0, 'panMs', p.panMs ?? 250), 0, 'leftView', p.leftView ?? 0), 0, 'rightView', p.rightView ?? 240))
    expect(withPan({})).toEqual([])
    expect(says(withPan({ panMs: 6000 }), 'between 0 and 5000')).toBe(true)
    expect(says(withPan({ rightView: 9000 }), 'right view must be a whole number between 0 and 8191')).toBe(true)
    expect(says(withPan({ rightView: 100 }), 'right fighter\'s foot point (391) is not inside the picture')).toBe(true)
    expect(says(withPan({ leftView: 120 }), 'left fighter\'s foot point (89) is not inside the picture')).toBe(true)
    d = setBandView(d, 0, 'both')
    expect(bandProblems(d)).toEqual([])
    expect(bandsOf(d)[0].pan).toBeUndefined()
    expect(bandsOf(d)[0].feet.right).toEqual([151, 104])  // moved back
  })
  it('checks the layout of the list like the loader', () => {
    const base = (ranges: unknown) => {
      const d = stage()
      return { ...d, graphics: { ...d.graphics!, battle: { stage: { ...d.graphics!.battle!.stage!, ranges } } } } as ProjectDoc
    }
    const band = (more: Record<string, unknown>) => ({ from: 4, feet: { left: [10, 10], right: [200, 10] }, ...more })
    expect(bandProblems(base([band({})]))).toEqual([])
    expect(says(bandProblems(base([])), '1 to 16')).toBe(true)
    expect(says(bandProblems(base([band({ from: 1 })])), '“from” must be')).toBe(true)
    expect(says(bandProblems(base([band({ from: 100 })])), '“from” must be')).toBe(true)
    expect(says(bandProblems(base([band({}), band({})])), 'also starts at 4')).toBe(true)
    expect(says(bandProblems(base([band({ feet: { left: [10, 10], right: [240, 10] } })])), 'right one of the foot points (240, 10) is outside the stage (240 × 160)')).toBe(true)
    expect(says(bandProblems(base([band({ platform: { left: [0, 200], right: [0, 0] } })])), 'platform places (0, 200) is outside')).toBe(true)
    expect(says(bandProblems(base([band({ view: 'one' })])), 'both fighters or one at a time')).toBe(true)
    expect(says(bandProblems(base([band({ view: 'each' })])), 'needs the pan settings')).toBe(true)
    expect(says(bandProblems(base([band({ pan: { leftView: 0, rightView: 0 } })])), 'a pan belongs to')).toBe(true)
    expect(says(bandProblems(base([band({ feet: { left: [10, 10], right: [1.5, 10] } })])), 'not a pair of whole numbers')).toBe(true)
  })
  it('the art checks list a band problem as one that blocks the game', () => {
    const d = setBandPoint(addBand(stage()), 0, 'feet', 'left', 0, 500)
    const checks = battleChecks(d)
    expect(checks.some((c) => c.blocks && c.text.includes('Range layout 1') && c.text.includes('outside the stage'))).toBe(true)
  })
})
