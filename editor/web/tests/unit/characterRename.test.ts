// Renaming a character's id (Characters screen > Advanced > Rename): it must follow into EVERY place that names the character, in one edit.
// The game is tests/fixtures/srpg-rename, built so that `hero` is spelled in every kind of place the loaders accept a character id (see the
// list at the top of model/characterRename.ts). The expected result of renaming `hero` is a plain text replacement of the quoted id.
import { describe, expect, it } from 'vitest'
import { reconcileCampaign } from '../../src/model/campaignRefs'
import { characterIdProblem, formulaMentions, renameCharacter, renameEffects } from '../../src/model/characterRename'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import type { ProjectDoc } from '../../src/model/types'
import { loadRenameFixture } from './fixtures'

const OLD = 'hero'
const NEW = 'captain'
const text = (d: unknown): string => JSON.stringify(d)
const count = (d: unknown, id: string): number => text(d).split(`"${id}"`).length - 1
const swapped = (d: ProjectDoc): ProjectDoc => JSON.parse(text(d).split(`"${OLD}"`).join(`"${NEW}"`))
const renamed = (d: ProjectDoc = loadRenameFixture()): ProjectDoc => { const r = renameCharacter(d, OLD, NEW); expect(r.error).toBeNull(); return r.doc }
const at = (d: ProjectDoc, mapId: string) => d.maps[mapId]!

describe('the fixture uses every kind of reference to a character', () => {
  const d = loadRenameFixture()
  const field = at(d, 'field')
  it.each<[string, () => boolean]>([
    ['the character entry', () => d.characters!.some((c) => c.id === OLD)],
    ['its own quotes: a line it speaks and a line with its portrait', () => text(d.characters![0].quotes).includes('"speaker":"hero","portrait":"hero"')],
    ['its own defeat quote: a line it speaks', () => d.characters![0].quotes!.defeat!.falls!.some((l) => l.speaker === OLD)],
    ['another character’s quote against it', () => d.characters!.some((c) => c.quotes?.battle?.some((q) => q.against === OLD))],
    ['another character’s quote: a line it speaks', () => d.characters![1].quotes!.battle![0].lines.some((l) => l.speaker === OLD)],
    ['another character’s defeat quote: a line it speaks with its portrait', () => d.characters![1].quotes!.defeat!.falls!.some((l) => l.speaker === OLD && l.portrait === OLD)],
    ['a support pair (as the first and as the second)', () => d.supports!.pairs[0].a === OLD && d.supports!.pairs[1].b === OLD],
    ['a support conversation line', () => d.supports!.pairs[0].levels[0].talk!.some((l) => l.speaker === OLD)],
    ['a weapon lock', () => d.weapons!.some((w) => w.wieldedBy?.includes(OLD))],
    ['a staff lock', () => d.items!.some((i) => i.kind === 'staff' && i.wieldedBy?.includes(OLD))],
    ['the general portrait', () => OLD in d.graphics!.portraits!],
    ['a style’s portrait', () => OLD in (d.graphics!.styles!.night as { portraits: object }).portraits],
    ['a battle animation rule', () => d.graphics!.battle!.assign!.some((a) => a.character === OLD)],
    ['a placed character (two maps)', () => at(d, 'field').units!.some((u) => u.character === OLD) && at(d, 'keep').units!.some((u) => u.character === OLD)],
    ['the preparation lists (required with a slot)', () => (field.prep!.required as Array<{ character: string }>)[0].character === OLD],
    ['a place’s visitors', () => field.structures![0].visitUnits!.includes(OLD)],
    ['a unit that seeks it, and one that prefers it', () => field.units!.some((u) => (u.behaviour as { unit?: string })?.unit === OLD) && field.units!.some((u) => (u.behaviour as { prefer?: { unit?: string } })?.prefer?.unit === OLD)],
    ['objectives: a win by reaching, a lose by being defeated', () => count(field.objectives, OLD) === 2],
    ['events: talk and to', () => text(field.events).includes('"talk":"hero","to":"ally"')],
    ['events: fight / with, defeated / by', () => text(field.events).includes('"fight":{"unit":"hero"}') && text(field.events).includes('"defeated":{"unit":"hero"},"by"')],
    ['events: a by filter that is a plain id, a reach by, a place by', () => text(field.events).includes('"by":"hero"') && text(field.events).includes('"by":{"unit":"hero"}')],
    ['events: conditions alive, sees, carries unit, support pair', () => text(field.events).includes('"alive":"hero"') && text(field.events).includes('"sees":"hero"') && text(field.events).includes('"carries":"potion","unit":"hero"') && text(field.events).includes('"support":["hero","ally"]')],
    ['commands: moveUnit, removeUnit-free focus, changeSide, giveItem, takeItem', () => ['"moveUnit":"hero"', '"changeSide":"ally"', '"giveItem":"potion","to":"hero"', '"takeItem":"potion","from":"hero"', '"focus":"hero"'].every((s) => text(field.events).includes(s))],
    ['commands: readings of level, hp, maxHp and stat/of', () => ['"level":"hero"', '"hp":"hero"', '"maxHp":"hero"', '"of":"hero"'].every((s) => text(field.events).includes(s))],
    ['commands: setBehaviour target and favourite, addUnit behaviour, addUnit character', () => text(field.events).includes('"setBehaviour":{"mode":"seek","unit":"hero"') && text(field.events).includes('"behaviour":{"mode":"seek","unit":"hero","prefer":{"unit":"ally"}}') && text(field.events).includes('"character":"rival"')],
    ['commands: say speaker and portrait, a choice’s speaker and portrait, addSupport', () => text(field.events).includes('"choice":"Hold?","speaker":"hero","portrait":"hero"') && text(field.events).includes('"addSupport":["ally","hero"]')],
    ['commands: promote, teachSkill (bare and as a filter), forgetSkill, addCondition (bare and as a filter), removeCondition', () => ['"promote":"hero"', '"openChest":"c1","by":"hero"', '"teachSkill":"tough","to":"hero"', '"teachSkill":"tough","to":{"unit":"hero"}', '"forgetSkill":"tough","from":{"unit":"hero"}', '"addCondition":"poison","to":"hero"', '"addCondition":"poison","to":{"unit":"hero"}', '"removeCondition":"poison","from":{"unit":"hero"}'].every((s) => text(field.events).includes(s))],
    ['the promoted trigger, and the hasSkill and hasCondition conditions', () => text(field.events).includes('"promoted":{"unit":"hero"}') && text(field.events).includes('"hasSkill":"tough","unit":{"unit":"hero"}') && text(field.events).includes('"hasCondition":"poison","unit":{"unit":"hero"}')],
    ['a second map’s event line and behaviour', () => text(at(d, 'keep').events).includes('"speaker":"hero"') && text(at(d, 'keep').units).includes('"unit":"hero"')],
    ['campaign scenes: say, alive/dead, level and stat readings, addSupport, choice, support condition, takeItem, else', () => ['"speaker":"hero","portrait":"hero"', '"alive":"hero"', '"level":"hero"', '"of":"hero"', '"addSupport":["hero","ally"]', '"support":["ally","hero"]', '"from":"hero"', '"dead":"hero"'].every((s) => text(d.campaign).includes(s))],
  ])('%s', (_name, present) => { expect(present()).toBe(true) })
})

describe('renaming', () => {
  it('leaves nothing with the old id and gives exactly what a text replacement of the quoted id gives', () => {
    const before = loadRenameFixture()
    const after = renamed(before)
    expect(count(after, OLD)).toBe(0)
    expect(count(before, OLD)).toBeGreaterThan(60)
    expect(count(after, NEW)).toBe(count(before, OLD))   // every place that said it now says the new one: none lost, none added
    expect(after).toEqual(swapped(before))
  })

  it('is one Undo step', () => {
    const before = loadRenameFixture()
    let h = initHistory(before)
    h = pushEdit(h, renamed(h.present), undefined, 0)
    expect(count(h.present, OLD)).toBe(0)
    h = undo(h)
    expect(h.present).toBe(before)
  })

  it('does not touch what the rename is not about', () => {
    const before = loadRenameFixture()
    const after = renamed(before)
    expect(after.game).toBe(before.game)
    expect(after.units).toBe(before.units)
    expect(after.terrain).toBe(before.terrain)
    expect(after.structures).toBe(before.structures)
    expect(at(after, 'field').units!.find((u) => u.id === 'g1')).toEqual(at(before, 'field').units!.find((u) => u.id === 'g1') && { ...at(before, 'field').units!.find((u) => u.id === 'g1')!, behaviour: { mode: 'advance', prefer: { unit: NEW } } })
    expect(after.characters!.map((c) => c.name)).toEqual(['Aldric', 'Brenna', 'Corin'])  // names are people’s words, not ids
    expect(reconcileCampaign(before, after)).toBe(after)  // the store’s campaign clean-up has nothing to remove
  })

  it('keeps the order of the lists, the portraits and the pairs', () => {
    const after = renamed()
    expect(after.characters!.map((c) => c.id)).toEqual([NEW, 'ally', 'rival'])
    expect(Object.keys(after.graphics!.portraits!)).toEqual([NEW, 'ally'])
    expect(after.supports!.pairs.map((p) => [p.a, p.b])).toEqual([[NEW, 'ally'], ['rival', NEW]])
  })

  it('does nothing for the same id or an unknown character', () => {
    const d = loadRenameFixture()
    expect(renameCharacter(d, OLD, OLD)).toEqual({ doc: d, error: null })
    expect(renameCharacter(d, 'nobody', NEW).doc).toBe(d)
  })

  it('moves a portrait line only when the portrait itself is renamed', () => {
    const d = loadRenameFixture()
    const noPortrait: ProjectDoc = { ...d, graphics: { ...d.graphics!, portraits: { ally: d.graphics!.portraits!.ally }, styles: {} } }
    const after = renamed(noPortrait)
    expect(text(after.maps.field).includes('"portrait":"hero"')).toBe(true)   // there was no picture of that name: the line is left as it was
    expect(text(after.maps.field).includes('"speaker":"captain"')).toBe(true)
  })
})

describe('what is refused, in plain words, and what is left alone', () => {
  const d = loadRenameFixture()
  it.each<[string, string, RegExp]>([
    ['an empty name', '', /Type the new internal name/],
    ['spaces and other characters', 'my hero', /letters, digits/],
    ['a name longer than 64', 'x'.repeat(65), /At most 64/],
    ['another character’s id', 'ally', /already has this internal name/],
    ['the story name of a placed unit on a map', 'g1', /map “Field” already has the story name “g1”/],
    ['the story name of a unit an event adds', 'g3', /already has the story name “g3”/],
  ])('refuses %s', (_what, to, message) => {
    expect(characterIdProblem(d, OLD, to)).toMatch(message)
    const r = renameCharacter(d, OLD, to)
    expect(r.error).toMatch(message)
    expect(r.doc).toBe(d)
  })

  it('refuses a name another portrait already has (the portrait would collide), but not when the character has no portrait', () => {
    const clash: ProjectDoc = { ...d, graphics: { ...d.graphics!, portraits: { ...d.graphics!.portraits!, captain: { sheet: 'units', cell: [0, 2] } } } }
    expect(characterIdProblem(clash, OLD, NEW)).toMatch(/already a portrait called “captain”/)
    const none: ProjectDoc = { ...clash, graphics: { ...clash.graphics!, portraits: { captain: { sheet: 'units', cell: [0, 2] } }, styles: {} } }
    expect(characterIdProblem(none, OLD, NEW)).toBeNull()
  })

  it('accepts a name that differs only by case, and one an unrelated list already holds', () => {
    expect(characterIdProblem(d, OLD, 'Hero')).toBeNull()
    expect(characterIdProblem(d, OLD, 'g1x')).toBeNull()
  })

  it('formulas and scripts that spell the old id in quotes are listed, not rewritten', () => {
    const withFormula: ProjectDoc = {
      ...d,
      game: { ...d.game!, formulas: { ...d.game!.formulas, damage: 'a.str + w.might - b.def + (a.character == "hero" and 1 or 0)' } },
      skills: [{ id: 's', name: 'Lone Wolf', blocks: [{ when: 'x', if: [{ formula: "a.character == 'hero'" }] }] }] as ProjectDoc['skills'],
      scripts: { 'x.lua': 'local who = "hero"' },
    }
    expect(formulaMentions(withFormula, OLD)).toEqual(['Skill “Lone Wolf” has a formula that spells “hero” in quotes', 'The rules’ formulas spell “hero” in quotes', 'The script x.lua spells “hero” in quotes'])
    expect(formulaMentions(d, OLD)).toEqual([])
    expect(text(renamed(withFormula).game)).toContain('== \\"hero\\"')   // still spelled: the rename does not rewrite formulas
  })

  it('lists, for the dialog, where the new name will appear', () => {
    const lines = renameEffects(d, OLD)
    expect(lines).toEqual(expect.arrayContaining([
      'The map “Field” places it', 'The map “Keep” places it',
      expect.stringContaining('The map “Field”: its events, objectives'),
      expect.stringContaining('The campaign'), expect.stringContaining('Supports'), expect.stringContaining('quotes'),
      expect.stringContaining('locked to it'), expect.stringContaining('portrait'),
    ]))
  })
})
