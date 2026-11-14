import { describe, expect, it } from 'vitest'
import { artNameFor, displayStem, pngSize, tooBig } from '../../src/model/artFiles'
import { addSheet, newSheetId, removeSheet, setSheetGrid, setSheetName, setSheetSmooth, setUnitMarker, unitMarkerOn, updateSheet } from '../../src/model/graphics'
import { copyArtOf, dropArtOf, dropStyleArt } from '../../src/model/graphicsClean'
import { effectiveLook, emptyPick, getLook, lookFromPick, pickFromLook, setLook, setSideLook, sideLook, stylesWithOverride, type Pick } from '../../src/model/graphicsLooks'
import { gameSides, outsideUsers, sheetUsers, unusedFiles } from '../../src/model/graphicsUsage'
import { gridInfo, guessDivision, isOnePicture } from '../../src/model/gridGuess'
import { findUsages } from '../../src/model/references'
import { deleteTerrain, duplicateTerrain } from '../../src/model/terrain'
import { deleteUnit, duplicateUnit } from '../../src/model/units'
import { removeStyle, addStyle } from '../../src/model/game'
import type { Graphics, Look, ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

const base = (): ProjectDoc => {
  const d = loadTemplate('strategy-rpg')
  return { ...d, graphics: null }
}
const withArt = (g: Graphics): ProjectDoc => ({ ...base(), graphics: g })

describe('images', () => {
  it('the first art edit creates the section; ids come from the name and stay unique', () => {
    let d = base()
    expect(d.graphics).toBeNull()
    d = addSheet(d, { file: 'art/tiles.png', name: 'Tiles', cellWidth: 24, cellHeight: 24 })
    expect(d.graphics).toEqual({ sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: 24, cellHeight: 24, name: 'Tiles' }] })
    d = addSheet(d, { file: 'art/tiles_2.png', name: 'Tiles', cellWidth: 16, cellHeight: 16 })
    expect(d.graphics!.sheets.map((s) => s.id)).toEqual(['tiles', 'tiles_2'])
    expect(newSheetId(d, '')).toBe('image')
  })
  it('rename and grid edits touch only that image; an empty name leaves the key', () => {
    let d = addSheet(base(), { file: 'art/a.png', name: 'A', cellWidth: 24, cellHeight: 24 })
    d = addSheet(d, { file: 'art/b.png', cellWidth: 8, cellHeight: 8 })
    const e = setSheetName(setSheetGrid(d, 'a', 16.4, 0), 'a', '  ')
    expect(e.graphics!.sheets[0]).toEqual({ id: 'a', file: 'art/a.png', cellWidth: 16, cellHeight: 1 })
    expect(e.graphics!.sheets[1]).toBe(d.graphics!.sheets[1])
    expect(updateSheet(d, 'missing', { name: 'x' })).toBe(d)
    expect(updateSheet(d, 'a', { name: 'New' }).graphics!.sheets[0].name).toBe('New')
  })
  it('smooth is off by default; turning it on writes true, off removes the key and other keys stay', () => {
    let d = addSheet(base(), { file: 'art/a.png', name: 'A', cellWidth: 24, cellHeight: 24 })
    expect(d.graphics!.sheets[0].smooth).toBeUndefined()
    d = setSheetSmooth(d, 'a', true)
    expect(d.graphics!.sheets[0]).toEqual({ id: 'a', file: 'art/a.png', cellWidth: 24, cellHeight: 24, name: 'A', smooth: true })
    d = setSheetGrid(d, 'a', 12, 12)
    expect(d.graphics!.sheets[0].smooth).toBe(true)
    d = setSheetSmooth(d, 'a', false)
    expect('smooth' in d.graphics!.sheets[0]).toBe(false)
  })
  it('the unit marker defaults on; turning it off writes false, on removes the key', () => {
    let d = base()
    expect(unitMarkerOn(d)).toBe(true)
    d = setUnitMarker(d, false)
    expect(d.graphics).toEqual({ sheets: [], unitMarker: false })
    expect(unitMarkerOn(d)).toBe(false)
    expect(setUnitMarker(d, true).graphics).toEqual({ sheets: [] })
  })
})

describe('first guess for dividing an image', () => {
  it('at most twice the tile size both ways is one picture', () => {
    expect(guessDivision(24, 24, 24)).toEqual({ mode: 'one', cellWidth: 24, cellHeight: 24 })
    expect(guessDivision(48, 40, 24)).toEqual({ mode: 'one', cellWidth: 48, cellHeight: 40 })
    expect(guessDivision(49, 24, 24).mode).toBe('grid')
  })
  it('a grid uses the tile size when it divides, else 16, 32, 24, 48 in that order, else the tile size', () => {
    expect(guessDivision(96, 48 * 2, 24)).toEqual({ mode: 'grid', cellWidth: 24, cellHeight: 24 })
    expect(guessDivision(128, 64, 24)).toEqual({ mode: 'grid', cellWidth: 16, cellHeight: 16 })
    expect(guessDivision(96, 96, 32)).toEqual({ mode: 'grid', cellWidth: 32, cellHeight: 32 })
    expect(guessDivision(240, 144, 20)).toEqual({ mode: 'grid', cellWidth: 16, cellHeight: 16 })
    expect(guessDivision(144, 96, 20).cellWidth).toBe(16) // 16 divides 144 and 96
    expect(guessDivision(150, 100, 20)).toEqual({ mode: 'grid', cellWidth: 20, cellHeight: 20 }) // tile size 20 divides neither -> 150 % 20 != 0
    expect(guessDivision(101, 103, 24)).toEqual({ mode: 'grid', cellWidth: 24, cellHeight: 24 })
  })
  it('grid info counts whole pieces and the pixels left over', () => {
    expect(gridInfo(100, 50, 24, 24)).toEqual({ cols: 4, rows: 2, extraRight: 4, extraBottom: 2 })
    expect(gridInfo(96, 48, 24, 24)).toEqual({ cols: 4, rows: 2, extraRight: 0, extraBottom: 0 })
    expect(gridInfo(10, 10, 24, 24).cols).toBe(0)
    expect(isOnePicture(24, 24, 24, 24)).toBe(true)
    expect(isOnePicture(48, 24, 24, 24)).toBe(false)
  })
})

describe('file helpers', () => {
  it('names on disk are slugs of the original name, unique among existing files', () => {
    expect(artNameFor('My Tiles!.PNG', [])).toBe('my_tiles')
    expect(artNameFor('tiles.png', ['art/tiles.png'])).toBe('tiles_2')
    expect(artNameFor('tiles.jpg', ['art/tiles.png', 'art/tiles_2.png'])).toBe('tiles_3')
    expect(artNameFor('日本語.png', [])).toBe('image')
    expect(artNameFor('C:\\pics\\Knight Sprite.gif', [])).toBe('knight_sprite')
    expect(artNameFor('a'.repeat(80) + '.png', [])).toMatch(/^[a-z0-9][a-z0-9_-]{0,39}$/)
    expect(displayStem('Hero Walk.v2.png')).toBe('Hero Walk.v2')
  })
  it('reads the size from a PNG header and refuses anything else', () => {
    const h = new Uint8Array(33)
    h.set([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52, 0, 0, 0x0b, 0xb8, 0, 0, 0x04, 0xb0])
    expect(pngSize(h)).toEqual({ width: 3000, height: 1200 })
    expect(pngSize(new Uint8Array(40))).toBeNull()
    expect(pngSize(new Uint8Array(3))).toBeNull()
    expect(tooBig(3000, 1200)).toBe(true)
    expect(tooBig(2048, 2048)).toBe(false)
  })
})

describe('looks', () => {
  const sheet = (d: ProjectDoc, id: string) => addSheet(d, { file: `art/${id}.png`, cellWidth: 24, cellHeight: 24 }, id)
  const start = () => sheet(sheet(base(), 'tiles'), 'men')

  it('a base look is set and removed; null or undefined both mean "no picture"', () => {
    let d = setLook(start(), { kind: 'terrain', id: 'forest' }, { sheet: 'tiles', cell: [3, 0] })
    expect(d.graphics!.terrain).toEqual({ forest: { sheet: 'tiles', cell: [3, 0] } })
    expect(getLook(d, { kind: 'terrain', id: 'forest' })).toEqual({ sheet: 'tiles', cell: [3, 0] })
    expect(setLook(d, { kind: 'terrain', id: 'forest' }, { sheet: 'tiles', cell: [3, 0] }).graphics!.terrain).toEqual(d.graphics!.terrain)
    const gone = setLook(d, { kind: 'terrain', id: 'forest' }, null)
    expect(gone.graphics!.terrain).toBeUndefined()
    expect(setLook(gone, { kind: 'terrain', id: 'forest' }, undefined)).toBe(gone)
  })
  it('style slots: override, "no picture" (null) and "same as the others" (removed)', () => {
    let d = setLook(start(), { kind: 'terrain', id: 'plain' }, { sheet: 'tiles', cell: [0, 0] })
    const slot = { kind: 'terrain' as const, id: 'plain', style: 'night' }
    d = setLook(d, slot, { sheet: 'tiles', cell: [0, 6] })
    expect(effectiveLook(d.graphics, 'terrain', 'plain', 'night')).toEqual({ sheet: 'tiles', cell: [0, 6] })
    expect(effectiveLook(d.graphics, 'terrain', 'plain', 'day')).toEqual({ sheet: 'tiles', cell: [0, 0] }) // inherited
    expect(effectiveLook(d.graphics, 'terrain', 'plain')).toEqual({ sheet: 'tiles', cell: [0, 0] })
    d = setLook(d, slot, null)
    expect(d.graphics!.styles).toEqual({ night: { terrain: { plain: null } } })
    expect(effectiveLook(d.graphics, 'terrain', 'plain', 'night')).toBeNull()
    expect(getLook(d, slot)).toBeNull()
    expect(stylesWithOverride(d, 'terrain', 'plain')).toEqual(['night'])
    d = setLook(d, slot, undefined)
    expect(d.graphics!.styles).toBeUndefined() // empty style objects do not linger
    expect(effectiveLook(d.graphics, 'terrain', 'plain', 'night')).toEqual({ sheet: 'tiles', cell: [0, 0] })
  })
  it('per-side pictures live inside the unit entry and survive changing its own picture', () => {
    const u = { kind: 'units' as const, id: 'knight' }
    let d = setLook(start(), u, { sheet: 'men', cell: [0, 0] })
    d = setSideLook(d, u, 'red', { sheet: 'men', cell: [0, 3] })
    expect(d.graphics!.units!.knight).toEqual({ sheet: 'men', cell: [0, 0], sides: { red: { sheet: 'men', cell: [0, 3] } } })
    d = setLook(d, u, { sheet: 'men', frames: [[0, 0], [1, 0]], frameMs: 200 })
    expect(d.graphics!.units!.knight.sides).toEqual({ red: { sheet: 'men', cell: [0, 3] } })
    expect(sideLook(getLook(d, u), 'red')).toEqual({ sheet: 'men', cell: [0, 3] })
    expect(sideLook(getLook(d, u), 'blue')).toEqual(getLook(d, u))
    d = setSideLook(d, u, 'red', undefined)
    expect('sides' in d.graphics!.units!.knight).toBe(false)
    // no entry for the unit: nothing to hang a side picture on
    expect(setSideLook(start(), u, 'red', { sheet: 'men', cell: [0, 0] })).toEqual(start())
  })
  it('the picker choice becomes the right look for each kind', () => {
    const p = (o: Partial<Pick>): Pick => ({ ...emptyPick('tiles'), ...o })
    expect(lookFromPick(p({}))).toBeNull()
    expect(lookFromPick(p({ kind: 'one', cells: [[2, 1]] }))).toEqual({ sheet: 'tiles', cell: [2, 1] })
    expect(lookFromPick(p({ kind: 'mix', cells: [[0, 0], [1, 0]] }))).toEqual({ sheet: 'tiles', variants: [[0, 0], [1, 0]] })
    expect(lookFromPick(p({ kind: 'mix', cells: [[5, 5]] }))).toEqual({ sheet: 'tiles', cell: [5, 5] })
    expect(lookFromPick(p({ kind: 'animation', cells: [[0, 1], [1, 1]], frameMs: 700 }))).toEqual({ sheet: 'tiles', frames: [[0, 1], [1, 1]], frameMs: 700 })
    expect(lookFromPick(p({ kind: 'animation', cells: [[0, 1]] }))).toEqual({ sheet: 'tiles', cell: [0, 1] })
    expect(lookFromPick(p({ kind: 'join', cells: [[0, 2], [9, 9]] }))).toEqual({ sheet: 'tiles', join: [[0, 2], [9, 9]], frameMs: 400 })
    expect(lookFromPick(p({ kind: 'join', cells: [[0, 2]], joinWith: ['bridge'], joinEdge: false }))).toEqual({ sheet: 'tiles', join: [0, 2], joinWith: ['bridge'], joinEdge: false })
    expect(lookFromPick(p({ kind: 'one', cells: [[1, 1], [2, 2]] }))).toEqual({ sheet: 'tiles', cell: [1, 1] })
  })
  it('a look read back gives the same choice (round trip)', () => {
    const looks: Look[] = [
      { sheet: 'tiles', cell: [1, 2] }, { sheet: 'tiles', variants: [[0, 0], [1, 0], [2, 0]] },
      { sheet: 'tiles', frames: [[0, 1], [1, 1]], frameMs: 500 }, { sheet: 'tiles', join: [0, 2], joinWith: ['bridge'], joinEdge: false },
    ]
    for (const l of looks) expect(lookFromPick(pickFromLook(l, 'x'))).toEqual(l)
    expect(pickFromLook(null, 'tiles')).toEqual(emptyPick('tiles'))
    expect(pickFromLook({ sheet: 'tiles', frames: [[0, 0]] }, 'x').kind).toBe('one')
  })
})

describe('removing an image', () => {
  it('clears every picture that used it, in the base, per side and in looks, and nothing else', () => {
    const g: Graphics = {
      sheets: [{ id: 'a', file: 'art/a.png', cellWidth: 24, cellHeight: 24 }, { id: 'b', file: 'art/b.png', cellWidth: 24, cellHeight: 24 }],
      terrain: { plain: { sheet: 'a', cell: [0, 0] }, forest: { sheet: 'b', cell: [0, 0] } },
      units: {
        knight: { sheet: 'b', cell: [0, 0], sides: { red: { sheet: 'a', cell: [0, 1] }, blue: { sheet: 'b', cell: [0, 2] } } },
        archer: { sheet: 'a', cell: [0, 0], sides: { red: { sheet: 'b', cell: [0, 0] } } },
      },
      styles: { night: { terrain: { plain: { sheet: 'a', cell: [1, 0] }, forest: null }, units: { knight: { sheet: 'a', cell: [2, 0] } } } },
    }
    const d = removeSheet(withArt(g), 'a')
    expect(d.graphics).toEqual({
      sheets: [g.sheets[1]],
      terrain: { forest: { sheet: 'b', cell: [0, 0] } },
      units: { knight: { sheet: 'b', cell: [0, 0], sides: { blue: { sheet: 'b', cell: [0, 2] } } } },
      styles: { night: { terrain: { forest: null } } },
    })
    expect(removeSheet(withArt(g), 'zzz').graphics).toEqual(g)
    expect(removeSheet(base(), 'a').graphics).toBeNull()
    const last = removeSheet(removeSheet(withArt(g), 'a'), 'b')
    expect(last.graphics).toEqual({ sheets: [], styles: { night: { terrain: { forest: null } } } })
  })
})

describe('used by, unused files and pieces outside the image', () => {
  const g: Graphics = {
    sheets: [{ id: 'a', file: 'art/a.png', cellWidth: 24, cellHeight: 24 }, { id: 'b', file: 'art/b.png', cellWidth: 24, cellHeight: 24 }],
    terrain: { plain: { sheet: 'a', cell: [0, 0] }, road: { sheet: 'a', join: [0, 2] } },
    units: { lord: { sheet: 'b', cell: [0, 0], sides: { red: { sheet: 'a', cell: [3, 3] } } } },
    styles: { night: { terrain: { forest: { sheet: 'a', cell: [1, 1] }, plain: null } } },
  }
  it('lists each terrain and unit once, terrain first', () => {
    const d = withArt(g)
    expect(sheetUsers(d, 'a').map((u) => `${u.kind}:${u.id}:${u.name}`)).toEqual(['terrain:plain:Plain', 'terrain:road:road', 'terrain:forest:Forest', 'units:lord:Lord'])
    expect(sheetUsers(d, 'b').map((u) => u.id)).toEqual(['lord'])
    expect(sheetUsers(d, 'zzz')).toEqual([])
    expect(sheetUsers(base(), 'a')).toEqual([])
  })
  it('files nobody uses', () => {
    const files = [{ file: 'art/a.png' }, { file: 'art/b.png' }, { file: 'art/c.png' }]
    expect(unusedFiles(withArt(g), files)).toEqual(['art/c.png'])
    expect(unusedFiles(base(), files)).toEqual(['art/a.png', 'art/b.png', 'art/c.png'])
    expect(unusedFiles(removeSheet(withArt(g), 'a'), files)).toEqual(['art/a.png', 'art/c.png'])
  })
  it('finds things whose pieces fall outside a smaller image (join blocks are 4 x 4)', () => {
    const d = withArt(g)
    expect(outsideUsers(d, 'a', 4, 6).map((u) => u.id)).toEqual([])
    expect(outsideUsers(d, 'a', 4, 5).map((u) => u.id)).toEqual(['road']) // the block at row 2 needs rows 2..5
    expect(outsideUsers(d, 'a', 3, 6).map((u) => u.id)).toEqual(['road', 'lord']) // road: 4 columns; lord's red picture is in column 3
    expect(outsideUsers(d, 'b', 1, 1).map((u) => u.id)).toEqual([])
  })
  it('sides come from the maps; ids that only still have pictures are marked', () => {
    const d = setLook(setLook(base(), { kind: 'units', id: 'lord' }, { sheet: 'b', cell: [0, 0] }), { kind: 'units', id: 'lord' }, { sheet: 'b', cell: [0, 0], sides: { gone: { sheet: 'b', cell: [0, 1] } } })
    const sides = gameSides(d)
    expect(sides.slice(0, 2).every((s) => !s.orphan && !!s.name)).toBe(true)
    expect(sides[sides.length - 1]).toEqual({ id: 'gone', name: 'gone', orphan: true })
    expect(new Set(sides.map((s) => s.id)).size).toBe(sides.length)
  })
})

describe('art follows its terrain, unit and visual style', () => {
  const g: Graphics = {
    sheets: [{ id: 't', file: 'art/t.png', cellWidth: 24, cellHeight: 24 }],
    terrain: { plain: { sheet: 't', cell: [0, 0] }, road: { sheet: 't', join: [0, 2], joinWith: ['plain', 'forest'] }, forest: { sheet: 't', cell: [1, 0] } },
    units: { lord: { sheet: 't', cell: [0, 5], sides: { red: { sheet: 't', cell: [1, 5] } } } },
    styles: {
      night: { terrain: { plain: { sheet: 't', cell: [0, 6] }, road: { sheet: 't', join: [0, 2], joinWith: ['plain'] } }, units: { lord: { sheet: 't', cell: [0, 7] } } },
      day: { terrain: { forest: null } },
    },
  }
  const d0 = (): ProjectDoc => {
    const d = withArt(g)
    return { ...d, terrain: ['plain', 'road', 'forest'].map((id) => d.terrain?.find((t) => t.id === id) ?? { id, name: id }) }
  }
  it('deleting a terrain removes its pictures everywhere and takes its id out of other joinWith lists', () => {
    const d = deleteTerrain(d0(), 'plain')
    expect(d.graphics!.terrain).toEqual({ road: { sheet: 't', join: [0, 2], joinWith: ['forest'] }, forest: { sheet: 't', cell: [1, 0] } })
    expect(d.graphics!.styles).toEqual({ night: { terrain: { road: { sheet: 't', join: [0, 2] } }, units: { lord: { sheet: 't', cell: [0, 7] } } }, day: { terrain: { forest: null } } })
    const e = deleteTerrain(deleteTerrain(d0(), 'road'), 'forest')
    expect(e.graphics!.terrain).toEqual({ plain: { sheet: 't', cell: [0, 0] } })
    expect(e.graphics!.styles).toEqual({ night: { terrain: { plain: { sheet: 't', cell: [0, 6] } }, units: { lord: { sheet: 't', cell: [0, 7] } } } })
  })
  it('deleting a unit removes its picture, sides and look overrides', () => {
    const d = deleteUnit({ ...d0(), units: [...d0().units!] }, 'lord')
    expect(d.graphics!.units).toBeUndefined()
    expect(d.graphics!.styles!.night.units).toBeUndefined()
    expect(d.graphics!.terrain).toEqual(g.terrain)
  })
  it('a unit or terrain without art is left alone (same object)', () => {
    const d = base()
    expect(dropArtOf(d, 'terrain', 'plain')).toBe(d)
    expect(dropArtOf(d0(), 'units', 'nobody')).toEqual(d0())
  })
  it('duplicating copies the base picture, per-side pictures and look overrides (as separate copies)', () => {
    const d = duplicateUnit(d0(), 'lord', 'lord_2')
    expect(d.graphics!.units!.lord_2).toEqual(g.units!.lord)
    expect(d.graphics!.units!.lord_2).not.toBe(d.graphics!.units!.lord)
    expect(d.graphics!.styles!.night.units!.lord_2).toEqual({ sheet: 't', cell: [0, 7] })
    const t = duplicateTerrain(d0(), 'forest', 'forest_2')
    expect(t.graphics!.terrain!.forest_2).toEqual({ sheet: 't', cell: [1, 0] })
    expect(t.graphics!.styles!.day.terrain!.forest_2).toBeNull()
    expect(copyArtOf(base(), 'units', 'a', 'b').graphics).toBeNull()
  })
  it('removing a visual style drops its picture overrides', () => {
    const d = addStyle(addStyle(d0(), 'Night', 'night'), 'Day', 'day')
    const e = removeStyle(d, 'night')
    expect(Object.keys(e.graphics!.styles!)).toEqual(['day'])
    expect(e.game!.styles!.map((s) => s.id)).toEqual(['classic', 'isometric', 'day']) // the template's own styles stay
    expect(dropStyleArt(removeStyle(e, 'day'), 'day').graphics!.styles).toBeUndefined()
  })
  it('the delete confirmation mentions the picture only when there is one', () => {
    expect(findUsages(d0(), 'terrain', 'forest').some((u) => u.section === 'art')).toBe(true)
    expect(findUsages(base(), 'terrain', 'forest').some((u) => u.section === 'art')).toBe(false)
    expect(findUsages(d0(), 'unit', 'lord').some((u) => u.section === 'art')).toBe(true)
  })
})
