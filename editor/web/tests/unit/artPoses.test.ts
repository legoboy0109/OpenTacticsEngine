import { describe, expect, it } from 'vitest'
import { artChecks } from '../../src/model/artChecks'
import { portraitSizeOf, removeSheet, setPortraitSize, setSheetUnitOffset } from '../../src/model/graphics'
import { getLook, previewAs, setLook } from '../../src/model/graphicsLooks'
import { sheetUsers } from '../../src/model/graphicsUsage'
import { renameCharacter } from '../../src/model/characterRename'
import { deleteCharacter } from '../../src/model/srpgRefs'
import { addPose, editWalk, setWalkCount, setWalkRow, walkProblems } from '../../src/model/walking'
import type { ProjectDoc } from '../../src/model/types'

const base = (): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [], styles: [{ id: 'iso', name: 'Iso', view: 'isometric' }] },
  terrain: [], structures: null, weapons: null, items: null, research: null, units: [{ id: 'knight', name: 'Knight' }], maps: {}, scripts: {},
  graphics: {
    sheets: [{ id: 'men', file: 'art/men.png', cellWidth: 16, cellHeight: 16 }, { id: 'fx', file: 'art/fx.png', cellWidth: 32, cellHeight: 32 }],
    units: { knight: { sheet: 'men', cell: [0, 0] } },
    characters: { eirik: { sheet: 'men', cell: [1, 0], facing: { downLeft: [[0, 1]], upLeft: [[0, 2]] } } },
  },
  campaign: null, audio: null, characters: [{ id: 'eirik', name: 'Eirik', type: 'knight' }], skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
} as unknown as ProjectDoc)
const dims = (f: string) => (f === 'art/men.png' ? { width: 64, height: 48 } : { width: 64, height: 64 })
const checks = (d: ProjectDoc) => artChecks(d, dims).map((c) => c.text)
const slot = { kind: 'units' as const, id: 'knight' }

describe('poses edit like walking, under their own key', () => {
  it('adds a pose, edits its rows and count, keeps other keys', () => {
    const t = { slot, key: 'attack' as const }
    let d = addPose(base(), t, 4)
    expect(getLook(d, slot)!.attack).toEqual({ downLeft: [[0, 0], [1, 0]] })
    expect(getLook(d, slot)!.walk).toBeUndefined()
    d = setWalkRow(d, t, 'upLeft', 2)
    d = setWalkCount(d, t, 3)
    expect(getLook(d, slot)!.attack).toEqual({ downLeft: [[0, 0], [1, 0], [2, 0]], upLeft: [[0, 2], [1, 2], [2, 2]] })
    d = editWalk(d, t, () => undefined)
    expect(getLook(d, slot)!.attack).toBeUndefined()
  })
  it('keys a picture change does not know survive (characters, sides, poses)', () => {
    const d = setLook(base(), slot, { sheet: 'men', cell: [2, 0], ko: { down: [[0, 0]] } })
    expect(getLook(d, slot)!.ko).toBeDefined()
    expect(d.graphics!.characters!.eirik.facing).toBeDefined()
  })
})

describe('art checks (mirror the loader)', () => {
  it('a good game has none', () => { expect(checks(base())).toEqual([]) })
  it('a character the game does not have', () => {
    const d = base()
    d.graphics!.characters!.ghost = { sheet: 'men', cell: [0, 0] }
    expect(checks(d)[0]).toMatch(/“ghost”/)
    d.graphics!.styles = { iso: { characters: { other: null } } }
    expect(checks(d).some((t) => /“other”.*Iso/.test(t))).toBe(true)
  })
  it('pose problems: unknown image, cell outside, empty, time per frame', () => {
    const d = base()
    d.graphics!.units!.knight = { sheet: 'men', cell: [0, 0], attack: { sheet: 'gone', down: [[0, 0]] }, hurt: { downLeft: [[9, 0]] }, ko: {}, facing: { down: [[0, 0]], frameMs: 5 } }
    const t = checks(d)
    expect(t.some((x) => /“attack” pictures use an image that is no longer/.test(x))).toBe(true)
    expect(t.some((x) => /outside the image \(it has 4 × 3/.test(x))).toBe(true)
    expect(t.some((x) => /at least one direction/.test(x))).toBe(true)
    expect(t.some((x) => /between 20 and 60000/.test(x))).toBe(true)
  })
  it('poses of a side picture are checked; poses on terrain or structures are refused', () => {
    const d = base()
    d.graphics!.characters!.eirik.sides = { red: { sheet: 'men', cell: [0, 0], hurt: { downLeft: [[7, 7]] } } }
    d.graphics!.structures = { keep: { sheet: 'men', cell: [0, 0], ko: { down: [[0, 0]] } } } as never
    const t = checks(d)
    expect(t.some((x) => /side “red”.*outside the image/.test(x))).toBe(true)
    expect(t.some((x) => /“keep”.*only a unit picture/.test(x))).toBe(true)
  })
  it('the diagonals of walking are checked like the others', () => {
    const d = base()
    d.graphics!.units!.knight = { sheet: 'men', cell: [0, 0], walk: { downRight: [[0, 5]] } }
    expect(walkProblems(d, d.graphics!.units!.knight, dims)[0]).toMatch(/outside the image/)
  })
  it('unit offset range', () => {
    let d = setSheetUnitOffset(base(), 'men', 4, -300)
    expect(d.graphics!.sheets[0].unitOffset).toEqual([4, -256])  // the form clamps
    d = base()
    d.graphics!.sheets[0].unitOffset = [0, 300]
    expect(checks(d)[0]).toMatch(/unit offset/)
    d.graphics!.sheets[0].unitOffset = [1.5, 0] as never
    expect(checks(d)[0]).toMatch(/unit offset/)
    expect(setSheetUnitOffset(d, 'men', 0, 0).graphics!.sheets[0].unitOffset).toBeUndefined()
  })
  it('portrait size range and shapes', () => {
    const d = base()
    for (const bad of [23, 161, 48.5, [64], [64, 200], 'big'] as unknown[]) { d.graphics!.portraitSize = bad as never; expect(checks(d)[0]).toMatch(/portrait size/) }
    for (const ok of [24, 160, [64, 80]] as unknown[]) { d.graphics!.portraitSize = ok as never; expect(checks(d)).toEqual([]) }
  })
  it('portrait size edits: a square is one number, empty takes the key away', () => {
    let d = setPortraitSize(base(), 64, 64)
    expect(d.graphics!.portraitSize).toBe(64)
    d = setPortraitSize(d, 64, 80)
    expect(d.graphics!.portraitSize).toEqual([64, 80])
    expect(portraitSizeOf(d)).toEqual([64, 80])
    expect(setPortraitSize(d, 500, 1).graphics!.portraitSize).toEqual([160, 24])
    expect(setPortraitSize(d, undefined, undefined).graphics!.portraitSize).toBeUndefined()
  })
})

describe('characters follow their owners', () => {
  it('renaming a character renames its picture; deleting it removes the picture', () => {
    const r = renameCharacter(base(), 'eirik', 'erik')
    expect(r.error).toBeNull()
    expect(Object.keys(r.doc.graphics!.characters!)).toEqual(['erik'])
    expect(deleteCharacter(base(), 'eirik').graphics!.characters).toBeUndefined()
  })
  it('removing an image takes the poses and character pictures that draw from it', () => {
    const d = base()
    d.graphics!.units!.knight = { sheet: 'men', cell: [0, 0], attack: { sheet: 'fx', down: [[0, 0]] }, sides: { red: { sheet: 'men', cell: [1, 0], ko: { sheet: 'fx', down: [[0, 0]] } } } }
    expect(sheetUsers(d, 'fx').map((u) => u.id)).toEqual(['knight'])
    const out = removeSheet(d, 'fx')
    expect(out.graphics!.units!.knight.attack).toBeUndefined()
    expect(out.graphics!.units!.knight.sides!.red.ko).toBeUndefined()
    expect(removeSheet(d, 'men').graphics!.characters).toBeUndefined()
  })
  it('a character is previewed as its unit type with its own picture; without one, as the type', () => {
    const d = base()
    const p = previewAs(d, 'characters', 'eirik')
    expect(p.id).toBe('knight')
    expect(p.doc.graphics!.units!.knight.cell).toEqual([1, 0])
    d.graphics!.characters = {}
    expect(previewAs(d, 'characters', 'eirik').doc.graphics!.units!.knight.cell).toEqual([0, 0])
    expect(previewAs(d, 'units', 'knight')).toEqual({ doc: d, id: 'knight' })
  })
})
