import { describe, expect, it } from 'vitest'
import { getLook, setLook } from '../../src/model/graphicsLooks'
import {
  isModelEntry, modelBase, modelFields, modelFile, modelNameFor, readGlbFacts, setAnimation, setModelFile, setModelHeight,
  setModelTurn, setSideMaterial,
} from '../../src/model/models3d'
import type { ProjectDoc } from '../../src/model/types'
import { loadTemplate } from './fixtures'

function glb(json: object, version = 2): Uint8Array {
  let text = JSON.stringify(json)
  while (text.length % 4) text += ' '
  const body = new TextEncoder().encode(text)
  const out = new Uint8Array(20 + body.length)
  const v = new DataView(out.buffer)
  v.setUint32(0, 0x46546c67, true); v.setUint32(4, version, true); v.setUint32(8, out.length, true)
  v.setUint32(12, body.length, true); v.setUint32(16, 0x4e4f534a, true)
  out.set(body, 20)
  return out
}

const doc = (): ProjectDoc => ({ ...loadTemplate('blank'), graphics: null })
const slot = { kind: 'units' as const, id: 'soldier', style: 'solid' }

describe('reading a .glb', () => {
  it('lists animation and material names, skipping unnamed and repeated ones', () => {
    const f = readGlbFacts(glb({ animations: [{ name: 'Walk' }, {}, { name: 'Walk' }, { name: 'Hit' }], materials: [{ name: 'side' }, { name: 'metal' }] }))
    expect(f).toEqual({ animations: ['Walk', 'Hit'], materials: ['side', 'metal'] })
  })
  it('a file with neither has empty lists', () => expect(readGlbFacts(glb({ asset: { version: '2.0' } }))).toEqual({ animations: [], materials: [] }))
  it('refuses anything that is not a version 2 glTF binary', () => {
    expect(readGlbFacts(new TextEncoder().encode('not a model, just text, long enough'))).toBeNull()
    expect(readGlbFacts(glb({}, 1))).toBeNull()
    expect(readGlbFacts(glb({}).subarray(0, 10))).toBeNull()
    expect(readGlbFacts(glb({ asset: { version: '2.0' } }).subarray(0, 30))).toBeNull() // JSON chunk cut short
  })
})

describe('file names', () => {
  it('base and unique upload names', () => {
    expect(modelBase('models/knight.glb')).toBe('knight')
    expect(modelNameFor('My Knight.GLB', ['models/my_knight.glb'])).toBe('my_knight_2')
  })
})

describe('editing an entry', () => {
  it('choosing a file writes only { model }', () => {
    const d = setModelFile(doc(), slot, 'models/k.glb')
    expect(d.graphics!.styles!.solid.units).toEqual({ soldier: { model: 'models/k.glb' } })
    expect(modelFile(getLook(d, slot))).toBe('models/k.glb')
  })
  it('defaults are not written and clear what was there', () => {
    let d = setModelFile(doc(), slot, 'models/k.glb')
    d = setModelHeight(d, slot, 1.5)
    d = setModelTurn(d, slot, 90)
    d = setSideMaterial(d, slot, 'cloth')
    d = setAnimation(d, slot, 'move', 'Run')
    d = setAnimation(d, slot, 'idle', '')
    expect(getLook(d, slot)).toEqual({ model: 'models/k.glb', height: 1.5, turn: 90, sideMaterial: 'cloth', animations: { move: 'Run', idle: '' } })
    d = setModelHeight(d, slot, undefined)
    d = setModelTurn(d, slot, 0)
    d = setSideMaterial(d, slot, 'side')
    d = setAnimation(d, slot, 'move', 'Move')
    d = setAnimation(d, slot, 'idle', 'Idle')
    expect(getLook(d, slot)).toEqual({ model: 'models/k.glb' })
  })
  it('none is written as an empty name', () => {
    const d = setSideMaterial(setModelFile(doc(), slot, 'models/k.glb'), slot, '')
    expect(getLook(d, slot)).toEqual({ model: 'models/k.glb', sideMaterial: '' })
  })
  it('unknown keys survive, and another file keeps the other settings', () => {
    let d = setModelFile(doc(), slot, 'models/k.glb')
    d = { ...d, graphics: { ...d.graphics!, styles: { solid: { units: { soldier: { model: 'models/k.glb', height: 2, future: { a: 1 } } as never } } } } }
    d = setModelTurn(d, slot, 45)
    d = setModelFile(d, slot, 'models/other.glb')
    expect(getLook(d, slot)).toEqual({ model: 'models/other.glb', height: 2, future: { a: 1 }, turn: 45 })
  })
  it('a setting does nothing without a model entry; same value returns the same document', () => {
    const d = doc()
    expect(setModelHeight(d, slot, 2)).toBe(d)
    const m = setModelFile(d, slot, 'models/k.glb')
    expect(setModelFile(m, slot, 'models/k.glb')).toBe(m)
    expect(setModelTurn(m, slot, 0)).toBe(m)
  })
  it('replaces a picture entry without carrying its side pictures', () => {
    let d = setLook(doc(), slot, { sheet: 's', cell: [0, 0], sides: { red: { sheet: 's', cell: [1, 0] } } })
    expect(isModelEntry(getLook(d, slot))).toBe(false)
    d = setModelFile(d, slot, 'models/k.glb')
    expect(getLook(d, slot)).toEqual({ model: 'models/k.glb' })
  })
  it('going back to a picture removes the entry and tidies empty parents', () => {
    let d = setModelFile(doc(), slot, 'models/k.glb')
    d = setLook(d, slot, undefined)
    expect(getLook(d, slot)).toBeUndefined()
    expect(d.graphics!.styles).toBeUndefined()
  })
  it('works for structures and keeps units untouched', () => {
    let d = setModelFile(doc(), slot, 'models/k.glb')
    d = setModelFile(d, { kind: 'structures', id: 'fort', style: 'solid' }, 'models/f.glb')
    expect(d.graphics!.styles!.solid).toEqual({ units: { soldier: { model: 'models/k.glb' } }, structures: { fort: { model: 'models/f.glb' } } })
  })
})

describe('fields', () => {
  it('show the stored value, else the default', () => {
    expect(modelFields({ model: 'x.glb', turn: 30, animations: { die: 'Fall' } } as never)).toEqual({
      height: undefined, turn: 30, sideMaterial: 'side', animations: { idle: 'Idle', move: 'Move', attack: 'Attack', die: 'Fall' },
    })
  })
})
