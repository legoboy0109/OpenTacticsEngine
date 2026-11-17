import { describe, expect, it } from 'vitest'
import { addAssign, addBackground, addSet, assignList, backgroundList, battleSceneMode, deleteSet, dropBattleOf, framesFromRow, msList, removeAssign, removeBackground, renameSet, setAnim, setAnchor, setBackgroundPicture, setBattleSceneMode, setDefaultBackground, setFaces, setIdProblem, setList, setMirror, setTerrainBackground, terrainBackground, updateAssign, weaponGroups } from '../../src/model/battleArt'
import { battleChecks } from '../../src/model/battleChecks'
import { removeSheet } from '../../src/model/graphics'
import { cleanUnit } from '../../src/model/references'
import type { ProjectDoc } from '../../src/model/types'

const doc = (extra: Partial<ProjectDoc> = {}): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [] },
  terrain: [{ id: 'plain', name: 'Plain' }, { id: 'wood', name: 'Wood' }], structures: null,
  weapons: [{ id: 'sword', name: 'Sword', group: 'blade' }, { id: 'bow', name: 'Bow', group: 'bow' }, { id: 'rock', name: 'Rock' }],
  items: null, research: null,
  units: [{ id: 'knight', name: 'Knight' }, { id: 'archer', name: 'Archer' }], maps: {}, scripts: {},
  graphics: { sheets: [{ id: 'fighters', file: 'art/f.png', cellWidth: 48, cellHeight: 48 }, { id: 'bg', file: 'art/bg.png', cellWidth: 240, cellHeight: 135 }] },
  campaign: null, audio: null, characters: [{ id: 'eirik', name: 'Eirik', type: 'knight' }], skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
  ...extra,
} as unknown as ProjectDoc)

describe('battle scene default', () => {
  it('is on the map unless the game opts in, and "map" is written as no key', () => {
    let d = doc()
    expect(battleSceneMode(d)).toBe('map')
    d = setBattleSceneMode(d, 'scene')
    expect(d.game?.battleScene).toBe('scene')
    expect(battleSceneMode(d)).toBe('scene')
    d = setBattleSceneMode(d, 'map')
    expect('battleScene' in d.game!).toBe(false)
    expect(setBattleSceneMode(d, 'map')).toBe(d)
  })
})

describe('sets', () => {
  it('a new set starts with a stand from the first image; the id is checked', () => {
    let d = addSet(doc(), 'knight')
    expect(setList(d).map((s) => s.id)).toEqual(['knight'])
    expect(d.graphics!.battle!.sets!.knight.animations.stand).toEqual({ sheet: 'fighters', frames: [[0, 0]] })
    expect(setIdProblem(d, 'knight')).toMatch(/already/)
    expect(setIdProblem(d, 'a b')).toMatch(/letters/)
    expect(setIdProblem(d, '')).toMatch(/name/)
    expect(addSet(d, 'knight')).toBe(d)
    expect(addSet({ ...doc(), graphics: { sheets: [] } }, 'x').graphics?.battle).toBeUndefined() // no image: no set
  })
  it('frames come from a row and a count; marks and times follow the frame count', () => {
    expect(framesFromRow(2, 3)).toEqual([[0, 2], [1, 2], [2, 2]])
    expect(framesFromRow(0, 0)).toEqual([[0, 0]])
    let d = addSet(doc(), 'k')
    d = setAnim(d, 'k', 'attack', { frames: framesFromRow(1, 4), ms: [100, 100, 80, 150], hit: 3 })
    d = setAnim(d, 'k', 'attack', { frames: framesFromRow(1, 2) })
    const a = d.graphics!.battle!.sets!.k.animations.attack
    expect(a.hit).toBe(1)                // pulled back to the last frame
    expect(a.ms).toEqual([100, 100])
    expect(msList({ sheet: 's', frames: framesFromRow(0, 3), ms: 90 })).toEqual([90, 90, 90])
    expect(msList({ sheet: 's', frames: framesFromRow(0, 2) })).toEqual([120, 120])
  })
  it('the stand cannot be removed, another animation can', () => {
    let d = setAnim(addSet(doc(), 'k'), 'k', 'hurt', { frames: framesFromRow(2, 2) })
    expect(setAnim(d, 'k', 'stand', null)).toBe(d)
    d = setAnim(d, 'k', 'hurt', null)
    expect(Object.keys(d.graphics!.battle!.sets!.k.animations)).toEqual(['stand'])
  })
  it('faces, mirror and the foot point are optional keys', () => {
    let d = addSet(doc(), 'k')
    d = setFaces(setMirror(setAnchor(d, 'k', [24, 46]), 'k', false), 'k', 'left')
    expect(d.graphics!.battle!.sets!.k).toMatchObject({ anchor: [24, 46], mirror: false, faces: 'left' })
    d = setFaces(setMirror(setAnchor(d, 'k', undefined), 'k', true), 'k', 'right')
    expect(Object.keys(d.graphics!.battle!.sets!.k)).toEqual(['animations'])
  })
  it('renaming a set keeps who uses it; deleting it removes those rules and the empty section', () => {
    let d = addAssign(addSet(doc(), 'k'), 'k', 'knight')
    d = renameSet(d, 'k', 'lancer')
    expect(assignList(d)).toEqual([{ set: 'lancer', type: 'knight' }])
    expect(renameSet(d, 'lancer', 'a b')).toBe(d)
    d = deleteSet(d, 'lancer')
    expect(d.graphics!.battle).toBeUndefined()
  })
})

describe('who uses a set', () => {
  it('rules can be added, changed and removed; weapon groups come from the weapons', () => {
    let d = addSet(doc(), 'k')
    d = addAssign(d, 'k')
    expect(assignList(d)).toEqual([{ set: 'k', type: 'knight' }])
    d = addAssign(d, 'k')
    expect(assignList(d)[1].type).toBe('archer') // the next type without a rule
    d = updateAssign(d, 1, { group: 'bow' })
    expect(assignList(d)[1]).toEqual({ set: 'k', type: 'archer', group: 'bow' })
    d = updateAssign(d, 1, { group: undefined, character: 'eirik' })
    expect(assignList(d)[1]).toEqual({ set: 'k', type: 'archer', character: 'eirik' })
    d = removeAssign(removeAssign(d, 1), 0)
    expect(d.graphics!.battle!.assign).toBeUndefined()
    expect(weaponGroups(doc())).toEqual(['blade', 'bow'])
  })
  it('a deleted unit type, character or terrain takes its rules and choices with it', () => {
    let d = addAssign(addAssign(addSet(doc(), 'k'), 'k', 'knight'), 'k', 'archer')
    d = updateAssign(d, 1, { character: 'eirik' })
    expect(assignList(dropBattleOf(d, 'type', 'knight'))).toEqual([{ set: 'k', type: 'archer', character: 'eirik' }])
    expect(assignList(dropBattleOf(d, 'character', 'eirik'))).toEqual([{ set: 'k', type: 'knight' }])
    expect(assignList(cleanUnit(d, 'knight'))).toEqual([{ set: 'k', type: 'archer', character: 'eirik' }])
    let t = setTerrainBackground(addBackground(doc(), 'meadow'), 'wood', 'meadow')
    t = dropBattleOf(t, 'terrain', 'wood')
    expect(t.graphics!.battle!.terrainBackground).toBeUndefined()
  })
})

describe('backgrounds', () => {
  it('can be added, picked per terrain, made the default and removed with what points at them', () => {
    let d = addBackground(doc(), 'meadow')
    expect(backgroundList(d)).toEqual([{ id: 'meadow', sheet: 'fighters', cell: [0, 0] }])
    d = setBackgroundPicture(d, 'meadow', 'bg', [0, 0])
    d = setTerrainBackground(d, 'wood', 'meadow')
    d = setDefaultBackground(d, 'meadow')
    expect(terrainBackground(d, 'wood')).toBe('meadow')
    expect(terrainBackground(d, 'plain')).toBe('')
    d = setTerrainBackground(d, 'wood', '')
    expect(d.graphics!.battle!.terrainBackground).toBeUndefined()
    d = setTerrainBackground(d, 'plain', 'meadow')
    d = removeBackground(d, 'meadow')
    expect(d.graphics!.battle).toBeUndefined()
  })
  it('removing an image takes the sets, rules and backgrounds that use it', () => {
    let d = addAssign(setAnim(addSet(doc(), 'k'), 'k', 'hurt', { frames: framesFromRow(2, 2) }), 'k', 'knight')
    d = setBackgroundPicture(addBackground(d, 'meadow'), 'meadow', 'bg', [0, 0])
    const noBg = removeSheet(d, 'bg')
    expect(backgroundList(noBg)).toEqual([])
    expect(setList(noBg)).toHaveLength(1)
    const noFighters = removeSheet(d, 'fighters')
    expect(setList(noFighters)).toEqual([])   // the stand went with the image, so the set and its rule went
    expect(assignList(noFighters)).toEqual([])
    expect(backgroundList(noFighters)).toHaveLength(1)
    // a set that only loses a non-stand animation stays
    d = setAnim(d, 'k', 'hurt', { sheet: 'bg', frames: [[0, 0]] })
    expect(Object.keys(setList(removeSheet(d, 'bg'))[0].set.animations)).toEqual(['stand'])
  })
})

describe('checks', () => {
  const base = () => addAssign(setAnim(addSet(doc(), 'k'), 'k', 'attack', { frames: framesFromRow(1, 4), hit: 2 }), 'k', 'knight')
  it('a good set has nothing to say', () => { expect(battleChecks(base())).toEqual([]) })
  it('an attack animation with no hit frame is worth a look, not a refusal', () => {
    const d = setAnim(base(), 'k', 'crit', { frames: framesFromRow(2, 3) })
    const c = battleChecks(d)
    expect(c).toHaveLength(1)
    expect(c[0]).toMatchObject({ section: 'art', ids: ['k'], blocks: false })
    expect(c[0].text).toMatch(/no hit frame/)
  })
  it('frames outside the image block the game (when the image size is known)', () => {
    const d = setAnim(base(), 'k', 'hurt', { frames: framesFromRow(0, 6) })   // 6 across; the image is 4 pieces wide
    const c = battleChecks(d, () => ({ width: 192, height: 192 }))
    expect(c.map((x) => x.text).join('\n')).toMatch(/2 frames outside the image \(it has 4 × 4 pieces\)/)
    expect(c.every((x) => x.blocks)).toBe(true)
    expect(battleChecks(d)).toEqual([])  // size unknown: nothing to compare with
  })
  it('a set given to a class that no longer exists blocks; an unused set is only worth a look', () => {
    let d = base()
    d = { ...d, units: d.units!.filter((u) => u.id !== 'knight') }
    const c = battleChecks(d)
    expect(c).toHaveLength(1)
    expect(c[0].blocks).toBe(true)
    expect(c[0].text).toMatch(/“knight”, which no longer exists/)
    const unused = battleChecks(addSet(doc(), 'idle'))
    expect(unused).toHaveLength(1)
    expect(unused[0]).toMatchObject({ blocks: false })
    expect(unused[0].text).toMatch(/not used/)
  })
  it('names a missing character, weapon group, set and terrain choice', () => {
    let d = base()
    d = updateAssign(d, 0, { character: 'gone' })
    d = updateAssign(d, 0, { group: 'wand' })
    const text = battleChecks(d).map((x) => x.text).join('\n')
    expect(text).toMatch(/character “gone”, who no longer exists/)
    expect(text).toMatch(/weapon group “wand”, which no weapon has/)
    const withGhost = { ...d, graphics: { ...d.graphics!, battle: { ...d.graphics!.battle!, terrainBackground: { lava: 'nope' } } } }
    expect(battleChecks(withGhost).map((x) => x.text).join('\n')).toMatch(/terrain “lava”, which no longer exists/)
  })
})
