// E0: the strategy-RPG skeleton (docs/SRPG.md "Editor"): the four sections, the switches, the sidebar rule, the
// references and the save body.
import { describe, expect, it } from 'vitest'
import { problemTarget } from '../../src/model/problems'
import type { Character } from '../../src/model/srpgTypes'
import { canSwitchOn, featureCount, srpgSectionShown, switchOffFeature, switchOnFeature } from '../../src/model/srpgFeatures'
import {
  abilityUsageLines, characterUsageLines, deleteAbility, deleteCharacter, deleteSkill, newAbilityId, newCharacterId, newSkillId,
  skillUsageLines,
} from '../../src/model/srpgRefs'
import { withAbilities, withCharacters, withSkills, withSupports } from '../../src/model/doc'
import type { ProjectDoc } from '../../src/model/types'
import { parseRoute } from '../../src/router'
import { buildBody } from '../../src/store/autosave'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import { loadDemo } from './fixtures'

const base = (): ProjectDoc => loadDemo('demo-srpg')
const chars: Character[] = [{ id: 'eirik', name: 'Eirik', type: 'knight', skills: ['heavy_blow'], abilities: ['shove'], learns: [{ level: 5, skill: 'heavy_blow' }] }, { id: 'lyra', name: 'Lyra', type: 'archer' }]

/** A game that uses every new list, wired together. */
function full(): ProjectDoc {
  const d = base()
  const unit = d.units![0]
  return {
    ...d,
    units: [{ ...unit, skills: ['heavy_blow'], learns: [{ level: 3, skill: 'heavy_blow' }], abilities: ['shove'] }, ...d.units!.slice(1)],
    weapons: d.weapons!.map((w, i) => (i === 0 ? { ...w, skills: ['heavy_blow'] } : w)),
    characters: chars,
    skills: [
      { id: 'heavy_blow', name: 'Heavy Blow', blocks: [{ when: 'strike', do: [{ damagePercent: 200 }] }] },
      { id: 'pusher', name: 'Pusher', blocks: [{ when: 'always', do: [{ ability: 'shove' }] }, { when: 'combat', do: [{ crit: 5 }] }] },
    ],
    abilities: [{ id: 'shove', name: 'Shove' }],
    supports: { pairs: [{ a: 'eirik', b: 'lyra' }] },
    maps: Object.fromEntries(Object.entries(d.maps).map(([k, m], i) => [k, i === 0 && m ? { ...m, units: [...(m.units ?? []), { character: 'eirik', side: 'blue', x: 0, y: 0, type: '' }] } : m])),
  }
}

describe('strategy-RPG sections in the document', () => {
  it('a game without the files has four nulls and sends nothing for them', () => {
    const d = base()
    expect(d.characters).toBeNull()
    expect(d.supports).toBeNull()
    expect(buildBody(d, d, 'r', true)).toMatchObject({ baseRevision: 'r' })
    const body = buildBody(d, d, 'r', true)!
    for (const k of ['characters', 'skills', 'abilities', 'supports']) expect(body).not.toHaveProperty(k)
  })
  it('sends a list when it changed and false (= remove the file) when it went back to none', () => {
    const a = base()
    const b = withCharacters(a, (l) => [...l, chars[0]])
    expect(buildBody(b, a, 'r', false)!.characters).toEqual([chars[0]])
    expect(buildBody(a, b, 'r', false)!.characters).toBe(false)
    expect(buildBody(a, b, 'r', false)).not.toHaveProperty('skills')
  })
  it('switching a feature on sends an empty list, which the helper writes so the switch is remembered', () => {
    const a = base()
    const on = switchOnFeature(a, 'skills')
    expect(on.skills).toEqual([])
    expect(buildBody(on, a, 'r', false)!.skills).toEqual([])
  })
  it('supports: the object while switched on (also when empty), false when removed', () => {
    const a = base()
    const withPair = { ...a, supports: { pairs: [{ a: 'x', b: 'y' }] } }
    expect(buildBody(withPair, a, 'r', false)!.supports).toEqual({ pairs: [{ a: 'x', b: 'y' }] })
    expect(buildBody(a, withPair, 'r', false)!.supports).toBe(false)
    expect(buildBody(switchOnFeature({ ...a, characters: [] }, 'supports'), a, 'r', false)!.supports).toEqual({ pairs: [] }) // nothing in it yet: still written
  })
  it('one undo step removes a feature again and the save body follows', () => {
    const start = full()
    let h = initHistory(start)
    h = pushEdit(h, switchOffFeature(h.present, 'skills'), undefined, 0)
    expect(h.present.skills).toBeNull()
    expect(buildBody(h.present, start, 'r', false)!.skills).toBe(false)
    h = undo(h)
    expect(h.present).toBe(start)
  })
  it('edits on a missing supports are a no-op; unknown keys survive an edit', () => {
    const a = base()
    expect(withSupports(a, (s) => ({ ...s, pairs: [] }))).toBe(a)
    const keep = { ...a, characters: [{ ...chars[1], mystery: { a: 1 } }] }
    const edited = withCharacters(keep, (l) => l.map((c) => ({ ...c, name: 'New' })))
    expect(edited.characters![0]).toMatchObject({ mystery: { a: 1 }, name: 'New' })
    expect(withSkills(a, (l) => l).skills).toEqual([]) // the first edit creates the list
    expect(withAbilities(a, (l) => [...l, { id: 'x', name: 'X' }]).abilities).toHaveLength(1)
  })
})

describe('switches', () => {
  it('supports need characters', () => {
    const a = base()
    expect(canSwitchOn(a, 'supports')).toBe(false)
    expect(switchOnFeature(a, 'supports')).toBe(a)
    expect(switchOnFeature(switchOnFeature(a, 'characters'), 'supports').supports).toEqual({ pairs: [] })
  })
  it('switching off removes the list and everything that named its items', () => {
    const d = full()
    const noSkills = switchOffFeature(d, 'skills')
    expect(noSkills.skills).toBeNull()
    expect(noSkills.units![0]).not.toHaveProperty('skills')
    expect(noSkills.units![0]).not.toHaveProperty('learns')
    expect(noSkills.characters![0]).not.toHaveProperty('skills')
    expect(noSkills.weapons![0]).not.toHaveProperty('skills')
    const noChars = switchOffFeature(d, 'characters')
    expect(noChars.characters).toBeNull()
    expect(noChars.supports).toBeNull()
    expect(Object.values(noChars.maps).every((m) => !m!.units!.some((u) => u.character))).toBe(true)
    const noAbilities = switchOffFeature(d, 'abilities')
    expect(noAbilities.units![0]).not.toHaveProperty('abilities')
    expect(noAbilities.skills![1].blocks).toEqual([{ when: 'combat', do: [{ crit: 5 }] }])
  })
  it('counts what a feature holds', () => {
    const d = full()
    expect(featureCount(d, 'characters')).toBe(2)
    expect(featureCount(d, 'supports')).toBe(1)
    expect(featureCount(base(), 'skills')).toBe(0)
  })
})

describe('sidebar rule and routes', () => {
  it('a screen is listed once its feature exists; the open one stays', () => {
    const d = base()
    expect(srpgSectionShown(d, 'units', 'game')).toBe(true)
    expect(srpgSectionShown(d, 'characters', 'game')).toBe(false)
    expect(srpgSectionShown(d, 'characters', 'characters')).toBe(true)
    expect(srpgSectionShown(switchOnFeature(d, 'characters'), 'characters', 'game')).toBe(true)
  })
  it('the four routes parse, and problems in the files go to their screens', () => {
    for (const s of ['characters', 'skills', 'abilities', 'supports']) {
      expect(parseRoute(`/p/demo/${s}/x`)).toEqual({ kind: 'project', id: 'demo', section: s, itemId: 'x' })
      expect(problemTarget(`data/${s}.json`, null).section).toBe(s)
    }
  })
})

describe('ids and references', () => {
  it('ids are made from the name and avoid taken ones; a character also avoids ids of map units', () => {
    const d = full()
    expect(newSkillId(d, 'Heavy Blow')).toBe('heavy_blow_2')
    expect(newAbilityId(d, 'Call Hound')).toBe('call_hound')
    expect(newCharacterId(d, 'Eirik')).toBe('eirik_2')
    const withUnit = { ...d, maps: { ...d.maps, extra: { ...d.maps[Object.keys(d.maps)[0]]!, units: [{ type: 'knight', side: 'blue', x: 1, y: 1, id: 'lyra_x' }] } } }
    expect(newCharacterId(withUnit, 'Lyra x')).toBe('lyra_x_2')
    expect(newCharacterId(base(), '')).toBe('character')
  })
  it('usage lines name who uses a skill, an ability and a character', () => {
    const d = full()
    const sk = skillUsageLines(d, 'heavy_blow').map((u) => u.text).join('|')
    expect(sk).toMatch(/Unit .* has it/)
    expect(sk).toMatch(/Character “Eirik” has it/)
    expect(sk).toMatch(/Weapon .* gives it/)
    expect(abilityUsageLines(d, 'shove').map((u) => u.text).join('|')).toMatch(/Skill “Pusher” grants it/)
    expect(characterUsageLines(d, 'lyra').map((u) => u.section)).toEqual(['supports'])
    expect(characterUsageLines(d, 'eirik').map((u) => u.section).sort()).toEqual(['maps', 'supports'])
    expect(skillUsageLines(d, 'pusher')).toEqual([])
  })
  it('deleting cleans every reference in the same edit and leaves untouched lists the same objects', () => {
    const d = full()
    const a = deleteSkill(d, 'heavy_blow')
    expect(a.skills!.map((s) => s.id)).toEqual(['pusher'])
    expect(a.units![0]).not.toHaveProperty('skills')
    expect(a.characters![0]).not.toHaveProperty('learns')
    expect(a.items).toBe(d.items)
    const b = deleteAbility(d, 'shove')
    expect(b.abilities).toEqual([])
    expect(b.characters![0]).not.toHaveProperty('abilities')
    const c = deleteCharacter(d, 'eirik')
    expect(c.characters!.map((x) => x.id)).toEqual(['lyra'])
    expect(c.supports!.pairs).toEqual([])
    expect(deleteSkill(d, 'nope')).toStrictEqual({ ...d })
  })
})
