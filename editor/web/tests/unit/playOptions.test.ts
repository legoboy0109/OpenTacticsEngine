// The Play panel's arguments and remembered choices (src/play/playOptions.ts): the address of the embedded player, "this map alone" /
// "from this chapter", the test level (off / automatic / a number), and what storage may hand back.
import { describe, expect, it } from 'vitest'
import { addChapter, createCampaign, setChapterMap } from '../../src/model/campaign'
import {
  chapterChoice, effectiveMode, firstChapterOfMap, parseLevelNumber, playerUrl, PLAY_CAMPAIGN, readTestLevel, readTestLevels, testLevelArg,
} from '../../src/play/playOptions'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const bare = (): ProjectDoc => ({ ...loadDemo('demo-srpg'), campaign: null })
/** A campaign with two chapters, both on the demo's first map ("if several chapters use it, the first"). */
function withCampaign(): ProjectDoc {
  let d = createCampaign(bare())
  d = addChapter(d, 'Chapter 2')
  const ids = d.campaign!.chapters.map((c) => c.id)
  const maps = d.game!.maps.map((m) => m.id)
  d = setChapterMap(d, ids[0]!, maps[0]!)
  d = setChapterMap(d, ids[1]!, maps[0]!)
  return d
}

describe('the address of the embedded player', () => {
  it('is the plain address with nothing asked for (what the Play tests have always seen)', () => {
    expect(playerUrl('g', 'ford')).toBe('/player/embed.html?project=g&map=ford')
  })
  it('keeps the old keys and adds the test level', () => {
    expect(playerUrl('g', 'ford', { style: 's', showAll: true, muted: true, difficulty: 'hard', testLevel: { kind: 'auto' } }))
      .toBe('/player/embed.html?project=g&map=ford&style=s&showall=1&noaudio=1&difficulty=hard&testlevel=auto')
    expect(playerUrl('g', 'ford', { testLevel: { kind: 'number', n: 7 } })).toBe('/player/embed.html?project=g&map=ford&testlevel=7')
    expect(playerUrl('g', 'ford', { testLevel: { kind: 'off' } })).toBe('/player/embed.html?project=g&map=ford')
  })
  it('starts the story at a chapter: campaign=1 and chapter, no map', () => {
    expect(playerUrl('g', 'ford', { chapter: 'ch2', testLevel: { kind: 'auto' } })).toBe('/player/embed.html?project=g&campaign=1&chapter=ch2&testlevel=auto')
    expect(playerUrl('g', PLAY_CAMPAIGN)).toBe('/player/embed.html?project=g&campaign=1')
    expect(playerUrl('g', PLAY_CAMPAIGN, { testLevel: { kind: 'number', n: 3 } })).toBe('/player/embed.html?project=g&campaign=1&testlevel=3')
  })
  it('escapes ids', () => {
    expect(playerUrl('a b', 'm&n', { chapter: 'c#1' })).toBe('/player/embed.html?project=a%20b&campaign=1&chapter=c%231')
  })
})

describe('this map alone / from this chapter', () => {
  it('offers the chapter only for a chapter\'s map, the first chapter that uses it', () => {
    const d = withCampaign()
    const map = d.campaign!.chapters[0]!.map
    const first = d.campaign!.chapters[0]!.id
    expect(firstChapterOfMap(d, map)?.id).toBe(first)
    const c = chapterChoice(d, map)
    expect(c.ok && c.chapter).toBe(first)
  })
  it('says why not: no campaign, not a chapter\'s map, the story picker', () => {
    expect(chapterChoice(bare(), 'ford')).toEqual({ ok: false, reason: 'noCampaign' })
    const d = withCampaign()
    const other = d.game!.maps.map((m) => m.id).find((m) => m !== d.campaign!.chapters[0]!.map)
    if (other) expect(chapterChoice(d, other)).toEqual({ ok: false, reason: 'notChapter' })
    expect(chapterChoice(d, PLAY_CAMPAIGN)).toEqual({ ok: false, reason: 'story' })
  })
  it('falls back to the map alone when the remembered choice is not possible here', () => {
    const d = withCampaign()
    const ok = chapterChoice(d, d.campaign!.chapters[0]!.map)
    expect(effectiveMode('chapter', ok)).toBe('chapter')
    expect(effectiveMode('map', ok)).toBe('map')
    expect(effectiveMode('chapter', { ok: false, reason: 'notChapter' })).toBe('map')
  })
})

describe('the test level and what is remembered', () => {
  it('turns into the player argument', () => {
    expect(testLevelArg({ kind: 'off' })).toBeUndefined()
    expect(testLevelArg({ kind: 'auto' })).toBe('auto')
    expect(testLevelArg({ kind: 'number', n: 12 })).toBe('12')
  })
  it('reads whatever storage holds as a valid choice, else off', () => {
    expect(readTestLevel({ kind: 'auto' })).toEqual({ kind: 'auto' })
    expect(readTestLevel({ kind: 'number', n: 7 })).toEqual({ kind: 'number', n: 7 })
    for (const bad of [null, 5, 'auto', {}, { kind: 'number' }, { kind: 'number', n: 0 }, { kind: 'number', n: 100 }, { kind: 'number', n: 2.5 }, { kind: 'number', n: '4' }, { kind: 'x' }])
      expect(readTestLevel(bad)).toEqual({ kind: 'off' })
  })
  it('keeps one level per map and drops the broken ones', () => {
    expect(readTestLevels({ ford: { kind: 'auto' }, keep: { kind: 'number', n: 9 }, bad: { kind: 'number', n: -1 }, off: { kind: 'off' } }))
      .toEqual({ ford: { kind: 'auto' }, keep: { kind: 'number', n: 9 } })
    expect(readTestLevels(null)).toEqual({})
    expect(readTestLevels([1, 2])).toEqual({})
  })
  it('accepts a number box value from 1 to 99 only', () => {
    expect(parseLevelNumber('7')).toBe(7)
    expect(parseLevelNumber(' 12 ')).toBe(12)
    for (const bad of ['', '0', '100', '-3', '2.5', 'abc', '1e1']) expect(parseLevelNumber(bad)).toBeNull()
  })
})
