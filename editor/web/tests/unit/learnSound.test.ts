// Every done test of the lesson "Sound" against small documents: false at the start, false for near misses, true when done.
import { describe, expect, it } from 'vitest'
import { LESSONS } from '../../src/learn/lessons'
import { sound, START_SOUNDS } from '../../src/learn/lessons/sound'
import { START_VOLUME } from '../../src/learn/helpers/sound'
import type { SignalName } from '../../src/learn/signals'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

const fresh = (): ProjectDoc => { const d = structuredClone(loadTemplate('blank')); d.game!.name = 'Practice: Sound'; return d }
const NONE: ReadonlySet<SignalName> = new Set()
const step = (id: string) => sound.steps.find((s) => s.id === id)!
const done = (id: string, doc: ProjectDoc, signals: SignalName[] = []) => step(id).done(doc, 'game', new Set(signals))

describe('lesson data', () => {
  it('is registered, with unique step ids and a template that exists', () => {
    expect(LESSONS).toContain(sound)
    expect(new Set(sound.steps.map((s) => s.id)).size).toBe(sound.steps.length)
    expect(sound.steps.length).toBeGreaterThanOrEqual(5)
    expect(sound.steps.length).toBeLessThanOrEqual(10)
    expect(sound.template).toBe('blank')
    expect(sound.playable).toBe(true)
    for (const s of sound.steps) { expect(s.title.length).toBeGreaterThan(0); expect(s.text.length).toBeGreaterThan(0) }
  })
  it('uses no programmer words in the texts, and never calls the style a "look"', () => {
    const all = [sound.title, sound.description, ...sound.summary, ...sound.steps.flatMap((s) => [s.title, s.text])].join(' ')
    expect(all).not.toMatch(/\b(tile|json|id|expression|formula|lua|script|look|looks)\b/i)
  })
  it('starts from the blank template as the steps assume: the starter music and sounds, nothing map- or weapon-specific', () => {
    const d = fresh()
    expect(d.audio!.use!.sounds).toEqual(START_SOUNDS)
    for (const [id, t] of Object.entries(d.audio!.music!)) expect(t.volume ?? 100, id).toBe(START_VOLUME[id])
    expect(Object.keys(d.audio!.music!).sort()).toEqual(Object.keys(START_VOLUME).sort())
    expect(d.maps.start!.music).toBeUndefined()
    expect(d.weapons!.every((w) => w.sounds === undefined)).toBe(true)
  })
  it('has no step that is already true at the start', () => {
    for (const s of sound.steps) expect(s.done(fresh(), 'game', NONE), s.id).toBe(false)
  })
})

describe('step: listen (editor signal)', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('listen', fresh(), ['saved', 'played'])).toBe(false)
    expect(done('listen', fresh(), ['soundPreviewed'])).toBe(true)
  })
})

describe('step: volume', () => {
  it('needs a track quieter than it began; louder or the same does not count', () => {
    const d = fresh()
    d.audio!.music!.title.volume = 100
    expect(done('volume', d)).toBe(false)
    d.audio!.music!.battle.volume = 90 // quieter than full, but louder than the template's 80
    expect(done('volume', d)).toBe(false)
    d.audio!.music!.battle.volume = 50
    expect(done('volume', d)).toBe(true)
  })
  it('turning another track down counts, and settles before ticking', () => {
    const d = fresh()
    d.audio!.music!.enemy.volume = 40
    expect(done('volume', d)).toBe(true)
    expect(step('volume').settleMs).toBeGreaterThan(0)
  })
})

describe('step: map-music', () => {
  it('needs a track of the game on the map; "no music" or a missing track does not count', () => {
    const d = fresh()
    d.maps.start!.music = ''
    expect(done('map-music', d)).toBe(false)
    d.maps.start!.music = 'ghost'
    expect(done('map-music', d)).toBe(false)
    d.maps.start!.music = 'enemy'
    expect(done('map-music', d)).toBe(true)
  })
  it('a turn music alone is not the map\'s music', () => {
    const d = fresh()
    d.maps.start!.enemyTurnMusic = 'enemy'
    expect(done('map-music', d)).toBe(false)
  })
  it('opens the first map', () => {
    expect(step('map-music').where!.item!(fresh())).toBe('start')
  })
})

describe('step: event-sound', () => {
  it('needs an event that plays another sound than before', () => {
    const d = fresh()
    d.audio!.use!.sounds!.hit = 'hit' // chosen again, the same
    expect(done('event-sound', d)).toBe(false)
    d.audio!.use!.sounds!.hit = ''
    expect(done('event-sound', d)).toBe(false) // silence is not a replacement
    delete d.audio!.use!.sounds!.hit
    expect(done('event-sound', d)).toBe(false)
    d.audio!.use!.sounds!.hit = 'ghost'
    expect(done('event-sound', d)).toBe(false) // not a sound of the game
    d.audio!.use!.sounds!.hit = 'critical'
    expect(done('event-sound', d)).toBe(true)
  })
  it('any event counts, not only the hit', () => {
    const d = fresh()
    d.audio!.use!.sounds!.cursor = 'confirm'
    expect(done('event-sound', d)).toBe(true)
  })
})

describe('step: weapon-sound', () => {
  it('needs a weapon with a sound of the game of its own', () => {
    const d = fresh()
    d.weapons![0].sounds = {}
    expect(done('weapon-sound', d)).toBe(false)
    d.weapons![0].sounds = { hit: '' }
    expect(done('weapon-sound', d)).toBe(false)
    d.weapons![0].sounds = { hit: 'ghost' }
    expect(done('weapon-sound', d)).toBe(false)
    d.weapons![0].sounds = { hit: 'critical' }
    expect(done('weapon-sound', d)).toBe(true)
  })
  it('opens the sword', () => {
    expect(step('weapon-sound').where!.item!(fresh())).toBe('sword')
  })
})

describe('step: play (editor signal)', () => {
  it('follows its own signal and nothing else', () => {
    expect(done('play', fresh(), ['soundPreviewed', 'saved'])).toBe(false)
    expect(done('play', fresh(), ['played'])).toBe(true)
  })
})
