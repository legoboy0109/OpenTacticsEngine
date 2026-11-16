// E3: promotion in the editor's model (docs/SRPG.md section 2 and "Editor"): rows on a unit type, defaults never written, the
// promotion item switch and what follows it, deleting a unit type / item / stat, rules.promotion, the checks, and the story
// command "Promote a unit" and trigger "A unit is promoted".
import { describe, expect, it } from 'vitest'
import { addItem, deleteItem, setItemKind } from '../../src/model/items'
import { itemChecks } from '../../src/model/itemChecks'
import { cleanStat, cleanUnit, findUsages } from '../../src/model/references'
import {
  addPromotion, baseDifference, gameLevelAfter, movePromotion, patchPromotion, promotionItems, promotionPatches, promotionsOf, removePromotion,
  setGain, setItemPromotes, setLevelAfterRule,
} from '../../src/model/promotion'
import { itemIsHandedOut, promotionChecks } from '../../src/model/promotionChecks'
import { itemUsageLines } from '../../src/model/itemRefs'
import { addAbility, setEffectKind, setSummon } from '../../src/model/abilities'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { newCommand, newTrigger } from '../../src/model/storyDefaults'
import { patchEvent } from '../../src/model/storyEdit'
import { storyChecks } from '../../src/model/storyChecks'
import { commandKind, triggerKind, type StoryEvent } from '../../src/model/storyTypes'
import { commandText, triggerText } from '../../src/model/storySummary'
import { storyUsages } from '../../src/model/storyRefs'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => loadDemo('demo-srpg')
const [A, B, C] = (() => { const d = base(); return d.units!.slice(0, 3).map((u) => u.id) })()
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const withMap = (d: ProjectDoc, fn: (m: MapFile) => MapFile): ProjectDoc => ({ ...d, maps: { ...d.maps, [mapId(d)]: fn(d.maps[mapId(d)]!) } })
const unit = (d: ProjectDoc, id: string) => d.units!.find((u) => u.id === id)!

describe('promotion rows of a unit type', () => {
  it('a new promotion names the first other unit type and writes nothing else', () => {
    const d = addPromotion(base(), A)
    expect(unit(d, A).promotions).toEqual([{ to: B }])
  })
  it('never offers the unit type itself: a game with one unit type cannot add a promotion', () => {
    const d = base()
    const one = { ...d, units: [d.units![0]] }
    expect(addPromotion(one, A)).toBe(one)
  })
  it('writes no default: from level 1, "nothing at the level", the game’s level rule, no items', () => {
    let d = patchPromotion(addPromotion(base(), A), A, 0, { ...promotionPatches.fromLevel(5), ...promotionPatches.atLevel('offer'), ...promotionPatches.levelAfter('keep'), ...promotionPatches.items(['x']) })
    expect(promotionsOf(unit(d, A))[0]).toEqual({ to: B, fromLevel: 5, atLevel: 'offer', levelAfter: 'keep', items: ['x'] })
    d = patchPromotion(d, A, 0, { ...promotionPatches.fromLevel(1), ...promotionPatches.atLevel('off'), ...promotionPatches.levelAfter('game'), ...promotionPatches.items([]) })
    expect(promotionsOf(unit(d, A))[0]).toEqual({ to: B })
  })
  it('keeps keys it does not know, removes the last row and the key with it, and reorders', () => {
    let d = patchPromotion(addPromotion(base(), A), A, 0, { mystery: 1 })
    d = addPromotion(d, A)
    d = patchPromotion(d, A, 1, { to: C })
    d = movePromotion(d, A, 1, 0)
    expect(promotionsOf(unit(d, A)).map((p) => p.to)).toEqual([C, B])
    expect(promotionsOf(unit(d, A))[1]).toMatchObject({ mystery: 1 })
    d = removePromotion(removePromotion(d, A, 0), A, 0)
    expect(unit(d, A)).not.toHaveProperty('promotions')
  })
  it('stat changes: the difference of the two types’ base stats, or a table that starts from it', () => {
    const d = base()
    const stat = d.game!.stats[0].id
    const a = { ...unit(d, A), stats: { [stat]: 5 } }
    const b = { ...unit(d, B), stats: { [stat]: 9 } }
    const e = { ...d, units: [a, b, ...d.units!.slice(2)] }
    expect(baseDifference(e, A, B)).toEqual({ [stat]: 4 })
    expect(setGain({ [stat]: 4 }, stat, undefined)).toEqual({})
    expect(setGain({}, stat, -2)).toEqual({ [stat]: -2 })
  })
  it('rules.promotion: "starts again at 1" is not written, "stays" is, and an empty block goes', () => {
    let d = setLevelAfterRule(base(), 'keep')
    expect(d.game!.rules!.promotion).toEqual({ levelAfter: 'keep' })
    expect(gameLevelAfter(d)).toBe('keep')
    d = setLevelAfterRule(d, 'reset')
    expect(d.game!.rules).not.toHaveProperty('promotion')
    expect(gameLevelAfter(d)).toBe('reset')
  })
})

describe('promotion items', () => {
  it('only a usable item with the switch counts, and switching it off lets every promotion go of it', () => {
    let d = addItem(base(), 'Writ', 'writ')
    expect(promotionItems(d)).toEqual([])
    d = setItemPromotes(d, 'writ', true)
    expect(promotionItems(d).map((i) => i.id)).toEqual(['writ'])
    d = patchPromotion(addPromotion(d, A), A, 0, { items: ['writ'] })
    d = setItemPromotes(d, 'writ', false)
    expect(d.items!.find((i) => i.id === 'writ')).not.toHaveProperty('promotes')
    expect(unit(d, A).promotions![0]).not.toHaveProperty('items')
  })
  it('a promotion item that is only a promotion item passes the usable-item check', () => {
    let d = addItem(base(), 'Writ', 'writ')
    d = { ...d, items: d.items!.map((i) => (i.id === 'writ' ? { id: 'writ', name: 'Writ', kind: 'usable' as const, promotes: true } : i)) }
    expect(itemChecks(d).filter((c) => c.text.startsWith('Writ'))).toEqual([])
  })
  it('turning the item into another kind clears the switch and the promotions that listed it; deleting it does too', () => {
    let d = setItemPromotes(addItem(base(), 'Writ', 'writ'), 'writ', true)
    d = patchPromotion(addPromotion(d, A), A, 0, { items: ['writ'] })
    expect(itemUsageLines(d, 'writ').some((l) => l.text.includes('promote'))).toBe(true)
    const acc = setItemKind(d, 'writ', 'accessory')
    expect(acc.items!.find((i) => i.id === 'writ')).not.toHaveProperty('promotes')
    expect(unit(acc, A).promotions![0]).not.toHaveProperty('items')
    const gone = deleteItem(d, 'writ')
    expect(unit(gone, A).promotions![0]).not.toHaveProperty('items')
  })
})

describe('deleting what a promotion or an ability names', () => {
  it('a unit type: promotions into it go, an ability that summons it is deleted, and both are listed first', () => {
    let d = switchOnFeature(addPromotion(base(), A), 'abilities')
    d = addAbility(d, 'Call', 'call')
    d = { ...d, abilities: [setSummon(setEffectKind(d, d.abilities![0], 'summon'), { type: B })] }
    d = { ...d, units: d.units!.map((u) => (u.id === C ? { ...u, abilities: ['call'] } : u)) }
    const lines = findUsages(d, 'unit', B).map((l) => l.text)
    expect(lines).toContain(`Unit “${unit(d, A).name}” can promote into it (that promotion is removed)`)
    expect(lines).toContain('Ability “Call” summons it (the ability is deleted too)')
    const e = cleanUnit(d, B)
    expect(unit(e, A)).not.toHaveProperty('promotions')
    expect(e.abilities).toEqual([])
    expect(unit(e, C)).not.toHaveProperty('abilities')
  })
  it('a stat leaves promotion changes and "gives when behind" numbers', () => {
    const d = base()
    const s = d.game!.stats[0].id
    const e = { ...d, units: d.units!.map((u) => (u.id === A ? { ...u, promotions: [{ to: B, gains: { [s]: 2 } }], pairBonus: { [s]: 1 } } : u)) }
    const f = cleanStat(e, s)
    expect(unit(f, A).promotions![0]).not.toHaveProperty('gains')
    expect(unit(f, A)).not.toHaveProperty('pairBonus')
  })
})

describe('promotion checks', () => {
  const withItem = (promotes = true) => {
    let d = addItem(base(), 'Writ', 'writ')
    d = { ...d, items: d.items!.map((i) => (i.id === 'writ' ? { id: 'writ', name: 'Writ', kind: 'usable' as const, heal: 5, ...(promotes ? { promotes: true } : {}) } : i)) }
    return d
  }
  it('a promotion item no promotion lists', () => {
    const d = withItem()
    expect(promotionChecks(d).map((c) => c.text)).toEqual(['Writ: no unit type’s promotion uses this item, so it promotes nobody. Add it to a promotion on the Units screen.'])
  })
  it('a promotion that needs an item nobody hands out; any hand-out clears it; a level trigger does not need one', () => {
    let d = patchPromotion(addPromotion(withItem(), A), A, 0, { items: ['writ'] })
    expect(promotionChecks(d).map((c) => c.text).join(' ')).toMatch(/nobody is ever given its promotion item/)
    expect(itemIsHandedOut(d, 'writ')).toBe(false)
    const shop = { ...d, structures: [{ id: 's', name: 'Shop', sells: ['writ'] }, ...(d.structures ?? [])] } as ProjectDoc
    expect(itemIsHandedOut(shop, 'writ')).toBe(true)
    expect(promotionChecks(shop)).toEqual([])
    const ev: StoryEvent = { id: 'gift', when: { battle: 'start' }, do: [{ giveItem: 'writ', to: {} }] }
    expect(itemIsHandedOut(withMap(d, (m) => ({ ...m, events: [ev] })), 'writ')).toBe(true)
    d = patchPromotion(d, A, 0, { atLevel: 'offer' })
    expect(promotionChecks(d).filter((c) => c.text.includes('nobody is ever given'))).toEqual([])
  })
  it('a chain that comes back to its start is a note, said once', () => {
    let d = patchPromotion(addPromotion(base(), A), A, 0, { to: B })
    d = patchPromotion(addPromotion(d, B), B, 0, { to: A })
    const notes = promotionChecks(d).filter((c) => c.text.startsWith('Note:'))
    expect(notes).toHaveLength(1)
    expect(notes[0].ids).toEqual(expect.arrayContaining([A, B]))
  })
  it('says the loader’s refusals in plain words', () => {
    const d = base()
    const bad = { ...d, units: d.units!.map((u) => (u.id === A ? { ...u, promotions: [{ to: A }, { to: 'nope' }, { to: B, atLevel: 'auto' as const, items: ['ghost'] }] } : u)) }
    const text = promotionChecks(bad).map((c) => c.text).join('\n')
    expect(text).toMatch(/cannot promote into itself/)
    expect(text).toMatch(/“nope” is not a unit type/)
    expect(text).toMatch(/not a promotion item/)
    if (bad.game!.rules?.progression?.enabled !== true) expect(text).toMatch(/needs experience and levels/)
  })
})

describe('story: Promote a unit and "A unit is promoted"', () => {
  it('start from valid values and read as plain sentences', () => {
    const d = withMap(base(), (m) => ({ ...m, units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'eirik' } : u)) }))
    const ctx = { doc: d, map: d.maps[mapId(d)]! }
    const cmd = newCommand(ctx, 'promote')
    expect(commandKind(cmd)).toBe('promote')
    expect(cmd).toEqual({ promote: 'eirik', to: A })
    expect(commandText(ctx, cmd)).toMatch(/^Promotes .* into an? /)
    expect(commandText(ctx, { promote: 'eirik' })).toMatch(/its only promotion/)
    const t = newTrigger(ctx, 'promoted')
    expect(triggerKind(t)).toBe('promoted')
    expect(triggerText(ctx, { promoted: { unit: 'eirik' } })).toMatch(/is promoted$/)
  })
  it('deleting the unit type a Promote command names removes the command; the unit it names is checked', () => {
    let d = withMap(base(), (m) => ({ ...m, units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'eirik' } : u)) }))
    const ev: StoryEvent = { id: 'up', when: { battle: 'start' }, do: [{ promote: 'eirik', to: B }, { say: 'hi' }] }
    d = withMap(d, (m) => ({ ...m, events: [ev] }))
    expect(storyUsages(d, [{ kind: 'type', id: B }]).map((u) => u.text).join(' ')).toMatch(/Promotes/)
    const gone = cleanUnit(d, B)
    expect(gone.maps[mapId(gone)]!.events![0].do).toEqual([{ say: 'hi' }])
    const bad = patchEvent(d.maps[mapId(d)]!, 'up', { do: [{ promote: 'ghost', to: B }] })
    expect(storyChecks({ ...d, maps: { ...d.maps, [mapId(d)]: bad } }, mapId(d)).length).toBeGreaterThan(0)
  })
})
