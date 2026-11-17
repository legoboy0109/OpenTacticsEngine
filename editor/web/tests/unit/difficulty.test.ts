// Difficulties in the editor's model (docs/DIFFICULTY.md): the list and its edits, defaults never written, a map's own numbers (they
// replace), who a placement is for, the story condition and addUnit, delete follow-through into maps, placements, events and scenes,
// and the checks that mirror the loader.
import { describe, expect, it } from 'vitest'
import {
  addDifficulty, clearMapOverride, defaultDifficulty, difficultyList, hasDifficulties, moveDifficulty, removeAllDifficulties, removeDifficulty,
  renameDifficulty, setChangeRule, setDefaultDifficulty, setDifficultyNumber, setDifficultyStat, setDifficultyText, setMapOverrideNumber,
  setMapOverrideStat, setPercentStats, startDifficulties, toggleOnlyOn, changeRule,
} from '../../src/model/difficulty'
import { gameDifficultyChecks, mapDifficultyChecks } from '../../src/model/difficultyChecks'
import { difficultyUsages } from '../../src/model/difficultyRefs'
import { mapWarnings } from '../../src/model/mapChecks'
import { reconcileCampaign } from '../../src/model/campaignRefs'
import { storyChecks } from '../../src/model/storyChecks'
import { newCondition } from '../../src/model/storyDefaults'
import { conditionText } from '../../src/model/storySummary'
import { conditionKind, CONDITION_KINDS, COMMAND_KINDS, type Command, type Condition } from '../../src/model/storyTypes'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const first = (d: ProjectDoc): string => d.game!.maps[0].id
const withMap = (d: ProjectDoc, fn: (m: MapFile) => MapFile): ProjectDoc => ({ ...d, maps: { ...d.maps, [first(d)]: fn(d.maps[first(d)]!) } })

/** A game with Easy / Normal / Hard whose first map has an event reading the difficulty and a hard-only added unit, and a hard-only placed unit. */
function setup(): ProjectDoc {
  let d = startDifficulties(loadDemo('demo-srpg'), true)
  const side = (d.maps[first(d)]!.sides ?? []).find((s) => s.controller === 'ai')!.id
  d = withMap(d, (m) => ({
    ...m,
    units: [...(m.units ?? []), { type: d.units![0].id, side, x: 0, y: 0, name: 'Hard guard', difficulty: ['hard'] }, { type: d.units![0].id, side, x: 1, y: 0, name: 'Pair guard', difficulty: ['normal', 'hard'] }],
    difficulty: { hard: { enemy: { levels: 3 } }, easy: { xpPercent: 150 } },
    events: [
      ...(m.events ?? []),
      // "Only if" names the difficulty: deleting it removes the whole event (it could not hold any more)
      { id: 'harder', when: { battle: 'start' }, if: [{ difficulty: 'hard' } as Condition], do: [{ say: 'Brace yourselves.' } as Command] },
      { id: 'reinforce', when: { battle: 'start' }, do: [
        { addUnit: { type: d.units![0].id, side, x: 2, y: 0, difficulty: ['hard'] } } as Command,
        { addUnit: { type: d.units![0].id, side, x: 3, y: 0, difficulty: ['easy', 'hard'] } } as Command,
        { if: [{ difficulty: 'hard' } as Condition, { difficulty: { atLeast: 'normal' } } as Condition], then: [{ say: 'Hard.' } as Command] } as Command,
      ] },
    ],
  }))
  return d
}

describe('the list', () => {
  it('starts with three or one, and a game without the key has none', () => {
    const plain = loadDemo('demo-srpg')
    expect(hasDifficulties(plain)).toBe(false)
    expect('difficulties' in plain.game!).toBe(false)
    const three = startDifficulties(plain, true)
    expect(difficultyList(three).map((d) => d.id)).toEqual(['easy', 'normal', 'hard'])
    expect(defaultDifficulty(three)).toBe('normal')
    expect(difficultyList(startDifficulties(plain, false)).map((d) => d.id)).toEqual(['normal'])
    expect(startDifficulties(three, false)).toBe(three)  // already there
  })
  it('adds, renames (the id stays), reorders and chooses the default', () => {
    let d = startDifficulties(loadDemo('demo-srpg'), true)
    d = addDifficulty(d, 'Nightmare')
    expect(difficultyList(d).map((x) => x.id)).toEqual(['easy', 'normal', 'hard', 'nightmare'])
    d = renameDifficulty(d, 'hard', 'Veteran')
    expect(difficultyList(d)[2]).toMatchObject({ id: 'hard', name: 'Veteran' })
    d = moveDifficulty(d, 3, 0)
    expect(difficultyList(d).map((x) => x.id)).toEqual(['nightmare', 'easy', 'normal', 'hard'])
    d = setDefaultDifficulty(d, 'easy')
    expect(defaultDifficulty(d)).toBe('easy')
    expect(defaultDifficulty(setDefaultDifficulty(d, 'nope'))).toBe('easy')  // not in the list: nothing changes
    expect(difficultyList(addDifficulty(d, 'Nightmare')).map((x) => x.id)).toContain('nightmare_2')  // another with the same name gets its own id
  })
  it('never writes a default: change lowerOnly, zero levels, 100 percent, empty enemy blocks', () => {
    let d = startDifficulties(loadDemo('demo-srpg'), true)
    expect(changeRule(d)).toBe('lowerOnly')
    d = setChangeRule(d, 'free')
    expect(d.game!.difficulties!.change).toBe('free')
    d = setChangeRule(d, 'lowerOnly')
    expect('change' in d.game!.difficulties!).toBe(false)
    d = setDifficultyNumber(d, 'normal', 'levels', 2)
    d = setDifficultyNumber(d, 'normal', 'levels', 0)
    d = setDifficultyNumber(d, 'normal', 'statsPercent', 100)
    d = setDifficultyNumber(d, 'normal', 'xpPercent', 100)
    d = setDifficultyStat(d, 'normal', 'str', 0)
    expect(difficultyList(d).find((x) => x.id === 'normal')).toEqual({ id: 'normal', name: 'Normal', description: 'The game as its maker meant it.' })
    d = setDifficultyNumber(d, 'normal', 'xpPercent', 80)
    d = setDifficultyStat(d, 'normal', 'str', 2)
    d = setDifficultyText(d, 'normal', 'description', '   ')
    expect(difficultyList(d).find((x) => x.id === 'normal')).toEqual({ id: 'normal', name: 'Normal', xpPercent: 80, enemy: { stats: { str: 2 } } })
  })
  it('keeps a stats list only when it differs from "every stat but movement"', () => {
    let d = startDifficulties(loadDemo('demo-srpg'), true)
    d = setPercentStats(d, ['hp'])
    expect(d.game!.difficulties!.percentStats).toEqual(['hp'])
    d = setPercentStats(d, undefined)
    expect('percentStats' in d.game!.difficulties!).toBe(false)
  })
})

describe('a map\'s own numbers replace the game\'s', () => {
  it('keeps 0 and 100 (they are a real choice on a map) and removes an emptied block', () => {
    const d = loadDemo('demo-srpg')
    let m = d.maps[first(d)]!
    m = setMapOverrideNumber(m, 'hard', 'levels', 3)
    m = setMapOverrideNumber(m, 'hard', 'statsPercent', 100)
    m = setMapOverrideStat(m, 'hard', 'def', 0)
    expect(m.difficulty).toEqual({ hard: { enemy: { levels: 3, statsPercent: 100, stats: { def: 0 } } } })
    m = setMapOverrideNumber(m, 'hard', 'levels', undefined)
    m = setMapOverrideNumber(m, 'hard', 'statsPercent', undefined)
    m = setMapOverrideStat(m, 'hard', 'def', undefined)
    expect('difficulty' in m).toBe(false)
    expect('difficulty' in clearMapOverride(setMapOverrideNumber(m, 'easy', 'xpPercent', 120), 'easy')).toBe(false)
  })
})

describe('who a placement is for', () => {
  const all = ['easy', 'normal', 'hard']
  it('unticking narrows it; every one ticked, or none, is no limit', () => {
    expect(toggleOnlyOn(undefined, all, 'easy', false)).toEqual(['normal', 'hard'])
    expect(toggleOnlyOn(['normal', 'hard'], all, 'easy', true)).toBeUndefined()
    expect(toggleOnlyOn(['hard'], all, 'hard', false)).toBeUndefined()  // the last one cannot be unticked into an empty list
    expect(toggleOnlyOn(['hard'], all, 'easy', true)).toEqual(['easy', 'hard'])
    expect(toggleOnlyOn(['ghost', 'hard'], all, 'normal', true)).toEqual(['normal', 'hard'])  // a vanished id is cleaned by any change
  })
})

describe('the story reads the difficulty', () => {
  it('has a condition kind that is offered and summarised', () => {
    const d = setup()
    expect(CONDITION_KINDS).toContain('difficulty')
    const c = newCondition({ doc: d, map: d.maps[first(d)]!, scene: false } as never, 'difficulty')
    expect(c).toEqual({ difficulty: 'normal' })
    expect(conditionKind({ difficulty: { atLeast: 'normal' } })).toBe('difficulty')
    const ctx = { doc: d, map: d.maps[first(d)]! }
    expect(conditionText(ctx, { difficulty: 'hard' })).toBe('the difficulty is “Hard”')
    expect(conditionText(ctx, { difficulty: { atLeast: 'normal' } })).toBe('the difficulty is at least “Normal”')
    expect(conditionText(ctx, { difficulty: { atMost: 'normal' } })).toBe('the difficulty is at most “Normal”')
  })
  it('keeps the scene presentation kinds last in COMMAND_KINDS', () => {
    expect(COMMAND_KINDS.slice(-8)).toEqual(['background', 'fade', 'wait', 'shake', 'leave', 'clearStage', 'weather', 'narrate'])
  })
  it('checks a condition and an added unit against the list', () => {
    const d = setup()
    expect(storyChecks(d, first(d)).filter((c) => /difficulty/i.test(c.text))).toEqual([])
    const bad = withMap(d, (m) => ({ ...m, events: m.events!.map((e) => (e.id === 'harder' ? { ...e, if: [{ difficulty: 'extreme' } as Condition] } : e)) }))
    expect(storyChecks(bad, first(bad)).map((c) => c.text).join(' ')).toContain('“extreme” is not a difficulty of this game')
    const none = withMap(loadDemo('demo-srpg'), (m) => ({ ...m, events: [{ id: 'x', when: { battle: 'start' }, if: [{ difficulty: 'hard' } as Condition], do: [] }] }))
    expect(storyChecks(none, first(none)).map((c) => c.text).join(' ')).toContain('the game has no difficulties yet')
  })
})

describe('deleting a difficulty', () => {
  it('lists what it takes with it, and the clean-up happens in the same edit', () => {
    const d = setup()
    const lines = difficultyUsages(d, 'hard').map((u) => u.text)
    expect(lines.some((t) => t.includes('its own numbers for “Hard” are removed'))).toBe(true)
    expect(lines.some((t) => t.includes('Hard guard') && t.includes('only on “Hard” and now appears on every difficulty'))).toBe(true)
    expect(lines.some((t) => t.includes('Pair guard') && t.includes('no longer limited to “Hard”'))).toBe(true)
    expect(lines.some((t) => t.includes('event “harder”') && t.includes('is removed'))).toBe(true)
    expect(lines.some((t) => t.includes('loses this difficulty'))).toBe(true)
    const out = removeDifficulty(d, 'hard')
    expect(difficultyList(out).map((x) => x.id)).toEqual(['easy', 'normal'])
    const m = out.maps[first(out)]!
    expect(m.difficulty).toEqual({ easy: { xpPercent: 150 } })
    expect(m.units!.find((u) => u.name === 'Hard guard')).not.toHaveProperty('difficulty')
    expect(m.units!.find((u) => u.name === 'Pair guard')!.difficulty).toEqual(['normal'])
    expect(m.events!.some((e) => e.id === 'harder')).toBe(false)
    const ev = m.events!.find((e) => e.id === 'reinforce')!
    expect(ev.do![0].addUnit).not.toHaveProperty('difficulty')
    expect(ev.do![1].addUnit!.difficulty).toEqual(['easy'])
    expect(ev.do!).toHaveLength(2)  // the "if" command lost its hard condition and so went whole
  })
  it('moves the default and, with the last one, switches difficulties off', () => {
    let d = setDefaultDifficulty(setup(), 'hard')
    d = removeDifficulty(d, 'hard')
    expect(defaultDifficulty(d)).toBe('easy')
    const none = removeAllDifficulties(setup())
    expect(hasDifficulties(none)).toBe(false)
    expect('difficulties' in none.game!).toBe(false)
    const nm = none.maps[first(none)]!
    expect('difficulty' in nm).toBe(false)
    expect(nm.units!.every((u) => !('difficulty' in u))).toBe(true)
  })
  it('lets the campaign follow: a scene condition on the deleted difficulty goes with the store\'s reconcile', () => {
    const d: ProjectDoc = {
      ...setup(),
      campaign: { name: 'C', chapters: [{ id: 'c1', name: 'One', map: first(setup()), before: [
        { if: [{ difficulty: 'hard' } as Condition], then: [{ say: 'Brace yourselves.' } as Command] } as Command,
        { say: 'Hello.' } as Command,
      ] }] } as never,
    }
    const after = reconcileCampaign(d, removeDifficulty(d, 'hard'))
    const before = (after.campaign!.chapters[0] as { before: Command[] }).before
    expect(before).toEqual([{ say: 'Hello.' }])
  })
})

describe('checks', () => {
  it('has nothing to say about a good list, and says it plainly for a bad one', () => {
    const d = setup()
    expect(gameDifficultyChecks(d)).toEqual([])
    expect(gameDifficultyChecks(startDifficulties(loadDemo('demo-srpg'), false)).map((c) => c.level)).toEqual(['hint'])
    const bad = (fn: (s: NonNullable<ProjectDoc['game']>['difficulties']) => void): string => {
      const copy = JSON.parse(JSON.stringify(d)) as ProjectDoc
      fn(copy.game!.difficulties)
      return gameDifficultyChecks(copy).map((c) => c.text).join(' ')
    }
    expect(bad((s) => { s!.default = 'ghost' })).toContain('default difficulty “ghost” is not in the list')
    expect(bad((s) => { s!.list[0].enemy = { levels: 500 } })).toContain('whole number from -98 to 98')
    expect(bad((s) => { s!.list[0].xpPercent = -5 })).toContain('experience percent must be a whole number from 0 to 1000')
    expect(bad((s) => { s!.list[0].enemy = { stats: { luck: 1 } } })).toContain('“luck” is not a stat of this game')
    expect(bad((s) => { s!.percentStats = ['luck'] })).toContain('“luck”')
    expect(bad((s) => { s!.list[1].id = 'easy' })).toContain('same id')
  })
  it('judges a map\'s numbers and placements, and a deleted difficulty a placement still names', () => {
    const d = setup()
    expect(mapDifficultyChecks(d, first(d), d.maps[first(d)]!)).toEqual([])
    const bad = withMap(d, (m) => ({
      ...m,
      difficulty: { ghost: { enemy: { levels: 1 } }, hard: { xpPercent: 5000 } },
      units: [...(m.units ?? []), { type: d.units![0].id, side: m.sides![1].id, x: 5, y: 5, difficulty: ['ghost'] }, { type: d.units![0].id, side: m.sides![1].id, x: 6, y: 5, difficulty: [] }],
    }))
    const text = mapDifficultyChecks(bad, first(bad), bad.maps[first(bad)]!).map((c) => c.text).join(' | ')
    expect(text).toContain('there is no such difficulty any more')
    expect(text).toContain('the experience percent must be a whole number from 0 to 1000')
    expect(text).toContain('limited to the difficulty “ghost”, which does not exist')
    expect(text).toContain('lists no difficulty')
    const human = withMap(d, (m) => ({ ...m, units: [...(m.units ?? []), { type: d.units![0].id, side: m.sides![0].id, x: 7, y: 7, name: 'Ally', difficulty: ['hard'] }] }))
    expect(mapDifficultyChecks(human, first(human), human.maps[first(human)]!).map((c) => c.text).join(' ')).toContain('is on a human side')
    expect(mapWarnings(bad, bad.maps[first(bad)]!).some((w) => w.key.startsWith('difficulty:'))).toBe(true)
    const none = withMap(loadDemo('demo-srpg'), (m) => ({ ...m, difficulty: { hard: {} } }))
    expect(mapDifficultyChecks(none, first(none), none.maps[first(none)]!)[0].text).toContain('the game has no difficulties')
  })
})
