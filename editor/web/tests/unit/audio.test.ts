// Sound in the editor's model (docs/SOUND.md): editing audio.json, the overrides and music keys, the scene object form, the
// story commands, rename and delete with every place that names a track or sound following, the checks that mirror the
// player's loader, "used by" lines, and the saving of "no sound". Built on a bare game: the demos and templates may or may
// not have audio of their own, and these tests must not care.
import { describe, expect, it, vi } from 'vitest'
import type { Api, PutBody, PutResult, Result } from '../../src/api/client'
import {
  addSound, addTrack, deleteSound, deleteTrack, dropAudioFile, hasMusic, hasSounds, newTrackId, overrideOf, patchTrack, removeAllAudio,
  setMapMusic, setOverride, setSoundFiles, setSoundVolume, setTrackFile, setTrackLoopStart, setTrackRepeats, setTrackVolume, setUseMusic, setUseSound,
} from '../../src/model/audio'
import { audioChecks, goodAudioPath } from '../../src/model/audioChecks'
import { audioFileUsers, audioNameFor, sniffAudio, unusedAudioFiles } from '../../src/model/audioFiles'
import { allAudioUsageLines, audioUsageLines, reconcileAudio, renameAudioId } from '../../src/model/audioRefs'
import { createCampaign, sceneAsMap, sceneOf, setSceneMusic, withScene } from '../../src/model/campaign'
import { campaignChecks } from '../../src/model/campaignChecks'
import { isSceneObject, sceneCommands, sceneMusic, withSceneCommands, withSceneMusic } from '../../src/model/campaignScene'
import { newCommand } from '../../src/model/storyDefaults'
import { storyChecks } from '../../src/model/storyChecks'
import { addCommand } from '../../src/model/storyEdit'
import { commandText } from '../../src/model/storySummary'
import type { ProjectDoc, ServerProject } from '../../src/model/types'
import { buildBody } from '../../src/store/autosave'
import { DocumentStore } from '../../src/store/documentStore'
import { loadDemo } from './fixtures'

/** The demo with every trace of sound taken out, so what these tests add is all there is. */
function silent(): ProjectDoc {
  const d = loadDemo('demo-srpg')
  const clean = <T extends { sounds?: unknown }>(l: T[] | null) => l?.map((x) => { const { sounds: _s, ...r } = x; return r as T }) ?? null
  const maps: ProjectDoc['maps'] = {}
  for (const [id, m] of Object.entries(d.maps)) { const { music: _a, playerTurnMusic: _b, enemyTurnMusic: _c, events: _e, objectives: _o, ...r } = m!; maps[id] = r as typeof m }
  return { ...d, audio: null, campaign: null, weapons: clean(d.weapons), units: clean(d.units), items: clean(d.items), maps }
}
const mapId = (d: ProjectDoc) => d.game!.maps[0].id

/** A game that uses every kind of place: the standard set, overrides, a map, a scene, story commands. */
function full(): ProjectDoc {
  let d = silent()
  d = addTrack(d, 'title', 'audio/title.ogg')
  d = addTrack(d, 'battle', 'audio/battle.ogg')
  d = addTrack(d, 'win', 'audio/win.wav')
  d = setTrackRepeats(d, 'win', false)
  d = addSound(d, 'hit', 'audio/hit.wav')
  d = addSound(d, 'step', 'audio/step1.wav')
  d = setSoundFiles(d, 'step', ['audio/step1.wav', 'audio/step2.wav'])
  d = setUseMusic(d, 'titleMusic', 'title')
  d = setUseMusic(d, 'mapMusic', 'battle')
  d = setUseMusic(d, 'victory', 'win')
  d = setUseSound(d, 'hit', 'hit')
  d = setUseSound(d, 'step', 'step')
  d = setOverride(d, 'weapons', d.weapons![0].id, 'hit', 'hit')
  d = setOverride(d, 'units', d.units![0].id, 'step', 'step')
  d = setMapMusic(d, mapId(d), 'music', 'battle')
  d = setMapMusic(d, mapId(d), 'enemyTurnMusic', 'title')
  return d
}

describe('editing audio.json', () => {
  it('the first track creates it, sections go where the file format lists them, defaults write no key', () => {
    let d = silent()
    expect(d.audio).toBeNull()
    d = addSound(d, 'hit', 'audio/hit.wav')
    d = addTrack(d, 'title', 'audio/title.ogg')
    d = setUseMusic(d, 'titleMusic', 'title')
    expect(Object.keys(d.audio!)).toEqual(['music', 'sounds', 'use'])
    expect(hasMusic(d) && hasSounds(d)).toBe(true)
    d = setTrackVolume(d, 'title', 80)
    d = setTrackLoopStart(d, 'title', 2.5)
    expect(d.audio!.music!.title).toEqual({ file: 'audio/title.ogg', volume: 80, loopStart: 2.5 })
    d = setTrackVolume(d, 'title', 100)
    d = setTrackLoopStart(d, 'title', 0)
    expect(d.audio!.music!.title).toEqual({ file: 'audio/title.ogg' })
    d = setTrackRepeats(d, 'title', false)
    expect(d.audio!.music!.title.loop).toBe(false)
    d = setTrackRepeats(d, 'title', true)
    expect('loop' in d.audio!.music!.title).toBe(false)
  })
  it('keeps keys it does not know, and their order', () => {
    let d = silent()
    d = { ...d, audio: { music: { a: { file: 'audio/a.ogg', futureKey: 1 } }, futureTop: { x: 1 }, use: { titleMusic: 'a' } } }
    d = patchTrack(d, 'a', { volume: 50 })
    d = setTrackFile(d, 'a', 'audio/b.ogg')
    expect(d.audio).toEqual({ music: { a: { file: 'audio/b.ogg', futureKey: 1, volume: 50 } }, futureTop: { x: 1 }, use: { titleMusic: 'a' } })
    expect(Object.keys(d.audio!)).toEqual(['music', 'futureTop', 'use'])
  })
  it('one file is `file`, several are `files`, and the file key stays where it was', () => {
    let d = addSound(silent(), 'step', 'audio/a.wav')
    d = setSoundVolume(d, 'step', 70)
    d = setSoundFiles(d, 'step', ['audio/a.wav', 'audio/b.wav'])
    expect(Object.keys(d.audio!.sounds!.step)).toEqual(['files', 'volume'])
    expect(d.audio!.sounds!.step.files).toEqual(['audio/a.wav', 'audio/b.wav'])
    d = setSoundFiles(d, 'step', ['audio/b.wav'])
    expect(d.audio!.sounds!.step).toEqual({ file: 'audio/b.wav', volume: 70 })
    expect(setSoundFiles(d, 'step', [])).toBe(d)
    expect(setSoundFiles(d, 'step', Array.from({ length: 20 }, (_, i) => `audio/${i}.wav`)).audio!.sounds!.step.files).toHaveLength(16)
  })
  it('the standard set: choices are written, "Nothing" leaves the key out, an emptied block goes', () => {
    let d = addSound(addTrack(silent(), 't', 'audio/t.ogg'), 's', 'audio/s.wav')
    d = setUseSound(d, 'hit', 's')
    d = setUseSound(d, 'miss', 's')
    expect(d.audio!.use!.sounds).toEqual({ hit: 's', miss: 's' })
    d = setUseSound(d, 'hit', undefined)
    d = setUseSound(d, 'miss', undefined)
    expect(d.audio!.use).toBeUndefined()
    d = setUseMusic(d, 'mapMusic', 't')
    expect(d.audio!.use).toEqual({ mapMusic: 't' })
    expect(setUseMusic(d, 'mapMusic', 't')).toBe(d)
    d = setUseMusic(d, 'mapMusic', undefined)
    expect('use' in d.audio!).toBe(false)
  })
  it('overrides: a sound, "" for silence on purpose, undefined for "same as the game"; an emptied block goes; other keys stay', () => {
    let d = addSound(silent(), 's', 'audio/s.wav')
    const w = d.weapons![0].id
    d = setOverride(d, 'weapons', w, 'hit', 's')
    d = setOverride(d, 'weapons', w, 'miss', '')
    expect(d.weapons![0].sounds).toEqual({ hit: 's', miss: '' })
    expect(overrideOf(d, 'weapons', w, 'miss')).toBe('')
    expect(setOverride(d, 'weapons', w, 'gold', 's')).toBe(d) // not a key a weapon may have
    d = setOverride(d, 'weapons', w, 'hit', undefined)
    d = setOverride(d, 'weapons', w, 'miss', undefined)
    expect('sounds' in d.weapons![0]).toBe(false)
    expect(d.weapons![0].name).toBe(silent().weapons![0].name)
  })
  it('a map\'s music: a track, "" for no music, undefined for the game\'s', () => {
    let d = addTrack(silent(), 't', 'audio/t.ogg')
    const m = mapId(d)
    d = setMapMusic(d, m, 'music', 't')
    d = setMapMusic(d, m, 'playerTurnMusic', '')
    expect(d.maps[m]!.music).toBe('t')
    expect(d.maps[m]!.playerTurnMusic).toBe('')
    d = setMapMusic(d, m, 'playerTurnMusic', undefined)
    expect('playerTurnMusic' in d.maps[m]!).toBe(false)
  })
  it('names for new tracks and sounds are made from what the person typed and never collide', () => {
    const d = addTrack(silent(), 'battle_theme', 'audio/a.ogg')
    expect(newTrackId(d, 'Battle Theme')).toBe('battle_theme_2')
    expect(newTrackId(d, '')).toBe('track')
  })
})

describe('the scene object form', () => {
  const say = [{ say: 'Hi', speakerName: 'N' }]
  it('music turns the plain list into the object, taking it off turns it back', () => {
    const o = withSceneMusic(say, 'title')
    expect(o).toEqual({ music: 'title', commands: say })
    expect(isSceneObject(o)).toBe(true)
    expect(sceneMusic(o)).toBe('title')
    expect(sceneCommands(o)).toEqual(say)
    expect(withSceneMusic(o, undefined)).toEqual(say)
    expect(withSceneMusic(withSceneMusic(undefined, 'a'), undefined)).toBeUndefined()
    expect(withSceneMusic([], 'a')).toEqual({ music: 'a' })
    // "keys it does not know" keep the object
    expect(withSceneMusic({ music: 'a', futureKey: 1, commands: say }, undefined)).toEqual({ futureKey: 1, commands: say })
  })
  it('editing the commands keeps the music; emptied commands leave only the music; no music and no commands is no key', () => {
    expect(withSceneCommands({ music: 'a', commands: say }, [])).toEqual({ music: 'a' })
    expect(withSceneCommands({ music: 'a' }, say)).toEqual({ music: 'a', commands: say })
    expect(withSceneCommands(undefined, [])).toBeUndefined()
    expect(withSceneCommands(say, [])).toBeUndefined()
  })
  it('the campaign screen writes the object only while a scene has music', () => {
    let d = addTrack(createCampaign(silent()), 't', 'audio/t.ogg')
    const ch = d.campaign!.chapters[0].id
    d = setSceneMusic(d, ch, 'before', 't')
    expect(d.campaign!.chapters[0].before).toEqual({ music: 't' })
    // edit the commands through the scene map, like the screen does
    const m = sceneAsMap(d, ch, 'before')
    d = withScene(d, ch, 'before', addCommand(m, m.events![0].id, [], { say: 'Hello' }))
    expect(d.campaign!.chapters[0].before).toEqual({ music: 't', commands: [{ say: 'Hello' }] })
    expect(sceneOf(d, ch, 'before')).toEqual([{ say: 'Hello' }])
    d = setSceneMusic(d, ch, 'before', undefined)
    expect(d.campaign!.chapters[0].before).toEqual([{ say: 'Hello' }])
  })
})

describe('deleting and renaming follow everywhere, in one step', () => {
  function withStory(d: ProjectDoc): ProjectDoc {
    const m = mapId(d)
    const map = { ...d.maps[m]!, events: [{ id: 'intro', when: { battle: 'start' as const }, do: [{ playMusic: 'battle', fade: 1 }, { playSound: 'hit' }, { stopMusic: true }] }] }
    let out: ProjectDoc = { ...d, maps: { ...d.maps, [m]: map } }
    out = createCampaign(out)
    const ch = out.campaign!.chapters[0].id
    out = setSceneMusic(out, ch, 'before', 'battle')
    const sm = sceneAsMap(out, ch, 'before')
    out = withScene(out, ch, 'before', addCommand(sm, sm.events![0].id, [], { playSound: 'hit' }))
    const am = sceneAsMap(out, ch, 'after')
    out = withScene(out, ch, 'after', addCommand(am, am.events![0].id, [], { playMusic: 'battle' }))
    return out
  }

  it('deleting a track takes it out of the standard set, maps, scenes and story commands (and nothing else)', () => {
    const before = withStory(full())
    const after = reconcileAudio(before, deleteTrack(before, 'battle'))
    expect(after.audio!.music!.battle).toBeUndefined()
    expect(after.audio!.use!.mapMusic).toBeUndefined()
    expect(after.audio!.use!.titleMusic).toBe('title') // untouched
    expect(after.maps[mapId(after)]!.music).toBeUndefined()
    expect(after.maps[mapId(after)]!.enemyTurnMusic).toBe('title')
    const cmds = after.maps[mapId(after)]!.events![0].do
    expect(cmds).toEqual([{ playSound: 'hit' }, { stopMusic: true }]) // the playMusic command went
    expect(after.campaign!.chapters[0].before).toEqual({ commands: [{ playSound: 'hit' }] }.commands) // only the music was taken off: back to the plain list
    expect(after.campaign!.chapters[0].after).toEqual([]) // its only command was the track's (a plain scene keeps an empty list, as for every other clean-up)
    expect(storyChecks(after, mapId(after))).toEqual([])
    expect(campaignChecks(after)).toEqual([])
  })
  it('deleting a sound takes it out of the standard set, overrides and story commands', () => {
    const before = withStory(full())
    const after = reconcileAudio(before, deleteSound(before, 'hit'))
    expect(after.audio!.use!.sounds).toEqual({ step: 'step' })
    expect(after.weapons![0].sounds).toBeUndefined()
    expect(after.maps[mapId(after)]!.events![0].do.some((c) => c.playSound)).toBe(false)
    expect(after.campaign!.chapters[0].before).toEqual({ music: 'battle' })
  })
  it('only what was there before and is gone after is touched: an id that already dangled stays for the checks', () => {
    const d = setOverride(full(), 'weapons', full().weapons![0].id, 'miss', 'never_existed')
    const after = reconcileAudio(d, deleteSound(d, 'hit'))
    expect(after.weapons![0].sounds).toEqual({ miss: 'never_existed' })
    expect(reconcileAudio(d, d)).toBe(d)
    const louder = setTrackVolume(d, 'title', 50)
    expect(reconcileAudio(d, louder)).toBe(louder) // no id lost: the very same object back
  })
  it('"Silent" and "no music" ("") are not names and are never removed', () => {
    let d = full()
    d = setOverride(d, 'weapons', d.weapons![0].id, 'miss', '')
    d = setMapMusic(d, mapId(d), 'playerTurnMusic', '')
    const after = reconcileAudio(d, deleteSound(deleteTrack(d, 'battle'), 'hit'))
    expect(after.weapons![0].sounds).toEqual({ miss: '' })
    expect(after.maps[mapId(after)]!.playerTurnMusic).toBe('')
  })
  it('removing all sound takes every place out; the file would be deleted', () => {
    const before = withStory(full())
    const after = reconcileAudio(before, removeAllAudio(before))
    expect(after.audio).toBeNull()
    expect(after.weapons![0].sounds).toBeUndefined()
    expect(after.units![0].sounds).toBeUndefined()
    expect(after.maps[mapId(after)]!.music).toBeUndefined()
    expect(after.maps[mapId(after)]!.events![0].do).toEqual([{ stopMusic: true }])
    expect(after.campaign!.chapters[0].before).toEqual([])
    expect(after.campaign!.chapters[0].after).toEqual([])
    expect(storyChecks(after, mapId(after))).toEqual([])
    expect(audioChecks(after)).toEqual([])
  })
  it('renaming follows every place and keeps the order of the list', () => {
    const before = withStory(full())
    const after = renameAudioId(before, 'music', 'battle', 'fight')
    expect(Object.keys(after.audio!.music!)).toEqual(['title', 'fight', 'win'])
    expect(after.audio!.use!.mapMusic).toBe('fight')
    expect(after.maps[mapId(after)]!.music).toBe('fight')
    expect(after.maps[mapId(after)]!.events![0].do[0]).toEqual({ playMusic: 'fight', fade: 1 })
    expect(after.campaign!.chapters[0].before).toEqual({ music: 'fight', commands: [{ playSound: 'hit' }] })
    expect(after.campaign!.chapters[0].after).toEqual([{ playMusic: 'fight' }])
    const s = renameAudioId(before, 'sound', 'hit', 'thump')
    expect(s.audio!.use!.sounds).toEqual({ hit: 'thump', step: 'step' })
    expect(s.weapons![0].sounds).toEqual({ hit: 'thump' })
    // a track and a sound may share a name: renaming one leaves the other's references alone
    const both = addSound(addTrack(silent(), 'x', 'audio/x.ogg'), 'x', 'audio/x.wav')
    expect(renameAudioId(setUseSound(setUseMusic(both, 'mapMusic', 'x'), 'hit', 'x'), 'music', 'x', 'y').audio!.use).toEqual({ mapMusic: 'y', sounds: { hit: 'x' } })
    expect(renameAudioId(before, 'music', 'battle', 'title')).toBe(before) // taken
  })
  it('"used by" lists what a delete would take out', () => {
    const d = withStory(full())
    const lines = audioUsageLines(d, 'music', 'battle').map((u) => u.text)
    expect(lines.some((l) => l.includes('standard set') && l.includes('Battles'))).toBe(true)
    expect(lines.some((l) => l.startsWith('Map ') && l.includes('Music'))).toBe(true)
    expect(lines.some((l) => l.includes('Plays the music “battle”'))).toBe(true)
    expect(lines.some((l) => l.includes('Campaign') && l.includes('its music is taken off'))).toBe(true)
    const sound = audioUsageLines(d, 'sound', 'hit').map((u) => u.text)
    expect(sound.some((l) => l.startsWith('Weapon '))).toBe(true)
    expect(audioUsageLines(d, 'music', 'win').map((u) => u.text)).toHaveLength(1)
    expect(audioUsageLines(silent(), 'music', 'nope')).toEqual([])
    const all = allAudioUsageLines(d).map((u) => u.text)
    expect(new Set(all).size).toBe(all.length)
    expect(all.length).toBeGreaterThan(lines.length)
  })
  it('a deleted sound FILE takes the tracks that play it and the variants that are it', () => {
    let d = full()
    d = addTrack(d, 'solo', 'audio/step2.wav')
    expect(audioFileUsers(d, 'audio/step2.wav')).toEqual([{ kind: 'music', id: 'solo' }, { kind: 'sound', id: 'step' }])
    const after = dropAudioFile(d, 'audio/step2.wav')
    expect(after.audio!.music!.solo).toBeUndefined()
    expect(after.audio!.sounds!.step).toEqual({ file: 'audio/step1.wav' })
    expect(dropAudioFile(after, 'audio/hit.wav').audio!.sounds!.hit).toBeUndefined()
  })
})

describe('story commands in the model', () => {
  it('start valid where they can and read as plain sentences', () => {
    const d = full()
    const ctx = { doc: d, map: d.maps[mapId(d)]! }
    expect(newCommand(ctx, 'playMusic')).toEqual({ playMusic: 'title' })
    expect(newCommand(ctx, 'playSound')).toEqual({ playSound: 'hit' })
    expect(newCommand(ctx, 'stopMusic')).toEqual({ stopMusic: true })
    expect(commandText(ctx, { playMusic: 'battle' })).toBe('Plays the music “battle”')
    expect(commandText(ctx, { playMusic: 'battle', fade: 1.5 })).toBe('Plays the music “battle” (fades in over 1.5 seconds)')
    expect(commandText(ctx, { playMusic: 'battle', fade: 0 })).toBe('Plays the music “battle” (at once)')
    expect(commandText(ctx, { stopMusic: true, fade: 1 })).toBe('Stops the music (fades out over 1 second)')
    expect(commandText(ctx, { playSound: 'hit' })).toBe('Plays the sound “hit”')
    expect(newCommand({ doc: silent(), map: ctx.map }, 'playMusic')).toEqual({ playMusic: '' })
  })
})

describe('the checks mirror the player\'s loader', () => {
  const texts = (d: ProjectDoc, files?: Parameters<typeof audioChecks>[1]) => audioChecks(d, files ?? null).map((c) => c.text)
  it('a good game has nothing to say', () => {
    expect(audioChecks(full())).toEqual([])
    expect(audioChecks(silent())).toEqual([])
  })
  it('unknown ids in the standard set, overrides and maps', () => {
    let d = full()
    d = { ...d, audio: { ...d.audio!, use: { ...d.audio!.use, mapMusic: 'nope', sounds: { hit: 'gone' } } } }
    d = { ...d, weapons: d.weapons!.map((w, i) => (i === 0 ? { ...w, sounds: { hit: 'nope2' } } : w)) }
    d = { ...d, maps: { ...d.maps, [mapId(d)]: { ...d.maps[mapId(d)]!, music: 'nope3' } } }
    const all = audioChecks(d)
    expect(all.some((c) => c.section === 'sound' && c.text.includes('“nope”'))).toBe(true)
    expect(all.some((c) => c.section === 'sound' && c.text.includes('“gone”'))).toBe(true)
    expect(all.some((c) => c.section === 'weapons' && c.text.includes('“nope2”'))).toBe(true)
    expect(all.some((c) => c.section === 'maps' && c.mapId === mapId(d) && c.text.includes('“nope3”'))).toBe(true)
  })
  it('a looping jingle, a volume out of range, bad ids, 0 or 17 variants, a path that is not a sound file', () => {
    let d = full()
    d = patchTrack(d, 'win', { loop: undefined })
    expect(texts(d).some((t) => t.includes('Standard set') && t.includes('plays once'))).toBe(true)
    d = patchTrack(setTrackRepeats(d, 'win', false), 'title', { volume: 101, loopStart: -1 })
    expect(texts(d).filter((t) => t.startsWith('Music “title”'))).toHaveLength(2)
    d = { ...d, audio: { ...d.audio!, sounds: { 'bad id': { file: 'audio/x.wav' }, none: { files: [] }, many: { files: Array(17).fill('audio/a.wav') }, mp3: { file: 'audio/x.mp3' }, both: { file: 'audio/a.wav', files: ['audio/b.wav'] } } } }
    const t = texts(d)
    expect(t.some((x) => x.includes('“bad id”') && x.includes('letters, digits'))).toBe(true)
    expect(t.some((x) => x.includes('“none”') && x.includes('1 to 16'))).toBe(true)
    expect(t.some((x) => x.includes('“many”') && x.includes('1 to 16'))).toBe(true)
    expect(t.some((x) => x.includes('“mp3”') && x.includes('.ogg or .wav'))).toBe(true)
    expect(t.some((x) => x.includes('“both”') && x.includes('exactly one'))).toBe(true)
  })
  it('a story command naming nothing, or used in a game without audio.json, is told by the map and campaign checks', () => {
    const d = silent()
    const m = mapId(d)
    const map = { ...d.maps[m]!, events: [{ id: 'intro', when: { battle: 'start' as const }, do: [{ playSound: 'hit' }] }] }
    const bare = { ...d, maps: { ...d.maps, [m]: map } }
    expect(storyChecks(bare, m).map((c) => c.text).join(' ')).toContain('no sound yet')
    const withAudio = addSound(bare, 'other', 'audio/o.wav')
    expect(storyChecks(withAudio, m).map((c) => c.text).join(' ')).toContain('is not a sound of this game')
    const fade = { ...d.maps[m]!, events: [{ id: 'e2', when: { battle: 'start' as const }, do: [{ stopMusic: true, fade: 61 }] }] }
    expect(storyChecks(addSound(silent(), 'a', 'audio/a.wav'), m)).toEqual([])
    expect(storyChecks({ ...addSound(d, 'a', 'audio/a.wav'), maps: { ...d.maps, [m]: fade } }, m).map((c) => c.text).join(' ')).toContain('0 to 60')
    // scenes
    let c = createCampaign(d)
    const ch = c.campaign!.chapters[0].id
    c = { ...c, campaign: { ...c.campaign!, chapters: [{ ...c.campaign!.chapters[0], before: { music: 'nope', commands: [{ playSound: 'zap' }] } }] } }
    expect(campaignChecks(c).map((x) => x.text).join(' ')).toContain('no sound yet')
    expect(campaignChecks(addTrack(addSound(c, 'a', 'audio/a.wav'), 'b', 'audio/b.ogg')).map((x) => x.text).join(' ')).toContain('“nope” is not a music track')
    expect(ch).toBe('chapter_1')
  })
  it('files: a missing file is a hint, a long effect, a bad rate or too many channels are errors', () => {
    const d = full()
    const ok = (file: string, extra: object = {}) => [file, { file, size: 1, format: 'wav' as const, rate: 22050, channels: 1, seconds: 0.5, ...extra }] as const
    const all = new Map([ok('audio/title.ogg'), ok('audio/battle.ogg'), ok('audio/win.wav'), ok('audio/hit.wav'), ok('audio/step1.wav'), ok('audio/step2.wav')])
    expect(audioChecks(d, all)).toEqual([])
    const missing = audioChecks(d, new Map([...all].filter(([f]) => f !== 'audio/hit.wav')))
    expect(missing).toHaveLength(1)
    expect(missing[0].level).toBe('hint')
    expect(missing[0].text).toContain('audio/hit.wav')
    const long = audioChecks(d, new Map([...all, ok('audio/hit.wav', { seconds: 10.5 })]))
    expect(long.map((c) => c.text).join(' ')).toContain('at most 10 seconds')
    expect(long[0].level).toBe('error')
    expect(audioChecks(d, new Map([...all, ok('audio/hit.wav', { rate: 96000 })])).map((c) => c.text).join(' ')).toContain('8000 to 48000')
    expect(audioChecks(d, new Map([...all, ok('audio/hit.wav', { channels: 6 })])).map((c) => c.text).join(' ')).toContain('mono or stereo')
    // a long file used as music is fine
    expect(audioChecks(d, new Map([...all, ok('audio/title.ogg', { seconds: 300 })]))).toEqual([])
  })
  it('paths follow the loader: inside the game, .ogg or .wav', () => {
    for (const good of ['audio/a.wav', 'audio/A.OGG', 'x.ogg']) expect(goodAudioPath(good)).toBe(true)
    for (const bad of ['', 'a.mp3', '/abs/a.wav', '../a.wav', 'audio\\a.wav', 'c:a.wav', '.wav', 5]) expect(goodAudioPath(bad)).toBe(false)
  })
})

describe('sound files', () => {
  it('names on disk, the first bytes, and which files nobody uses', () => {
    expect(audioNameFor('My Song!.WAV', 'wav', [])).toBe('my_song.wav')
    expect(audioNameFor('hit.wav', 'wav', ['audio/hit.wav'])).toBe('hit_2.wav')
    expect(audioNameFor('hit.ogg', 'ogg', ['audio/hit.wav'])).toBe('hit_2.ogg')
    expect(audioNameFor('日本語.ogg', 'ogg', [])).toBe('sound.ogg')
    const bytes = (s: string) => new Uint8Array([...s].map((c) => c.charCodeAt(0)))
    expect(sniffAudio(bytes('RIFF\0\0\0\0WAVEfmt '))).toBe('wav')
    expect(sniffAudio(bytes('OggS\0\u0002'))).toBe('ogg')
    expect(sniffAudio(bytes('hello world, I am text'))).toBeNull()
    expect(sniffAudio(bytes('RIFF\0\0\0\0AVI LIST'))).toBeNull()
    const d = full()
    const files = ['title.ogg', 'battle.ogg', 'win.wav', 'hit.wav', 'step1.wav', 'step2.wav', 'spare.wav'].map((n) => ({ file: `audio/${n}` }))
    expect(unusedAudioFiles(d, files)).toEqual(['audio/spare.wav'])
    expect(unusedAudioFiles(silent(), files)).toHaveLength(7)
  })
})

describe('saving sound', () => {
  const project = (d: ProjectDoc): ServerProject => ({ ...d, id: 'g', revision: 'r0', problems: [] })
  it('audio goes over the wire as an object; taking all of it out is `false`; nothing changed sends nothing', () => {
    const a = full()
    expect(buildBody(a, a, 'r', false)).toBeNull()
    const b = setTrackVolume(a, 'title', 70)
    expect(buildBody(b, a, 'r', false)!.audio).toEqual(b.audio)
    expect(Object.keys(buildBody(b, a, 'r', false)!)).toEqual(['baseRevision', 'audio'])
    const c = reconcileAudio(a, removeAllAudio(a))
    const body = buildBody(c, a, 'r', false)!
    expect(body.audio).toBe(false)
    expect(body.weapons).toBeDefined() // the overrides that named a sound were cleaned in the same save
    // a game that never had audio and still has none: nothing sent for it, not even on "save everything"
    expect('audio' in buildBody(silent(), silent(), 'r', true)!).toBe(false)
  })
  it('one edit that deletes a track is one undo step, and Undo brings every place back', async () => {
    const d = full()
    const api = {
      getProject: vi.fn(async () => ({ ok: true, value: project(d) }) as Result<ServerProject>),
      putProject: vi.fn(async (_id: string, _body: PutBody) => ({ ok: true, value: { revision: 'r1', problems: [] } }) as Result<PutResult>),
      listArt: vi.fn(async () => ({ ok: true, value: [] })),
      listModels: vi.fn(async () => ({ ok: true, value: [] })),
      listAudio: vi.fn(async () => ({ ok: true, value: [] })),
    } as unknown as Api
    const store = new DocumentStore(api, 'g')
    await store.load()
    store.edit((x) => deleteTrack(x, 'battle'))
    const after = store.getState().doc
    expect(after.audio!.music!.battle).toBeUndefined()
    expect(after.audio!.use!.mapMusic).toBeUndefined()
    expect(after.maps[mapId(after)]!.music).toBeUndefined()
    store.undo()
    expect(store.getState().doc).toEqual(d)
    store.redo()
    expect(store.getState().doc).toEqual(after)
    // a rename is one step too
    store.edit((x) => renameAudioId(x, 'sound', 'hit', 'thump'))
    expect(store.getState().doc.weapons![0].sounds).toEqual({ hit: 'thump' })
    store.undo()
    expect(store.getState().doc).toEqual(after)
  })
})
