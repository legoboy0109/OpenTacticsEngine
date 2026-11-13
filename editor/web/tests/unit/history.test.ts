import { describe, expect, it } from 'vitest'
import { COALESCE_MS, initHistory, pushEdit, undo } from '../../src/store/history'

describe('stroke keys', () => {
  it('edits of one stroke merge however long the pointer rests; a new stroke is a new step', () => {
    let h = initHistory(0)
    h = pushEdit(h, 1, 'stroke:1', 0)
    h = pushEdit(h, 2, 'stroke:1', COALESCE_MS * 10) // long pause mid-drag
    h = pushEdit(h, 3, 'stroke:2', COALESCE_MS * 10 + 5)
    expect(h.past).toEqual([0, 2])
    expect(undo(h).present).toBe(2)
  })
  it('ordinary keys (typing) still stop merging after the window', () => {
    let h = initHistory(0)
    h = pushEdit(h, 1, 'name', 0)
    h = pushEdit(h, 2, 'name', COALESCE_MS + 1)
    expect(h.past).toEqual([0, 1])
  })
})
