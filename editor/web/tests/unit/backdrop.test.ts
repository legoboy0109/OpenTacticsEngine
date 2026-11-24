import { describe, expect, it } from 'vitest'
import { addBackdrop, backdropAllowed, pieceCount, removeBackdrop, setBackdropOrigin, setBackdropOver, setBackdropSheet, setBackdropStyles } from '../../src/model/backdrop'
import { backdropChecks } from '../../src/model/backdropChecks'
import { removeSheet } from '../../src/model/graphics'
import { mapWarnings } from '../../src/model/mapChecks'
import type { MapFile, ProjectDoc } from '../../src/model/types'

const map = (extra: Record<string, unknown> = {}): MapFile => ({ name: 'M', grid: { width: 4, height: 3 }, legend: { a: 'grass' }, tiles: ['aaaa', 'aaaa', 'aaaa'], ...extra }) as MapFile
const doc = (m: MapFile = map(), game: Record<string, unknown> = {}): ProjectDoc => ({
  game: { formatVersion: 1, name: 'G', stats: [], roles: { hp: 'hp', move: 'mov' }, moveTypes: [], maps: [{ id: 'm', name: 'M', file: 'maps/m.json' }], styles: [{ id: 'iso', name: 'Iso', view: 'isometric' }, { id: 'top', name: 'Top' }], ...game },
  terrain: [], structures: null, weapons: null, items: null, research: null, units: [], maps: { m }, scripts: {},
  graphics: { sheets: [{ id: 'ground', file: 'art/ground.png', cellWidth: 16, cellHeight: 16 }, { id: 'props', file: 'art/props.png', cellWidth: 16, cellHeight: 16 }] },
  campaign: null, audio: null, characters: null, skills: null, abilities: null, conditions: null, supports: null, ranks: null, caps: null,
} as unknown as ProjectDoc)
const size = (f: string) => (f === 'art/props.png' ? { width: 64, height: 48 } : { width: 256, height: 256 })
const texts = (d: ProjectDoc) => backdropChecks(d, 'm', size).map((c) => c.text)
const withBackdrop = (b: unknown, game: Record<string, unknown> = {}) => doc(map({ backdrop: b }), game)

describe('backdrop edits', () => {
  it('adds, edits and removes; defaults leave their keys out', () => {
    let d = addBackdrop(doc(), 'm', 'ground')
    expect(d.maps.m!.backdrop).toEqual({ sheet: 'ground' })
    expect(addBackdrop(d, 'm', 'props')).toBe(d)
    d = setBackdropOrigin(d, 'm', 256, -10)
    d = setBackdropOver(d, 'm', 'props')
    d = setBackdropStyles(d, 'm', ['iso'])
    expect(d.maps.m!.backdrop).toEqual({ sheet: 'ground', origin: [256, -10], over: 'props', styles: ['iso'] })
    d = setBackdropStyles(d, 'm', [])
    expect(d.maps.m!.backdrop!.styles).toBeUndefined()
    d = setBackdropSheet(d, 'm', 'props')
    expect(d.maps.m!.backdrop!.sheet).toBe('props')
    expect(removeBackdrop(d, 'm').maps.m!.backdrop).toBeUndefined()
  })

  it('an origin of 0, 0 takes the key away; clearing the over image takes the pieces with it; pieces survive other edits', () => {
    const pieces = [{ rect: [0, 0, 16, 16], square: [1, 1] }]
    let d = withBackdrop({ sheet: 'ground', origin: [5, 5], over: 'props', pieces })
    expect(pieceCount(d.maps.m!.backdrop)).toBe(1)
    d = setBackdropOrigin(d, 'm', 0, 0)
    expect(d.maps.m!.backdrop!.origin).toBeUndefined()
    d = setBackdropSheet(d, 'm', 'props')
    expect(d.maps.m!.backdrop!.pieces).toEqual(pieces)
    d = setBackdropOver(d, 'm', '')
    expect(d.maps.m!.backdrop).toEqual({ sheet: 'props' })
  })

  it('is offered only on square games with a 2D isometric style', () => {
    expect(backdropAllowed(doc())).toBe(true)
    expect(backdropAllowed(doc(map(), { styles: [{ id: 'top', name: 'Top' }] }))).toBe(false)
    expect(backdropAllowed(doc(map(), { grid: { type: 'hex', layout: 'pointy' } }))).toBe(false)
  })

  it('removing an image takes the backdrop that uses it (or only its over picture)', () => {
    const d = withBackdrop({ sheet: 'ground', over: 'props', pieces: [{ rect: [0, 0, 8, 8], square: [0, 0] }] })
    expect(removeSheet(d, 'ground').maps.m!.backdrop).toBeUndefined()
    expect(removeSheet(d, 'props').maps.m!.backdrop).toEqual({ sheet: 'ground' })
  })

  it('keeps keys it does not know through an edit', () => {
    const d = withBackdrop({ sheet: 'ground', future: 7 })
    expect(setBackdropOrigin(d, 'm', 3, 4).maps.m!.backdrop).toEqual({ sheet: 'ground', future: 7, origin: [3, 4] })
  })
})

describe('backdrop checks (mirror loader_map_backdrop.cpp)', () => {
  it('a good backdrop has none', () => {
    expect(texts(withBackdrop({ sheet: 'ground', origin: [256, 256], over: 'props', pieces: [{ rect: [16, 8, 32, 40], square: [3, 2] }], styles: ['iso'] }))).toEqual([])
    expect(texts(doc())).toEqual([])
  })
  it('unknown image or over image', () => {
    expect(texts(withBackdrop({ sheet: 'nope' }))[0]).toMatch(/“nope”/)
    expect(texts(withBackdrop({ sheet: 'ground', over: 'zip' }))[0]).toMatch(/over picture.*“zip”/)
    expect(texts(withBackdrop({}))[0]).toMatch(/needs an image/)
  })
  it('pieces need an over image', () => {
    expect(texts(withBackdrop({ sheet: 'ground', pieces: [{ rect: [0, 0, 1, 1], square: [0, 0] }] }))[0]).toMatch(/none/)
    expect(texts(withBackdrop({ sheet: 'ground', over: '', pieces: [{ rect: [0, 0, 1, 1], square: [0, 0] }] }))[0]).toMatch(/none/)
  })
  it('a piece outside the over image (when its size is known) or the map, or malformed', () => {
    const p = (piece: unknown) => texts(withBackdrop({ sheet: 'ground', over: 'props', pieces: [piece] }))
    expect(p({ rect: [40, 0, 32, 8], square: [0, 0] })[0]).toMatch(/outside the over picture \(64 × 48/)
    expect(p({ rect: [0, 0, 64, 48], square: [0, 0] })).toEqual([])
    expect(p({ rect: [0, 0, 0, 8], square: [0, 0] })[0]).toMatch(/rect must be/)
    expect(p({ rect: [0, 0, 8, 8], square: [4, 0] })[0]).toMatch(/outside the map \(4 × 3\)/)
    expect(p({ rect: [0, 0, 8, 8], square: [0, 3] })[0]).toMatch(/outside the map/)
    expect(p({ rect: [0, 0, 8, 8] })[0]).toMatch(/both/)
    expect(p({ rect: [0, 0, 8.5, 8], square: [0, 0] })[0]).toMatch(/rect must be/)
    expect(texts(withBackdrop({ sheet: 'ground', over: 'props', pieces: [] }))[0]).toMatch(/between 1 and 4096/)
    // without an image size the rect is not compared with it
    expect(backdropChecks(withBackdrop({ sheet: 'ground', over: 'props', pieces: [{ rect: [900, 0, 32, 8], square: [0, 0] }] }), 'm')).toEqual([])
  })
  it('origin range', () => {
    expect(texts(withBackdrop({ sheet: 'ground', origin: [9000, 0] }))[0]).toMatch(/origin/)
    expect(texts(withBackdrop({ sheet: 'ground', origin: [1.5, 0] }))[0]).toMatch(/origin/)
    expect(texts(withBackdrop({ sheet: 'ground', origin: [-8192, 8192] }))).toEqual([])
  })
  it('styles: unknown, not isometric, empty', () => {
    expect(texts(withBackdrop({ sheet: 'ground', styles: ['gone'] }))[0]).toMatch(/“gone”/)
    expect(texts(withBackdrop({ sheet: 'ground', styles: ['top'] }))[0]).toMatch(/not of the 2D isometric/)
    expect(texts(withBackdrop({ sheet: 'ground', styles: [] }))[0]).toMatch(/empty/)
  })
  it('no isometric style in the game; hexagon game', () => {
    expect(texts(withBackdrop({ sheet: 'ground' }, { styles: [{ id: 'top', name: 'Top' }] }))[0]).toMatch(/no such style/)
    expect(texts(withBackdrop({ sheet: 'ground' }, { grid: { type: 'hex', layout: 'pointy' } }))[0]).toMatch(/hexagon/)
  })
  it('reaches the map warnings', () => {
    const d = withBackdrop({ sheet: 'nope' })
    expect(mapWarnings(d, d.maps.m!).some((w) => w.key.startsWith('backdrop:'))).toBe(true)
  })
})
