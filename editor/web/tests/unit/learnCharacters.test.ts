// Every done test of the lesson "Characters and skills" against small documents: false at the start, false for near misses,
// true when done.
import { describe, expect, it } from 'vitest'
import { advance } from '../../src/learn/runner'
import { LESSONS } from '../../src/learn/lessons'
import { characters } from '../../src/learn/lessons/characters'
import type { SignalName } from '../../src/learn/signals'
import type { LessonProgress } from '../../src/learn/state'
import { switchOnFeature } from '../../src/model/srpgFeatures'
import { addCharacter } from '../../src/model/characters'
import { addSkill } from '../../src/model/skills'
import { guessStats, installStarter } from '../../src/model/starterSkills'
import { setFeature } from '../../src/model/rules'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

const fresh = (): ProjectDoc => { const d = structuredClone(loadTemplate('blank')); d.game!.name = 'Practice: Characters and skills'; return d }
const NONE: ReadonlySet<SignalName> = new Set()
const step = (id: string) => characters.steps.find((s) => s.id === id)!
const done = (id: string, doc: ProjectDoc, signals: SignalName[] = []) => step(id).done(doc, 'game', new Set(signals))
const withCharacters = (): ProjectDoc => switchOnFeature(fresh(), 'characters')
const withMira = (): ProjectDoc => addCharacter(withCharacters(), 'Mira', 'mira')
const withStarter = (): ProjectDoc => { const d = switchOnFeature(withMira(), 'skills'); return installStarter(d, ['tough', 'light_step'], guessStats(d)).doc }

describe('lesson data', () => {
  it('is registered, with unique step ids and a template that exists', () => {
    expect(LESSONS).toContain(characters)
    expect(new Set(characters.steps.map((s) => s.id)).size).toBe(characters.steps.length)
    expect(characters.steps.length).toBeGreaterThanOrEqual(5)
    expect(characters.steps.length).toBeLessThanOrEqual(10)
    expect(characters.template).toBe('blank')
    expect(characters.playable).toBe(true)
    for (const s of characters.steps) { expect(s.title.length).toBeGreaterThan(0); expect(s.text.length).toBeGreaterThan(0) }
  })
  it('uses no programmer words in the texts', () => {
    const all = [characters.title, characters.description, ...characters.summary, ...characters.steps.flatMap((s) => [s.title, s.text])].join(' ')
    expect(all).not.toMatch(/\b(tile|json|id|expression|formula|lua|script|look|looks)\b/i)
  })
  it('starts from the blank template as the steps assume: nothing of the strategy-RPG parts, one soldier type, levelling off', () => {
    const d = fresh()
    expect(d.characters).toBeNull()
    expect(d.skills).toBeNull()
    expect(d.units!.map((u) => u.id)).toEqual(['soldier'])
    expect(d.game!.rules?.progression?.enabled).not.toBe(true)
    expect(d.maps.start!.units!.map((u) => u.side).sort()).toEqual(['blue', 'red'])
  })
  it('has no step that is already true at the start', () => {
    for (const s of characters.steps) expect(s.done(fresh(), 'game', NONE), s.id).toBe(false)
  })
})

describe('steps: the three switches', () => {
  it('characters, skills and levelling up each tick on their own switch', () => {
    expect(done('characters-on', withCharacters())).toBe(true)
    expect(done('skills-on', withCharacters())).toBe(false)
    expect(done('skills-on', switchOnFeature(fresh(), 'skills'))).toBe(true)
    expect(done('levelling-on', switchOnFeature(fresh(), 'skills'))).toBe(false)
    expect(done('levelling-on', setFeature(fresh(), 'progression', true))).toBe(true)
  })
})

describe('steps: new-character, place', () => {
  it('a character alone is not done: it needs a personal bonus (a number that is not 0)', () => {
    const d = withMira()
    expect(done('new-character', d)).toBe(false)
    d.characters![0].stats = { hp: 0 }
    expect(done('new-character', d)).toBe(false)
    d.characters![0].stats = { hp: 5 }
    expect(done('new-character', d)).toBe(true)
  })
  it('its "Take me there" opens the character, or nothing while there is none', () => {
    expect(step('new-character').where!.item!(fresh())).toBeUndefined()
    expect(step('new-character').where!.item!(withMira())).toBe('mira')
  })
  it('placing needs a map unit that is the character; a plain soldier does not count', () => {
    const d = withMira()
    expect(done('place', d)).toBe(false)
    d.maps.start!.units!.push({ character: 'mira', side: 'blue', x: 1, y: 1 })
    expect(done('place', d)).toBe(true)
  })
})

describe('steps: starter, personal-skill, learned-skill', () => {
  it('the starter set ticks when any of its skills is in the game', () => {
    const d = switchOnFeature(withMira(), 'skills')
    expect(done('starter', d)).toBe(false)
    d.skills = [{ id: 'mine', name: 'Mine' }]
    expect(done('starter', d)).toBe(false) // a skill of your own is not the starter set
    expect(done('starter', withStarter())).toBe(true)
  })
  it('a personal skill and a learned skill are separate lists of the character', () => {
    const d = withStarter()
    expect(done('personal-skill', d)).toBe(false)
    d.characters![0].learns = [{ level: 3, skill: 'tough' }]
    expect(done('personal-skill', d)).toBe(false)
    expect(done('learned-skill', d)).toBe(true)
    d.characters![0].learns = undefined
    d.characters![0].skills = ['tough']
    expect(done('personal-skill', d)).toBe(true)
    expect(done('learned-skill', d)).toBe(false)
  })
})

describe('step: own-skill', () => {
  const withOwn = (): ProjectDoc => addSkill(switchOnFeature(fresh(), 'skills'), 'Sharp Eyes', 'sharp_eyes')
  it('a new skill is valid as it stands, so it is not done until its rule is changed', () => {
    expect(done('own-skill', withOwn())).toBe(false)
  })
  it('changing the rule ticks it, once the editor finds nothing wrong with it', () => {
    const d = withOwn()
    d.skills![0].blocks = [{ when: 'combat', do: [{ damage: 2 }] }]
    expect(done('own-skill', d)).toBe(true)
    d.skills![0].blocks = [{ when: 'combat', do: [] }]
    expect(done('own-skill', d)).toBe(false) // "It does nothing yet"
  })
  it('the starter skills do not count as your own', () => {
    expect(done('own-skill', withStarter())).toBe(false)
  })
  it('settles before ticking, so a number typed digit by digit is not cut off', () => {
    expect(step('own-skill').settleMs).toBeGreaterThan(0)
    expect(step('new-character').settleMs).toBeGreaterThan(0)
  })
})

describe('step: play (editor signal)', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('play', fresh(), ['saved', 'problemsChecked'])).toBe(false)
    expect(done('play', fresh(), ['played'])).toBe(true)
  })
})

describe('the lesson through the runner', () => {
  it('ticks the steps in order as the work is done, never ahead', () => {
    const start: LessonProgress = { state: 'running', step: 0, skipped: [], signals: [], game: 'practice' }
    expect(advance(characters, start, fresh(), 'game').progress.step).toBe(0)
    // Only the later work is done: nothing ticks, because the first step is not.
    const later = addSkill(switchOnFeature(fresh(), 'skills'), 'Sharp Eyes', 'sharp_eyes')
    expect(advance(characters, start, later, 'game').progress.step).toBe(0)
    expect(advance(characters, start, withCharacters(), 'game').progress.step).toBe(1)
  })
})
