// The editor's model for what this round of the engine added: the seven stage slots with enter / move / leave transitions, the
// focus speed, music by situation (a side's phase music, few enemies, the preparation and the two new jingles, setPhaseMusic), steps
// by movement type, sounds on frames of a battle animation, and a portrait's facing, talk and blink frames.
import { describe, expect, it } from 'vitest'
import { addTrack, addSound, setUseMusic, setTrackRepeats } from '../../src/model/audio'
import { audioChecks } from '../../src/model/audioChecks'
import { MAP_MUSIC_KEYS, MUSIC_SLOTS, JINGLE_SLOTS } from '../../src/model/audioLabels'
import { allAudioUsageLines, audioUsageLines, reconcileAudio, renameAudioId } from '../../src/model/audioRefs'
import { animSounds, moveTypeStep, setAnimSound, setMapFew, setMapPhaseMusic, setMoveTypeInterval, setMoveTypeStep, setUseFew, setUsePhaseMusic, situationProblems } from '../../src/model/audioSituations'
import { deleteSound, deleteTrack } from '../../src/model/audio'
import { addPortrait, framesText, parseFrames, setPortraitFacing, setPortraitFrames } from '../../src/model/portraits'
import { sceneProblems } from '../../src/model/sceneChecks'
import { newCommand } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { commandText } from '../../src/model/storySummary'
import { commandKind, STAGE_POSITIONS, type Command, type StoryEvent } from '../../src/model/storyTypes'
import type { ProjectDoc } from '../../src/model/types'
import { loadDemo } from './fixtures'

/** The demo without events and without any sound. */
function bare(): ProjectDoc {
  const d = loadDemo('demo-srpg')
  const clean = <T extends { sounds?: unknown }>(l: T[] | null) => l?.map((x) => { const { sounds: _s, ...r } = x; return r as T }) ?? null
  const maps: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(d.maps)) { const { music: _a, playerTurnMusic: _b, enemyTurnMusic: _c, events: _e, objectives: _o, ...r } = m!; maps[id] = r as typeof m }
  return { ...d, audio: null, campaign: null, weapons: clean(d.weapons), units: clean(d.units), items: clean(d.items), maps }
}
const mapId = (d: ProjectDoc) => d.game!.maps[0].id
const sideId = (d: ProjectDoc) => d.maps[mapId(d)]!.sides![0].id
const withEvent = (d: ProjectDoc, commands: Command[]): ProjectDoc => {
  const ev: StoryEvent = { id: 'scene', when: { battle: 'start' }, do: commands }
  return { ...d, maps: { ...d.maps, [mapId(d)]: { ...d.maps[mapId(d)]!, events: [ev] } } }
}
const checks = (d: ProjectDoc) => storyChecks(d, mapId(d)).map((c) => c.text)
const sound = (): ProjectDoc => {
  let d = bare()
  d = addTrack(d, 'calm', 'audio/calm.ogg')
  d = addTrack(d, 'last', 'audio/last.ogg')
  d = addTrack(d, 'jingle', 'audio/jingle.ogg')
  d = setTrackRepeats(d, 'jingle', false)
  d = addSound(d, 'swing', 'audio/swing.wav')
  d = addSound(d, 'tread', 'audio/tread.wav')
  return d
}

describe('the stage commands', () => {
  it('enter and move are scene commands after the old ones and start valid', () => {
    expect(commandKind({ enter: 'eirik', position: 'midLeft' })).toBe('enter')
    expect(commandKind({ move: 'eirik', position: 'right', transition: 'cut' })).toBe('move')
    expect(commandKind({ leave: 'eirik', transition: 'fade' })).toBe('leave')
    expect(commandKind({ setPhaseMusic: { side: 'blue', music: null } })).toBe('setPhaseMusic')
    const d = bare()
    const c = { doc: d, map: d.maps[mapId(d)]! }
    expect(newCommand(c, 'enter')).toMatchObject({ position: 'midLeft' })
    expect(newCommand(c, 'move')).toMatchObject({ position: 'right' })
    expect(newCommand(c, 'setPhaseMusic')).toEqual({ setPhaseMusic: { side: sideId(d), music: '' } })
    expect(STAGE_POSITIONS).toHaveLength(7)
  })

  it('the sentences say where and how', () => {
    const d = bare()
    const c = { doc: d, map: d.maps[mapId(d)]! }
    expect(commandText(c, { enter: 'Mira', position: 'farRight', transition: 'fade' })).toBe('“Mira” comes on stage at the far right with a fade')
    expect(commandText(c, { move: 'Mira', position: 'midLeft' })).toBe('“Mira” moves left of the middle')
    expect(commandText(c, { move: 'Mira', position: 'left', transition: 'cut' })).toBe('“Mira” moves on the left at once')
    expect(commandText(c, { leave: 'Mira', transition: 'fade' })).toBe('“Mira” leaves the stage with a fade')
    expect(commandText(c, { focus: { x: 1, y: 1 }, speed: 4 })).toContain('at 4 squares a second')
    expect(commandText(c, { setPhaseMusic: { side: sideId(d), music: 'calm' } })).toContain('Changes the music of')
    expect(commandText(c, { setPhaseMusic: { side: sideId(d), music: null } })).toContain('Takes the changed music')
    expect(commandText(c, { setPhaseMusic: { side: sideId(d), music: '' } })).toContain('no music of its own')
    expect(commandText(c, { say: 'Hi', speaker: 'x', position: 'midRight' })).toContain('right of the middle')
  })

  it('the checks mirror the loader', () => {
    const d = bare()
    const p = (c: Command) => sceneProblems(d, c, false).join(' ')
    expect(p({ enter: 'a', position: 'midLeft' })).toBe('')
    expect(p({ enter: 'a' } as Command)).toContain('pick the place')
    expect(p({ enter: 'a', position: 'up' as never })).toContain('farLeft, left, midLeft')
    expect(p({ enter: '', position: 'left' })).toContain('name the speaker')
    expect(p({ move: 'a', position: 'left', transition: 'wipe' as never })).toContain('“cut” or “fade”')
    expect(p({ leave: 'a', transition: 'fade', ms: 99999 })).toContain('milliseconds')
    expect(p({ say: 'x', position: 'farLeft' })).toBe('')
    expect(p({ focus: 'a', speed: 4 })).toBe('')
    expect(p({ focus: 'a', speed: 0 })).toContain('squares per second')
    expect(p({ focus: 'a', speed: 4, ms: 300 })).toContain('not both')
  })

  it('setPhaseMusic: the side and the track follow a rename and are checked', () => {
    let d = withEvent(sound(), [{ setPhaseMusic: { side: sideId(bare()), music: 'calm' } }])
    expect(checks(d)).toEqual([])
    d = renameAudioId(d, 'music', 'calm', 'peace')
    expect(d.maps[mapId(d)]!.events![0].do[0].setPhaseMusic!.music).toBe('peace')
    expect(checks(withEvent(sound(), [{ setPhaseMusic: { side: 'nobody', music: 'calm' } }])).join(' ')).toContain('nobody')
    expect(checks(withEvent(sound(), [{ setPhaseMusic: { side: sideId(bare()), music: 'gone' } }])).join(' ')).toContain('gone')
    // "" and null name no track: nothing to check
    expect(checks(withEvent(sound(), [{ setPhaseMusic: { side: sideId(bare()), music: '' } }, { setPhaseMusic: { side: sideId(bare()), music: null } }]))).toEqual([])
    // deleting the track takes the command away
    const before = withEvent(sound(), [{ setPhaseMusic: { side: sideId(bare()), music: 'calm' } }])
    const gone = reconcileAudio(before, deleteTrack(before, 'calm'))
    expect(gone.maps[mapId(gone)]!.events![0].do).toEqual([])
  })
})

describe('music by situation', () => {
  it('lists the new slots and keys; the two new jingles must not repeat', () => {
    expect(MUSIC_SLOTS.slice(-3)).toEqual(['prepMusic', 'promotion', 'chapterCard'])
    expect(JINGLE_SLOTS).toEqual(['victory', 'defeat', 'levelUp', 'promotion', 'chapterCard'])
    expect(MAP_MUSIC_KEYS).toContain('prepMusic')
    let d = sound()
    d = setUseMusic(d, 'promotion', 'calm')
    expect(audioChecks(d).map((c) => c.text).join(' ')).toContain('repeats, but this plays once')
    d = setUseMusic(d, 'promotion', 'jingle')
    d = setUseMusic(d, 'chapterCard', 'jingle')
    d = setUseMusic(d, 'prepMusic', 'calm')
    expect(audioChecks(d)).toEqual([])
  })

  it('phase music per side and few enemies: edits, no-ops and file shape', () => {
    let d = sound()
    d = setUsePhaseMusic(d, 'green', 'calm')
    d = setUsePhaseMusic(d, 'red', '')
    expect(d.audio!.use!.phaseMusic).toEqual({ green: 'calm', red: '' })
    expect(setUsePhaseMusic(d, 'green', 'calm')).toBe(d)
    d = setUsePhaseMusic(setUsePhaseMusic(d, 'green', undefined), 'red', undefined)
    expect(d.audio!.use?.phaseMusic).toBeUndefined()
    d = setUseFew(d, { music: 'last', count: 2 })
    expect(d.audio!.use!.fewEnemies).toEqual({ music: 'last', count: 2 })
    d = setMapPhaseMusic(d, mapId(d), sideId(d), 'calm')
    expect(d.maps[mapId(d)]!.phaseMusic).toEqual({ [sideId(d)]: 'calm' })
    d = setMapFew(d, mapId(d), { music: 'last', count: 1 })
    expect(d.maps[mapId(d)]!.fewEnemies).toEqual({ music: 'last', count: 1 })
    expect(situationProblems(d)).toEqual([])
  })

  it('checks: unknown tracks, bad counts, sides the map does not have', () => {
    let d = sound()
    d = setUsePhaseMusic(d, 'green', 'nope')
    d = setUseFew(d, { music: 'last', count: 0 })
    d = setMapPhaseMusic(d, mapId(d), 'purple', 'calm')
    d = setMapFew(d, mapId(d), { music: 'nope', count: 1000 })
    const text = situationProblems(d).map((p) => p.text).join('\n')
    expect(text).toContain('music on “green”’s turn: “nope” is not a music track')
    expect(text).toContain('Standard set, few enemies left: the number of hostile units')
    expect(text).toContain('names a side this map does not have')
    expect(text).toContain('Map')
    expect(text).toContain('from 1 to 999')
    // they show on the Sound / Maps screens through audioChecks
    expect(audioChecks(d).length).toBeGreaterThan(3)
  })

  it('a rename or a delete of a track follows into phase music, few enemies and the maps', () => {
    let d = sound()
    d = setUsePhaseMusic(d, 'green', 'calm')
    d = setUseFew(d, { music: 'calm', count: 2 })
    d = setMapPhaseMusic(d, mapId(d), sideId(d), 'calm')
    d = setMapFew(d, mapId(d), { music: 'calm', count: 1 })
    const renamed = renameAudioId(d, 'music', 'calm', 'peace')
    expect(renamed.audio!.use!.phaseMusic).toEqual({ green: 'peace' })
    expect(renamed.audio!.use!.fewEnemies!.music).toBe('peace')
    expect(renamed.maps[mapId(d)]!.phaseMusic![sideId(d)]).toBe('peace')
    expect(renamed.maps[mapId(d)]!.fewEnemies!.music).toBe('peace')
    expect(audioUsageLines(d, 'music', 'calm').length).toBeGreaterThanOrEqual(4)
    const gone = reconcileAudio(d, deleteTrack(d, 'calm'))
    expect(gone.audio!.use?.phaseMusic).toBeUndefined()
    expect(gone.audio!.use?.fewEnemies).toBeUndefined()
    expect(gone.maps[mapId(d)]!.phaseMusic).toBeUndefined()
    expect(gone.maps[mapId(d)]!.fewEnemies).toBeUndefined()
  })
})

describe('steps by movement type', () => {
  it('sets the sound and the interval in the movement type and keeps the rest', () => {
    let d = sound()
    const mt = d.game!.moveTypes[0].id
    d = setMoveTypeStep(d, mt, 'tread')
    d = setMoveTypeInterval(d, mt, 0.25)
    expect(moveTypeStep(d, mt)).toEqual({ step: 'tread', interval: 0.25 })
    expect((d.game!.moveTypes[0].sounds as Record<string, unknown>)).toEqual({ step: 'tread', stepInterval: 0.25 })
    d = setMoveTypeStep(d, mt, undefined)
    d = setMoveTypeInterval(d, mt, undefined)
    expect(d.game!.moveTypes[0].sounds).toBeUndefined()
  })

  it('checks and follow-through', () => {
    let d = sound()
    const mt = d.game!.moveTypes[0].id
    d = setMoveTypeStep(d, mt, 'tread')
    expect(situationProblems(d)).toEqual([])
    expect(situationProblems(setMoveTypeInterval(d, mt, 5)).map((p) => p.text).join(' ')).toContain('seconds, from 0.02 to 2')
    expect(situationProblems(setMoveTypeStep(d, mt, 'nope')).map((p) => p.text).join(' ')).toContain('not a sound of this game')
    expect(renameAudioId(d, 'sound', 'tread', 'stomp').game!.moveTypes[0].sounds).toEqual({ step: 'stomp' })
    expect(reconcileAudio(d, deleteSound(d, 'tread')).game!.moveTypes[0].sounds).toBeUndefined()
    expect(allAudioUsageLines(d).some((u) => u.section === 'game')).toBe(true)
  })
})

describe('sounds on frames of a battle animation', () => {
  const withSet = (): ProjectDoc => {
    const d = sound()
    return { ...d, graphics: { ...(d.graphics ?? { sheets: [] }), sheets: d.graphics?.sheets?.length ? d.graphics.sheets : [{ id: 'k', file: 'art/k.png', cellWidth: 16, cellHeight: 16 }],
      battle: { sets: { knight: { animations: { stand: { sheet: 'k', frames: [[0, 0]] }, attack: { sheet: 'k', frames: [[0, 1], [1, 1], [2, 1], [3, 1]], hit: 2 } } } } } } }
  }
  it('adds, moves and takes away a sound on a frame', () => {
    let d = withSet()
    d = setAnimSound(d, 'knight', 'attack', 1, 'swing')
    d = setAnimSound(d, 'knight', 'attack', 3, 'tread')
    expect(animSounds(d.graphics!.battle!.sets!.knight.animations.attack)).toEqual([[1, 'swing'], [3, 'tread']])
    expect(setAnimSound(d, 'knight', 'attack', 1, 'swing')).toBe(d)
    d = setAnimSound(d, 'knight', 'attack', 3, undefined)
    expect(d.graphics!.battle!.sets!.knight.animations.attack.sounds).toEqual({ '1': 'swing' })
    d = setAnimSound(d, 'knight', 'attack', 1, undefined)
    expect(d.graphics!.battle!.sets!.knight.animations.attack.sounds).toBeUndefined()
    expect(setAnimSound(d, 'knight', 'nope', 0, 'swing')).toBe(d)
  })
  it('checks the frame and the sound, and a sound that goes takes its frame with it', () => {
    let d = setAnimSound(withSet(), 'knight', 'attack', 1, 'swing')
    expect(situationProblems(d)).toEqual([])
    const a = d.graphics!.battle!.sets!.knight.animations.attack
    d = { ...d, graphics: { ...d.graphics!, battle: { sets: { knight: { animations: { ...d.graphics!.battle!.sets!.knight.animations, attack: { ...a, sounds: { '1': 'swing', '9': 'swing', '2': 'whoosh' } } } } } } } }
    const text = situationProblems(d).map((p) => p.text).join('\n')
    expect(text).toContain('a sound on frame 9, but the animation has frames 0 to 3')
    expect(text).toContain('“whoosh”, which is not a sound of this game')
    const ok = setAnimSound(withSet(), 'knight', 'attack', 1, 'swing')
    expect(renameAudioId(ok, 'sound', 'swing', 'swoosh').graphics!.battle!.sets!.knight.animations.attack.sounds).toEqual({ '1': 'swoosh' })
    expect(reconcileAudio(ok, deleteSound(ok, 'swing')).graphics!.battle!.sets!.knight.animations.attack.sounds).toBeUndefined()
  })
})

describe('portraits: facing, talk and blink', () => {
  const withPortrait = (): ProjectDoc => {
    const d = bare()
    return addPortrait({ ...d, graphics: { ...(d.graphics ?? { sheets: [] }), sheets: [{ id: 'faces', file: 'art/faces.png', cellWidth: 48, cellHeight: 48 }] } }, 'eirik')
  }
  it('sets and clears the keys, and leaves a portrait that says nothing as it was', () => {
    let d = withPortrait()
    const sheet = d.graphics!.portraits!.eirik.sheet
    expect(d.graphics!.portraits!.eirik).toEqual({ sheet, cell: [0, 0] })
    d = setPortraitFacing(d, 'eirik', 'left')
    d = setPortraitFrames(d, 'eirik', 'talk', [[0, 1], [1, 1]])
    d = setPortraitFrames(d, 'eirik', 'blink', [[0, 2]])
    expect(d.graphics!.portraits!.eirik).toEqual({ sheet, cell: [0, 0], faces: 'left', talk: [[0, 1], [1, 1]], blink: [[0, 2]] })
    d = setPortraitFacing(d, 'eirik', undefined)
    d = setPortraitFrames(d, 'eirik', 'talk', [])
    d = setPortraitFrames(d, 'eirik', 'blink', [])
    expect(d.graphics!.portraits!.eirik).toEqual({ sheet, cell: [0, 0] })
  })
  it('reads "2,1  3,1" with the column and row counted from 1', () => {
    expect(parseFrames('2,1  3,1')).toEqual({ cells: [[1, 0], [2, 0]] })
    expect(parseFrames('')).toEqual({ cells: [] })
    expect(framesText([[1, 0], [2, 0]])).toBe('2,1  3,1')
    expect('error' in parseFrames('a,b')).toBe(true)
    expect('error' in parseFrames('0,1')).toBe(true)
    expect('error' in parseFrames('1,1 1,1 1,1 1,1 1,1 1,1 1,1 1,1 1,1')).toBe(true)
  })
})
