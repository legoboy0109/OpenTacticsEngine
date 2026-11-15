// Small pieces of the hexagon editor that are not the map: the grid labels, the wording swap, the first guess for an image, the
// art-outside check with 8 x 8 blocks, and the checks that mention a structure's size.
import { describe, expect, it } from 'vitest'
import { economyChecks } from '../../src/model/economyChecks'
import { guessDivision, hexPieceBox } from '../../src/model/gridGuess'
import { gridLabel, gridOfGame, isHex, SQUARE } from '../../src/model/gridSpec'
import { outsideUsers } from '../../src/model/graphicsUsage'
import { setRings } from '../../src/model/structures'
import { addUpgrade } from '../../src/model/structuresEconomy'
import type { ProjectDoc } from '../../src/model/types'
import { cellWords } from '../../src/ui/cellWords'
import { loadHexFixture } from './fixtures'

describe('grid of a game', () => {
  it('reads game.json and names it in plain words', () => {
    expect(gridOfGame(undefined)).toEqual(SQUARE)
    expect(gridOfGame({ grid: { type: 'square' } })).toEqual(SQUARE)
    expect(gridLabel(gridOfGame({ grid: { type: 'hex', layout: 'pointy' } }))).toBe('Hexagons, pointy-top')
    expect(gridLabel(gridOfGame({ grid: { type: 'hex', layout: 'flat' } }))).toBe('Hexagons, flat-top')
    expect(gridLabel(SQUARE)).toBe('Squares')
    expect(isHex(gridOfGame({ grid: { type: 'hex', layout: 'flat' } }))).toBe(true)
  })
})

describe('wording', () => {
  it('says hexagon for square on a hexagon game, and leaves squares alone', () => {
    expect(cellWords(true)('Click a square. Squares are 12 × 8 squares. Square 3, 4')).toBe('Click a hexagon. Hexagons are 12 × 8 hexagons. Hexagon 3, 4')
    expect(cellWords(false)('Click a square')).toBe('Click a square')
  })
})

describe('first guess for an image', () => {
  it('a hexagon game divides by the hexagon box; squares by the tile size', () => {
    const pointy = loadHexFixture('pointy').game
    const flat = loadHexFixture('flat').game
    expect(hexPieceBox(pointy)).toEqual({ w: 24, h: 28 })
    expect(hexPieceBox(flat)).toEqual({ w: 28, h: 24 })
    expect(hexPieceBox({ ...pointy!, grid: undefined })).toBeNull()
    expect(guessDivision(240, 252, 24, hexPieceBox(pointy))).toEqual({ mode: 'grid', cellWidth: 24, cellHeight: 28 })
    expect(guessDivision(240, 252, 24)).toEqual({ mode: 'grid', cellWidth: 24, cellHeight: 24 })
    expect(guessDivision(40, 40, 24, hexPieceBox(pointy)).mode).toBe('one') // small images are one picture
  })
})

describe('pictures outside the image', () => {
  const doc = (): ProjectDoc => ({ ...loadHexFixture('pointy'), graphics: { sheets: [{ id: 's', file: 'art/s.png', cellWidth: 24, cellHeight: 28 }], terrain: { road: { sheet: 's', join: [3, 0] } } } })
  it('a join block is 8 x 8 on a hexagon game, 4 x 4 on squares', () => {
    // an image 10 x 9 pieces: the 8 x 8 block at column 3 reaches column 11
    expect(outsideUsers(doc(), 's', 10, 9).map((u) => u.id)).toEqual(['road'])
    expect(outsideUsers(doc(), 's', 11, 8).map((u) => u.id)).toEqual([])
    const square = { ...doc(), game: { ...doc().game!, grid: undefined } }
    expect(outsideUsers(square, 's', 7, 4)).toEqual([])
    expect(outsideUsers(square, 's', 6, 4).map((u) => u.id)).toEqual(['road'])
  })
})

describe('upgrade size check on a hexagon game', () => {
  it('names the number of hexagons, not squares', () => {
    const d = loadHexFixture('pointy')
    const added = addUpgrade(d, 'city') // city -> factory (both one hexagon)
    const bigger = setRings(added, 'factory', 1)
    const text = economyChecks(bigger).map((c) => c.text).join('\n')
    expect(text).toMatch(/Factory covers 7 hexagons but City covers 1 hexagon/)
    expect(text).not.toMatch(/squares/)
  })
})
