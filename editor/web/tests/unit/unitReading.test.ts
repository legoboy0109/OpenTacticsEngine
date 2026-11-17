// Variable commands that read a unit's number (docs/EVENTS.md "Reading a unit's numbers"): the model (kinds, summary, rename and
// delete follow-through, "where it is used") and the checks that mirror the loader, in map events and in campaign scenes.
import { describe, expect, it } from 'vitest'
import { campaignChecks } from '../../src/model/campaignChecks'
import { reconcileCampaign } from '../../src/model/campaignRefs'
import { findUsages } from '../../src/model/references'
import { addStoryItem } from '../../src/model/storyLists'
import { renameUnitRefs, scrubMap, storyUsages } from '../../src/model/storyRefs'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText } from '../../src/model/storySummary'
import { numberSource, type Command } from '../../src/model/storyTypes'
import { addStat, removeStat } from '../../src/model/stats'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const mapId = (d: ProjectDoc) => d.game!.maps[0].id

/** The economy demo with two named units, a variable "bar" and a start event holding `commands`. */
function withEvent(commands: Command[]): ProjectDoc {
  let d = addStoryItem(loadDemo('demo-economy'), 'variables', 'Bar')
  const m = d.maps[mapId(d)]!
  const next: MapFile = {
    ...m,
    units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'gorm' } : i === 1 ? { ...u, id: 'lyra' } : u)),
    events: [{ id: 'sum', when: { battle: 'start' }, do: commands }],
  }
  d = { ...d, maps: { ...d.maps, [mapId(d)]: next } }
  return d
}
const cmds = (d: ProjectDoc): Command[] => d.maps[mapId(d)]!.events![0].do
const problems = (d: ProjectDoc) => storyChecks(d, mapId(d)).map((c) => c.text)
const stat = (d: ProjectDoc) => d.game!.stats[0].id

describe('reading a unit in a map event', () => {
  it('knows where the number comes from', () => {
    expect(numberSource({ setVariable: 'bar', to: 3 })).toBe('number')
    expect(numberSource({ setVariable: 'bar', level: 'gorm' })).toBe('level')
    expect(numberSource({ addVariable: 'bar', stat: 'str', of: 'gorm' })).toBe('stat')
  })
  it('has nothing to complain about when well formed, and says it in a sentence', () => {
    const d = withEvent([{ setVariable: 'bar', level: 'gorm' }, { addVariable: 'bar', level: 'lyra' }, { addVariable: 'bar', stat: '', of: 'lyra' }])
    const ok = withEvent([{ setVariable: 'bar', level: 'gorm' }, { addVariable: 'bar', maxHp: 'lyra' }])
    expect(problems(ok)).toEqual([])
    const s = { doc: d, map: d.maps[mapId(d)]! }
    expect(commandText(s, { setVariable: 'bar', level: 'gorm' })).toMatch(/Sets the variable “Bar” to the level of /)
    expect(commandText(s, { addVariable: 'bar', hp: 'lyra' })).toMatch(/Adds the hit points of .* to the variable “Bar”/)
  })
  it('mirrors the loader: not both, one reading, stat needs of, a unit of the map, a stat of the game', () => {
    const bad = (c: Command) => problems(withEvent([c])).join(' | ')
    expect(bad({ setVariable: 'bar', to: 3, level: 'gorm' })).toContain('either a number or a reading')
    expect(bad({ addVariable: 'bar', by: 1, level: 'gorm' })).toContain('either a number or a reading')
    expect(bad({ setVariable: 'bar', level: 'gorm', hp: 'gorm' })).toContain('only one thing')
    expect(bad({ setVariable: 'bar', stat: stat(withEvent([]))! })).toContain('needs the unit')
    expect(bad({ setVariable: 'bar', level: 'gorm', of: 'gorm' })).toContain('stat reading only')
    expect(bad({ setVariable: 'bar', level: '' })).toContain('pick a unit')
    expect(bad({ setVariable: 'bar', level: 'nobody' })).toContain('“nobody” is not a named unit')
    expect(bad({ setVariable: 'bar', stat: 'luck', of: 'gorm' })).toContain('“luck” is not a stat')
    expect(bad({ setVariable: 'bar', stat: '', of: 'gorm' })).toContain('pick a stat')
  })
  it('follows a unit rename and is removed with the unit', () => {
    const d = withEvent([{ setVariable: 'bar', level: 'gorm' }, { addVariable: 'bar', stat: stat(withEvent([])), of: 'gorm' }, { addVariable: 'bar', level: 'lyra' }])
    const m = d.maps[mapId(d)]!
    const renamed = renameUnitRefs(m, 'gorm', 'gormund')
    expect(renamed.events![0].do).toEqual([{ setVariable: 'bar', level: 'gormund' }, { addVariable: 'bar', stat: stat(d), of: 'gormund' }, { addVariable: 'bar', level: 'lyra' }])
    const gone = scrubMap(d, m, [{ kind: 'unit', id: 'gorm' }])
    expect(gone.events![0].do).toEqual([{ addVariable: 'bar', level: 'lyra' }])
    expect(storyUsages(d, [{ kind: 'unit', id: 'gorm' }]).length).toBe(2)
  })
  it('follows a deleted stat: the command that reads it goes, and the delete says so', () => {
    const s = 'luck'
    const d = addStat(withEvent([{ setVariable: 'bar', stat: s, of: 'gorm' }, { setVariable: 'bar', level: 'lyra' }]), 'Luck', s)
    expect(findUsages(d, 'stat', s).map((u) => u.text).join(' ')).toContain('removed')
    expect(cmds(removeStat(d, s))).toEqual([{ setVariable: 'bar', level: 'lyra' }])
  })
})

describe('reading a character in a campaign scene', () => {
  const scenes = (commands: Command[]): ProjectDoc => {
    const d = addStoryItem(loadDemo('demo-srpg'), 'variables', 'Bar')
    const ch = d.campaign!.chapters[0]
    return { ...d, campaign: { ...d.campaign!, chapters: [{ ...ch, after: commands }, ...d.campaign!.chapters.slice(1)] } }
  }
  const texts = (d: ProjectDoc) => campaignChecks(d).map((c) => c.text).join(' | ')
  it('accepts level, most hit points and a stat, and mirrors the loader', () => {
    const d0 = loadDemo('demo-srpg')
    const unit = d0.maps[d0.game!.maps[0].id]!.units!.find((u) => u.id)!.id!
    expect(campaignChecks(scenes([{ setVariable: 'bar', level: unit }, { addVariable: 'bar', maxHp: unit }, { addVariable: 'bar', stat: d0.game!.stats[0].id, of: unit }]))).toEqual([])
    expect(texts(scenes([{ setVariable: 'bar', hp: unit }]))).toContain('cannot read hit points')
    expect(texts(scenes([{ setVariable: 'bar', to: 1, level: unit }]))).toContain('either a number or a reading')
    expect(texts(scenes([{ setVariable: 'bar', stat: 'luck', of: unit }]))).toContain('“luck” is not a stat')
    expect(texts(scenes([{ setVariable: 'bar', level: '' }]))).toContain('pick a unit')
  })
  it('loses the command when the stat it reads is deleted', () => {
    const d0 = loadDemo('demo-srpg')
    const unit = d0.maps[d0.game!.maps[0].id]!.units!.find((u) => u.id)!.id!
    const s = 'luck'
    const d = addStat(scenes([{ setVariable: 'bar', stat: s, of: unit }, { setVariable: 'bar', level: unit }]), 'Luck', s)
    const after = reconcileCampaign(d, removeStat(d, s))
    expect(after.campaign!.chapters[0].after).toEqual([{ setVariable: 'bar', level: unit }])
  })
})
