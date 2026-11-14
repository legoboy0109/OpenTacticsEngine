import { describe, expect, it } from 'vitest'
import { addStructure } from '../../src/model/structures'
import { convertOldProject } from '../../src/model/structuresConvert'
import { editMap } from '../../src/model/mapCtx'
import { setStructureOwner } from '../../src/model/mapStructures'
import type { MapFile, ProjectDoc } from '../../src/model/types'
import { buildBody } from '../../src/store/autosave'
import { initHistory, pushEdit, undo } from '../../src/store/history'
import { loadDemo } from './fixtures'
import { oldProject } from './oldProject'

describe('saving structures', () => {
  it('sends the list when it changed, and nothing for a list that did not load', () => {
    const a = loadDemo('demo-economy')
    const b = addStructure(a, 'Tower')
    expect(buildBody(b, a, 'r', false)!.structures).toHaveLength(4)
    expect(buildBody(a, a, 'r', false)).toBeNull()
    expect(buildBody(b, a, 'r', true)!.structures).toHaveLength(4)
  })
  it('back to "none" after the first structure was saved (undo) writes an empty list, never leaves the old file', () => {
    const none: ProjectDoc = { ...loadDemo('demo-srpg'), structures: null }
    const added = addStructure(none, 'Tower')
    expect(buildBody(none, added, 'r', false)!.structures).toEqual([])
    expect(buildBody(none, none, 'r', true)).not.toHaveProperty('structures') // never saved, still none: nothing to write
  })
  it('a map loaded with an empty owners list is saved without it; a non-empty one is kept as it is', () => {
    const a = loadDemo('demo-economy')
    const m: MapFile = { ...a.maps.twin_rivers!, owners: [] }
    const withEmpty: ProjectDoc = { ...a, maps: { twin_rivers: m } }
    // unchanged maps are not sent at all; once sent (keep mine / any edit) the empty list is gone
    expect(buildBody(withEmpty, withEmpty, 'r', false)).toBeNull()
    expect(buildBody(withEmpty, withEmpty, 'r', true)!.maps!.twin_rivers).not.toHaveProperty('owners')
    const edited = editMap(withEmpty, 'twin_rivers', (x) => setStructureOwner(x, { x: 7, y: 5 }, 'red'))
    expect(edited.maps.twin_rivers).not.toHaveProperty('owners')
    const old: ProjectDoc = { ...a, maps: { twin_rivers: { ...a.maps.twin_rivers!, owners: [{ x: 1, y: 1, side: 'blue' }] } } }
    expect(buildBody(old, a, 'r', false)!.maps!.twin_rivers!.owners).toEqual([{ x: 1, y: 1, side: 'blue' }])
  })
  it('converting is one undo step and undoing it saves the old project back exactly', () => {
    const start = oldProject()
    let h = initHistory(start)
    h = pushEdit(h, convertOldProject(h.present), undefined, 0)
    expect(h.past).toHaveLength(1)
    const converted = h.present
    h = undo(h)
    expect(h.present).toBe(start)
    const body = buildBody(h.present, converted, 'r', false)!
    expect(body.terrain).toEqual(start.terrain)
    expect(body.maps!.crossroads!.owners).toHaveLength(7)
    expect(body.maps!.crossroads).not.toHaveProperty('structures')
    expect(body.structures).toEqual([]) // the list did not exist before: an empty one
    expect(body.game).toEqual(start.game)
    expect(body.graphics).toEqual(start.graphics)
  })
})
