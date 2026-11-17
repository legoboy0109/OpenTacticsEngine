// The "take an item away" command in the editor's model: default, summary, checks, and how item / unit renames and deletes
// reach it, in map events and in campaign scenes.
import { describe, expect, it } from 'vitest'
import { campaignChecks } from '../../src/model/campaignChecks'
import { reconcileCampaign } from '../../src/model/campaignRefs'
import { addItem, deleteItem } from '../../src/model/items'
import { itemUsageLines } from '../../src/model/itemRefs'
import { newCommand } from '../../src/model/storyDefaults'
import { renameUnitRefs, scrubMap } from '../../src/model/storyRefs'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText } from '../../src/model/storySummary'
import { commandKind, type Command } from '../../src/model/storyTypes'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const mapId = (d: ProjectDoc) => d.game!.maps[0].id

function withEvent(commands: Command[]): ProjectDoc {
  let d = addItem(loadDemo('demo-economy'), 'Potion', 'potion')
  const m = d.maps[mapId(d)]!
  const next: MapFile = {
    ...m,
    units: m.units!.map((u, i) => (i === 0 ? { ...u, id: 'gorm' } : u)),
    events: [{ id: 'take', when: { battle: 'start' }, do: commands }],
  }
  d = { ...d, maps: { ...d.maps, [mapId(d)]: next } }
  return d
}
const cmds = (d: ProjectDoc): Command[] => d.maps[mapId(d)]!.events![0].do
const problems = (d: ProjectDoc) => storyChecks(d, mapId(d)).map((c) => c.text).join(' | ')

describe('takeItem in a map event', () => {
  it('is its own kind, starts on the first item and the first unit, and reads as a sentence', () => {
    const d = withEvent([])
    const c = newCommand({ doc: d, map: d.maps[mapId(d)]! }, 'takeItem')
    expect(c).toEqual({ takeItem: 'potion', from: { unit: 'gorm' } })
    expect(commandKind(c)).toBe('takeItem')
    const s = { doc: d, map: d.maps[mapId(d)]! }
    expect(commandText(s, { takeItem: 'potion', from: { side: d.maps[mapId(d)]!.sides![0].id }, all: true })).toMatch(/^Takes every “Potion” from any unit of side/)
    expect(commandText(s, { takeItem: 'potion', from: 'gorm' })).toMatch(/^Takes one “Potion” from /)
  })
  it('mirrors the loader: an item of the game, a unit of the map', () => {
    expect(problems(withEvent([{ takeItem: 'potion', from: { unit: 'gorm' }, all: true }]))).toBe('')
    expect(problems(withEvent([{ takeItem: 'nothing', from: {} }]))).toContain('“nothing” is not an item')
    expect(problems(withEvent([{ takeItem: '', from: {} }]))).toContain('pick an item')
    expect(problems(withEvent([{ takeItem: 'potion', from: { unit: 'nobody' } }]))).toContain('“nobody” is not a named unit')
  })
  it('follows a unit rename; a deleted unit or item takes the command with it and the delete says so', () => {
    const d = withEvent([{ takeItem: 'potion', from: { unit: 'gorm' } }, { setSwitch: 'x' } as Command])
    const m = d.maps[mapId(d)]!
    expect(renameUnitRefs(m, 'gorm', 'gormund').events![0].do[0]).toEqual({ takeItem: 'potion', from: { unit: 'gormund' } })
    expect(scrubMap(d, m, [{ kind: 'unit', id: 'gorm' }]).events![0].do).toEqual([{ setSwitch: 'x' }])
    expect(itemUsageLines(d, 'potion').map((u) => u.text).join(' ')).toContain('removed')
    expect(cmds(deleteItem(d, 'potion'))).toEqual([{ setSwitch: 'x' }])
  })
})

describe('takeItem in a campaign scene', () => {
  const scenes = (commands: Command[]): ProjectDoc => {
    const d = addItem(loadDemo('demo-srpg'), 'Potion', 'potion')
    const ch = d.campaign!.chapters[0]
    return { ...d, campaign: { ...d.campaign!, chapters: [{ ...ch, after: commands }, ...d.campaign!.chapters.slice(1)] } }
  }
  const unitOf = (d: ProjectDoc) => d.maps[d.game!.maps[0].id]!.units!.find((u) => u.id)!.id!
  it('takes a roster id, is accepted by the campaign checks, and is cleaned with the item', () => {
    const d0 = scenes([])
    const u = unitOf(d0)
    const d = scenes([{ takeItem: 'potion', from: u, all: true }, { setSwitch: 'x' } as Command])
    expect(campaignChecks(d).filter((c) => c.level === 'error').map((c) => c.text).join(' | ')).not.toContain('cannot use this command')
    expect(campaignChecks(scenes([{ takeItem: 'potion', from: '' }])).map((c) => c.text).join(' | ')).toContain('pick a unit')
    expect(campaignChecks(scenes([{ takeItem: 'gone', from: u }])).map((c) => c.text).join(' | ')).toContain('not an item')
    const after = reconcileCampaign(d, deleteItem(d, 'potion'))
    expect(after.campaign!.chapters[0].after).toEqual([{ setSwitch: 'x' }])
  })
})
