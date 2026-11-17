// Story scene presentation in the editor's model (docs/EVENTS.md "Scene presentation"): the command kinds and their defaults and
// sentences, the checks that mirror the loader (maps and campaign scenes), the Backgrounds list on the Art screen (add, rename,
// delete, picture) and every command that names a background following a rename or a delete in the same edit.
import { describe, expect, it } from 'vitest'
import { addBackground, backgroundIdProblem, backgroundList, backgroundUsageLines, backgroundsUsingSheet, moveBackground, reconcileBackgrounds, removeBackground, renameBackground, setBackgroundPicture } from '../../src/model/backgrounds'
import { createCampaign, sceneOf, withScene, sceneAsMap } from '../../src/model/campaign'
import { campaignChecks } from '../../src/model/campaignChecks'
import { removeSheet } from '../../src/model/graphics'
import { sceneProblems } from '../../src/model/sceneChecks'
import { newCommand } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { addCommand } from '../../src/model/storyEdit'
import { commandText } from '../../src/model/storySummary'
import { commandKind, COMMAND_KINDS, SCENE_FX_KINDS, type Command, type StoryEvent } from '../../src/model/storyTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

/** The demo with one image and no earlier events. */
function base(): ProjectDoc {
  const d = loadDemo('demo-srpg')
  const maps: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(d.maps)) { const { events: _e, objectives: _o, ...r } = m!; maps[id] = r as typeof m }
  return { ...d, campaign: null, maps, graphics: { sheets: [{ id: 'forest', file: 'art/forest.png', cellWidth: 16, cellHeight: 16 }, { id: 'places', file: 'art/places.png', cellWidth: 32, cellHeight: 32 }] } }
}
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const withEvent = (d: ProjectDoc, commands: Command[]): ProjectDoc => {
  const ev: StoryEvent = { id: 'scene', when: { battle: 'start' }, do: commands }
  return { ...d, maps: { ...d.maps, [mapId(d)]: { ...d.maps[mapId(d)]!, events: [ev] } } }
}
const commandsOf = (d: ProjectDoc): Command[] => d.maps[mapId(d)]!.events![0].do
const checks = (d: ProjectDoc) => storyChecks(d, mapId(d)).map((c) => c.text)
const withBgs = (d: ProjectDoc) => addBackground(addBackground(d, 'forest_dusk'), 'hall')
const ctx = (d: ProjectDoc) => ({ doc: d, map: d.maps[mapId(d)]! })

describe('the command kinds', () => {
  it('lists the eight scene commands after the old ones, so playMusic keeps its fade and focus its wait', () => {
    expect(SCENE_FX_KINDS).toEqual(['background', 'fade', 'wait', 'shake', 'leave', 'clearStage', 'weather', 'narrate'])
    expect(COMMAND_KINDS.slice(-8)).toEqual(SCENE_FX_KINDS)
    expect(commandKind({ playMusic: 'a', fade: 1 })).toBe('playMusic')
    expect(commandKind({ stopMusic: true, fade: 1 })).toBe('stopMusic')
    expect(commandKind({ focus: 'eirik', ms: 300, wait: false })).toBe('focus')
    expect(commandKind({ moveUnit: 'eirik', x: 1, y: 1, walk: true, wait: false })).toBe('moveUnit')
    expect(commandKind({ fade: 'out', ms: 300 })).toBe('fade')
    expect(commandKind({ wait: 500 })).toBe('wait')
    expect(commandKind({ background: null })).toBe('background')
    expect(commandKind({ say: 'x', position: 'left', auto: 400 })).toBe('say')
    expect(commandKind({ narrate: 'x' })).toBe('narrate')
  })

  it('a new command starts valid where it can be', () => {
    const d = withBgs(base())
    const c = ctx(d)
    expect(newCommand(c, 'background')).toEqual({ background: 'forest_dusk' })
    expect(newCommand(c, 'fade')).toEqual({ fade: 'out', ms: 500, color: '#000000' })
    expect(newCommand(c, 'wait')).toEqual({ wait: 500 })
    expect(newCommand(c, 'shake')).toEqual({ shake: { ms: 400, strength: 3 } })
    expect(newCommand(c, 'clearStage')).toEqual({ clearStage: true })
    expect(newCommand(c, 'weather')).toEqual({ weather: 'rain' })
    expect(newCommand(c, 'narrate')).toEqual({ narrate: '' })
    expect(typeof newCommand(c, 'leave').leave).toBe('string')
    for (const k of SCENE_FX_KINDS) expect(commandKind(newCommand(c, k))).toBe(k)
  })

  it('every one has a plain sentence', () => {
    const c = ctx(withBgs(base()))
    expect(commandText(c, { background: 'hall' })).toBe('Shows the background “hall”')
    expect(commandText(c, { background: 'hall', transition: 'fade', ms: 600 })).toBe('Shows the background “hall” with a fade over 600 ms')
    expect(commandText(c, { background: null })).toBe('Takes the background away')
    expect(commandText(c, { fade: 'out', ms: 300, color: '#102030' })).toBe('Fades the screen out to #102030 over 300 ms')
    expect(commandText(c, { fade: 'in', ms: 300 })).toBe('Fades the screen back in over 300 ms')
    expect(commandText(c, { wait: 500 })).toBe('Waits 500 ms')
    expect(commandText(c, { shake: { ms: 400, strength: 3 } })).toBe('Shakes the screen for 400 ms (strength 3)')
    expect(commandText(c, { leave: 'Lyra' })).toBe('“Lyra” leaves the stage')
    expect(commandText(c, { clearStage: true })).toBe('Clears the stage')
    expect(commandText(c, { weather: 'snow' })).toBe('Makes it snow')
    expect(commandText(c, { weather: 'none' })).toBe('Clears the weather')
    expect(commandText(c, { narrate: 'Night falls.' })).toBe('Narrates “Night falls.”')
    expect(commandText(c, { say: 'Hi', position: 'left' })).toBe('Says “Hi” (on the left)')
    expect(commandText(c, { focus: { x: 2, y: 3 }, ms: 0 })).toContain('at once')
    expect(commandText(c, { focus: { x: 2, y: 3 }, ms: 400 })).toContain('over 400 ms')
  })
})

describe('the checks mirror the loader', () => {
  const d = withBgs(base())
  const p = (c: Command, scene = false) => sceneProblems(d, c, scene)
  it('accepts what the loader accepts', () => {
    for (const c of [{ background: 'hall' }, { background: null }, { background: 'hall', transition: 'fade' as const, ms: 0 }, { fade: 'out' as const, ms: 10000, color: '#fff' },
      { fade: 'in' as const }, { wait: 0 }, { wait: 60000 }, { shake: { ms: 400, strength: 32 } }, { shake: {} }, { leave: 'x' }, { clearStage: true as const }, { weather: 'none' as const },
      { narrate: 'x', auto: 1 }, { say: 'x', position: 'center' as const, auto: 60000 }, { focus: 'a', ms: 0, wait: false }, { moveUnit: 'a', x: 1, y: 1, walk: true, wait: true }])
      expect(p(c)).toEqual([])
  })
  it('refuses what the loader refuses', () => {
    expect(p({ background: 'swamp' }).join()).toContain('not one of this game’s')
    expect(p({ background: '' }).join()).toContain('pick a background')
    expect(p({ background: 'hall', transition: 'wipe' as never }).join()).toContain('“cut” or “fade”')
    expect(p({ background: 'hall', ms: 10001 }).join()).toContain('from 0 to 10000')
    expect(p({ background: 'hall', ms: 1.5 }).join()).toContain('whole number')
    expect(p({ fade: 'sideways' as never }).join()).toContain('“out” or “in”')
    expect(p({ fade: 'out', ms: -1 }).join()).toContain('milliseconds')
    expect(p({ fade: 'out', color: 'black' }).join()).toContain('#000000')
    expect(p({ fade: 'out', color: '#12345' }).join()).toContain('#000000')
    expect(p({ wait: -5 }).join()).toContain('0 to 60000')
    expect(p({ wait: 70000 }).join()).toContain('0 to 60000')
    expect(p({ shake: { ms: 99999 } }).join()).toContain('milliseconds')
    expect(p({ shake: { strength: 0 } }).join()).toContain('1 to 32')
    expect(p({ shake: { strength: 99 } }).join()).toContain('1 to 32')
    expect(p({ leave: ' ' }).join()).toContain('name the speaker')
    expect(p({ weather: 'fog' as never }).join()).toContain('rain, snow or none')
    expect(p({ narrate: '' }).join()).toContain('text to say is empty')
    expect(p({ say: 'x', position: 'up' as never }).join()).toContain('left, right or center')
    expect(p({ say: 'x', auto: 0 }).join()).toContain('from 1 to 60000')
    expect(p({ focus: 'a', ms: 20000 }).join()).toContain('from 0 to 10000')
    expect(p({ moveUnit: 'a', x: 1, y: 1, wait: false }).join()).toContain('only means something while the unit walks')
  })
  it('weather is for map events only; a scene left faded out is fine', () => {
    expect(p({ weather: 'rain' }, true).join()).toContain('map events only')
    expect(p({ fade: 'out' }, true)).toEqual([])  // (nothing says anything about a scene ending faded out)
  })
  it('a map event with them is checked where the maker reads it', () => {
    const bad = withEvent(d, [{ background: 'swamp' }, { wait: -1 }, { say: 'ok' }])
    const text = checks(bad).join('\n')
    expect(text).toContain('command 1')
    expect(text).toContain('not one of this game’s')
    expect(text).toContain('command 2')
    expect(checks(withEvent(d, [{ fade: 'out', ms: 300 }, { background: 'hall' }, { wait: 200 }, { fade: 'in', ms: 300 }, { say: 'ok' }]))).toEqual([])
  })
  it('a campaign scene is checked the same way, weather refused', () => {
    let doc = createCampaign(withBgs(base()))
    const ch = doc.campaign!.chapters[0].id
    const scene = (cmds: Command[]) => {
      const m = cmds.reduce((acc, c) => addCommand(acc, acc.events![0].id, [], c), sceneAsMap(doc, ch, 'before'))
      return withScene(doc, ch, 'before', m)
    }
    expect(campaignChecks(scene([{ background: 'hall' }, { fade: 'out' }, { wait: 300 }, { narrate: 'x' }, { say: 'x', position: 'left' }, { clearStage: true }])).filter((c) => c.level === 'error')).toEqual([])
    const bad = campaignChecks(scene([{ background: 'swamp' }, { weather: 'rain' }])).map((c) => c.text).join('\n')
    expect(bad).toContain('not one of this game’s')
    expect(bad).toContain('map events only')
    doc = scene([{ shake: { strength: 0 } }])
    expect(campaignChecks(doc).map((c) => c.text).join('\n')).toContain('1 to 32')
  })
})

describe('the Backgrounds list', () => {
  it('adds, names, orders and sets the picture', () => {
    let d = base()
    expect(backgroundList(d)).toEqual([])
    d = addBackground(d, 'forest_dusk')
    expect(d.graphics!.backgrounds).toEqual({ forest_dusk: { sheet: 'forest' } })  // the first image, whole
    d = addBackground(d, 'hall')
    d = setBackgroundPicture(d, 'hall', 'places', [1, 0])
    expect(d.graphics!.backgrounds!.hall).toEqual({ sheet: 'places', cell: [1, 0] })
    d = setBackgroundPicture(d, 'hall', 'places', null)  // back to the whole image
    expect(d.graphics!.backgrounds!.hall).toEqual({ sheet: 'places' })
    d = moveBackground(d, 0, 1)
    expect(backgroundList(d).map((b) => b.id)).toEqual(['hall', 'forest_dusk'])
    expect(backgroundsUsingSheet(d, 'places')).toEqual(['hall'])
  })
  it('refuses bad and repeated names; adds nothing without an image', () => {
    const d = withBgs(base())
    expect(backgroundIdProblem(d, '')).toBe('Type a name.')
    expect(backgroundIdProblem(d, 'two words')).toContain('letters, digits')
    expect(backgroundIdProblem(d, 'hall')).toContain('already a background')
    expect(backgroundIdProblem(d, 'hall', 'hall')).toBeNull()
    expect(addBackground(d, 'hall')).toBe(d)
    const noArt: ProjectDoc = { ...base(), graphics: { sheets: [] } }
    expect(addBackground(noArt, 'x')).toBe(noArt)
  })
  it('removing the last one writes no key; removing an image takes the backgrounds that used it', () => {
    let d = addBackground(base(), 'a')
    d = removeBackground(d, 'a')
    expect('backgrounds' in d.graphics!).toBe(false)
    d = setBackgroundPicture(withBgs(base()), 'hall', 'places', [0, 0])
    const without = removeSheet(d, 'places')
    expect(Object.keys(without.graphics!.backgrounds!)).toEqual(['forest_dusk'])
  })
})

describe('commands follow their background', () => {
  const sceneDoc = (): ProjectDoc => {
    let d = withEvent(withBgs(base()), [{ background: 'hall' }, { say: 'Hello' }, { background: 'forest_dusk', transition: 'fade' }, { background: null }])
    d = createCampaign(d)
    const ch = d.campaign!.chapters[0].id
    const m = sceneAsMap(d, ch, 'before')
    return withScene(d, ch, 'before', addCommand(m, m.events![0].id, [], { background: 'hall' }))
  }
  const sceneCmds = (d: ProjectDoc) => sceneOf(d, d.campaign!.chapters[0].id, 'before')
  it('a rename is followed in the maps and the scenes, and clearing is not a name', () => {
    const d = sceneDoc()
    expect(sceneCmds(d)).toEqual([{ background: 'hall' }])
    const r = renameBackground(d, 'hall', 'great_hall')
    expect(commandsOf(r).map((c) => c.background)).toEqual(['great_hall', undefined, 'forest_dusk', null])
    expect(sceneCmds(r)).toEqual([{ background: 'great_hall' }])
    expect(Object.keys(r.graphics!.backgrounds!)).toEqual(['forest_dusk', 'great_hall'])
    expect(backgroundList(renameBackground(d, 'hall', 'forest_dusk'))).toEqual(backgroundList(d))  // taken name: nothing
  })
  it('a delete takes the commands that showed it out in the same edit', () => {
    const d = sceneDoc()
    const lines = backgroundUsageLines(d, ['hall'])
    expect(lines.length).toBeGreaterThanOrEqual(2)
    expect(lines.map((l) => l.text).join('\n')).toContain('Shows the background “hall”')
    const after = reconcileBackgrounds(d, removeBackground(d, 'hall'))
    expect(commandsOf(after).map((c) => commandKind(c))).toEqual(['say', 'background', 'background'])
    expect(commandsOf(after).some((c) => c.background === 'hall')).toBe(false)
    expect(sceneCmds(after)).toEqual([])
    // A rename is not a delete.
    const renamed = renameBackground(d, 'hall', 'great_hall')
    expect(reconcileBackgrounds(d, renamed)).toBe(renamed)
    // What was already dangling is left for the checks to point at.
    const dangling = withEvent(withBgs(base()), [{ background: 'swamp' }])
    expect(reconcileBackgrounds(dangling, removeBackground(dangling, 'hall'))).toEqual(removeBackground(dangling, 'hall'))
  })
})
