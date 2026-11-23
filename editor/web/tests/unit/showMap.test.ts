// "Show another map" (docs/EVENTS.md "Showing another map") and cut-scene maps in the editor's model: defaults, sentences, the checks that mirror the
// loader (loader_story_show_map.cpp), the rename walk leaving a section alone, and where cut-scene maps are left out.
import { describe, expect, it } from 'vitest'
import { freeBattleChecks } from '../../src/model/campaignFree'
import { campaignChecks } from '../../src/model/campaignChecks'
import { cutSceneChecks, isCutScene, playableMaps, setCutScene } from '../../src/model/cutScene'
import { mapWarnings } from '../../src/model/mapChecks'
import { namedUnits } from '../../src/model/storyIds'
import { storyChecks } from '../../src/model/storyChecks'
import { newCommand } from '../../src/model/storyDefaults'
import { sectionsOf } from '../../src/model/storyShowMap'
import { shownScope } from '../../src/model/storyShownScope'
import { commandText } from '../../src/model/storySummary'
import { COMMAND_KINDS, SCENE_FX_KINDS, commandKind } from '../../src/model/storyTypes'
import { mapCommands, DROP } from '../../src/model/storyWalk'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

const base = loadDemo('demo-economy')
const own = base.game!.maps[0].id
const MAPS = (d: ProjectDoc) => d.game!.maps.map((m) => m.id)

/** The demo with a second map whose units are named "ghost" (and "echo"), and the first map's events set. */
function setup(events: unknown[], cut = true): { doc: ProjectDoc; other: string } {
  let d = base
  const other = MAPS(d).find((id) => id !== own) ?? 'flash'
  if (!d.maps[other]) {
    const src = d.maps[own]!
    d = { ...d, game: { ...d.game!, maps: [...d.game!.maps, { id: other, name: 'Flash', file: `maps/${other}.json` }] }, maps: { ...d.maps, [other]: { ...src, events: undefined, objectives: undefined, prep: undefined } as MapFile } }
  }
  const o = d.maps[other]!
  const units = (o.units ?? []).map((u, i) => (i === 0 ? { ...u, id: 'ghost' } : i === 1 ? { ...u, id: 'echo' } : { ...u, id: undefined }))
  const m = d.maps[own]!
  const mine = (m.units ?? []).map((u, i) => (i === 0 ? { ...u, id: 'gorm' } : { ...u, id: undefined }))
  d = { ...d, maps: { ...d.maps, [other]: { ...o, units, events: undefined, objectives: undefined, prep: undefined } as MapFile, [own]: { ...m, units: mine, events: events as MapFile['events'] } } }
  if (cut) d = setCutScene(d, other, true)
  return { doc: d, other }
}
const run = (events: (other: string) => unknown[]) => {
  const { doc, other } = setup([])
  const d = { ...doc, maps: { ...doc.maps, [own]: { ...doc.maps[own]!, events: events(other) as MapFile['events'] } } }
  return storyChecks(d, own).map((c) => c.text).join(' | ')
}
const ev = (...cmds: unknown[]) => [{ id: 'e', when: { battle: 'start' }, do: cmds }]

describe('the two commands', () => {
  it('are command kinds before focus, and the scene kinds still end the list', () => {
    expect(COMMAND_KINDS.slice(-8)).toEqual(SCENE_FX_KINDS)
    expect(commandKind({ showMap: 'x', focus: { x: 1, y: 1 } })).toBe('showMap')
    expect(commandKind({ returnMap: true })).toBe('returnMap')
    expect(commandKind({ focus: { x: 1, y: 1 } })).toBe('focus')
  })
  it('start on the first other map (a cut-scene map first) and read as sentences', () => {
    const { doc, other } = setup([])
    const ctx = { doc, map: doc.maps[own]! }
    const c = newCommand(ctx, 'showMap')
    expect(c).toEqual({ showMap: other })
    expect(newCommand(ctx, 'returnMap')).toEqual({ returnMap: true })
    expect(commandText(ctx, c)).toContain('Shows the map')
    expect(commandText(ctx, { showMap: other, focus: { x: 2, y: 3 } })).toContain('3, 4')
    expect(commandText(ctx, { returnMap: true })).toContain('back')
  })
})

describe('the checks', () => {
  it('accept a plain scene on a shown map', () => {
    expect(run((o) => ev({ showMap: o, focus: 'ghost' }, { say: 'Long ago', speaker: 'ghost' }, { moveUnit: 'echo', x: 0, y: 0 }, { returnMap: true }))).toBe('')
    expect(run((o) => ev({ showMap: o, focus: { x: 0, y: 0 } }, { say: 'x' }))).toBe('')  // the end of the event returns
  })
  it('refuse showMap inside an if, a second showMap, an unknown or the own map, a stray returnMap', () => {
    expect(run((o) => ev({ if: [{ switch: 'x' }], then: [{ showMap: o }] }))).toContain('goes in the event’s own list')
    expect(run((o) => ev({ showMap: o }, { showMap: o }))).toContain('is shown already')
    expect(run(() => ev({ showMap: 'nope' }))).toContain('is not a map of this game')
    expect(run(() => ev({ showMap: own }))).toContain('event’s own map')
    expect(run(() => ev({ returnMap: true }))).toContain('nothing to come back from')
    expect(run((o) => ev({ showMap: o }, { if: [{ switch: 'x' }], then: [{ returnMap: true }] }))).toContain('goes in the event’s own list')
    expect(run((o) => ev({ showMap: o }, { returnMap: false }))).toContain('must be switched on')
  })
  it('check the camera start against the shown map', () => {
    expect(run((o) => ev({ showMap: o, focus: 'gorm' }))).toContain('not a unit of map')
    expect(run((o) => ev({ showMap: o, focus: { x: 999, y: 0 } }))).toContain('is outside map')
  })
  it('read unit ids against the shown map in a section and the battle map outside it', () => {
    expect(run((o) => ev({ showMap: o }, { moveUnit: 'gorm', x: 0, y: 0 }))).toContain('“gorm” is not a named unit')
    expect(run((o) => ev({ showMap: o }, { moveUnit: 'ghost', x: 0, y: 0 }, { returnMap: true }, { moveUnit: 'ghost', x: 0, y: 0 }))).toContain('“ghost” is not a named unit')
    expect(run((o) => ev({ showMap: o }, { addUnit: { id: 'newbie', type: base.units![0].id, side: base.maps[own]!.sides![0].id, x: 0, y: 0 } }, { moveUnit: 'newbie', x: 1, y: 1 }))).not.toContain('newbie')
    // a squares outside the SHOWN map
    expect(run((o) => ev({ showMap: o }, { moveUnit: 'ghost', x: 9999, y: 0 }))).toContain('outside the map')
  })
  it('read an if condition and a variable that reads a unit against the battle map', () => {
    expect(run((o) => ev({ showMap: o }, { if: [{ alive: 'gorm' }], then: [{ say: 'x' }] }))).not.toContain('gorm')
    expect(run((o) => ev({ showMap: o }, { if: [{ alive: 'ghost' }], then: [{ say: 'x' }] }))).toContain('“ghost” is not a named unit')
  })
  it('refuse what acts on the battle itself in a section, and the actor', () => {
    for (const c of [{ win: 'x' }, { lose: 'x' }, { endTurn: true }, { objective: {} }, { setFog: true }, { removeUnit: 'ghost', hide: true }])
      expect(run((o) => ev({ showMap: o }, c)), JSON.stringify(c)).toMatch(/acts on the battle itself|Only hide it/)
    expect(run((o) => [{ id: 'e', when: { reach: { x: 0, y: 0, w: 1, h: 1 } }, do: [{ showMap: o }, { removeUnit: { actor: true } }] }])).toContain('set the event off')
    expect(run(() => ev({ win: 'x' }))).not.toContain('acts on the battle itself')
  })
  it('allow a staged fight in a section of a beforePrep event only', () => {
    const fightCmd = { fight: { attacker: 'ghost', defender: 'echo' } }
    const prep = (o: string) => [{ id: 'e', when: { battle: 'beforePrep' }, do: [{ showMap: o }, fightCmd, { returnMap: true }] }]
    expect(run(prep)).not.toContain('makes no sense')
    expect(run((o) => [{ id: 'e', when: { battle: 'beforePrep' }, do: [{ showMap: o }, { returnMap: true }, { fight: { attacker: 'gorm', defender: 'gorm' } }] }])).toContain('makes no sense')
  })
  it('do not count the section’s added units as units of the event’s own map', () => {
    const first = setup([])
    const doc = { ...first.doc, maps: { ...first.doc.maps, [own]: { ...first.doc.maps[own]!, events: ev({ showMap: first.other }, { addUnit: { id: 'newbie', type: 'x', side: 's', x: 0, y: 0 } }) as MapFile['events'] } } }
    expect(namedUnits(doc, doc.maps[own]!).map((u) => u.id)).not.toContain('newbie')
    const sec = sectionsOf(doc, doc.maps[own]!.events![0].do)
    expect(sec.list[0].unitIds).toContain('newbie')
    expect(shownScope(doc, sec.list[0])!.units.map((u) => u.id)).toContain('newbie')
  })
})

describe('the walk', () => {
  it('leaves a section’s unit ids alone when the battle map renames or loses a unit', () => {
    const list = [{ showMap: 'other' }, { moveUnit: 'gorm', x: 0, y: 0 }, { returnMap: true }, { moveUnit: 'gorm', x: 0, y: 0 }] as never[]
    const seen: string[] = []
    const out = mapCommands(list, { ref: (r) => { seen.push(`${r.kind}:${r.id}`); return r.kind === 'unit' && r.id === 'gorm' ? DROP : r.id } }, 'event “e”')
    expect(out.map((c) => commandKind(c))).toEqual(['showMap', 'moveUnit', 'returnMap'])  // only the one after the return is dropped
  })
})

describe('cut-scene maps', () => {
  it('are set and cleared in the map list, the key only written when on', () => {
    const { doc, other } = setup([], false)
    expect(isCutScene(doc, other)).toBe(false)
    const on = setCutScene(doc, other, true)
    expect(on.game!.maps.find((m) => m.id === other)!.cutScene).toBe(true)
    expect('cutScene' in setCutScene(on, other, false).game!.maps.find((m) => m.id === other)!).toBe(false)
    expect(playableMaps(on.game!.maps).map((m) => m.id)).not.toContain(other)
  })
  it('may not have events, objectives or a preparation', () => {
    const { doc, other } = setup([])
    expect(cutSceneChecks(doc, other)).toEqual([])
    const bad = { ...doc, maps: { ...doc.maps, [other]: { ...doc.maps[other]!, events: [], objectives: { win: [] } } as MapFile } }
    expect(cutSceneChecks(bad, other)).toHaveLength(2)
    expect(mapWarnings(bad, bad.maps[other]!).map((c) => c.text).join(' ')).toContain('only for story scenes')
    expect(cutSceneChecks(setCutScene(bad, other, false), other)).toEqual([])
  })
  it('cannot be a chapter map or a free battle', () => {
    const { doc, other } = setup([])
    const withCampaign = { ...doc, campaign: { chapters: [{ id: 'c1', name: 'One', map: other }], freeBattles: [other] } } as ProjectDoc
    const text = campaignChecks(withCampaign).map((c) => c.text).join(' | ')
    expect(text).toContain('only story scenes show')
    expect(freeBattleChecks(withCampaign.campaign!)).toEqual([])
  })
})
