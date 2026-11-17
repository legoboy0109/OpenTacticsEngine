// The timeline's layout (columns, lanes, arrows) and the edits it makes (model/timelineLayout.ts, timelineEdit.ts).
import { describe, expect, it } from 'vitest'
import { deleteChapter } from '../../src/model/campaign'
import type { Chapter } from '../../src/model/campaignTypes'
import { buildTimeline } from '../../src/model/timelineGraph'
import { BOX_H, boxHeight, layoutTimeline, MAX_ROW_W } from '../../src/model/timelineLayout'
import { addScene, dropIndex, insertChapter, moveChapterTo, removeScene } from '../../src/model/timelineEdit'
import type { Command } from '../../src/model/storyTypes'
import { bareMap, campaignDoc, say, straight } from './timelineKit'

const ask = (q: string, answers: Array<[string, Command[]?]>): Command => ({ choice: q, answers: answers.map(([text, d]) => ({ text, ...(d ? { do: d } : {}) })) })
const lay = (chapters: Chapter[], expanded: string[] = []) => {
  const g = buildTimeline(campaignDoc(chapters))!
  return { g, l: layoutTimeline(g, new Set(expanded)) }
}
const overlap = (a: { x: number; y: number; w: number; h: number }, b: typeof a) => a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h

/** Every box inside the picture, no two boxes on each other, every path made of numbers. */
function sane(g: ReturnType<typeof lay>['g'], l: ReturnType<typeof lay>['l']): void {
  const rects = g.nodes.map((n) => l.nodes[n.id])
  rects.forEach((r, i) => {
    expect(r, `box ${g.nodes[i].id}`).toBeTruthy()
    expect(r.x).toBeGreaterThanOrEqual(0); expect(r.y).toBeGreaterThanOrEqual(0)
    expect(r.x + r.w).toBeLessThanOrEqual(l.width); expect(r.y + r.h).toBeLessThanOrEqual(l.height)
    for (let j = i + 1; j < rects.length; j++) expect(overlap(r, rects[j]), `${g.nodes[i].id} on ${g.nodes[j].id}`).toBe(false)
  })
  for (const e of g.edges) {
    const p = l.edges[e.id]
    expect(p.path.startsWith('M'), e.id).toBe(true)
    expect(p.path).not.toMatch(/NaN|undefined|Infinity/)
  }
}

describe('layout', () => {
  it('puts a straight campaign on one row, left to right in play order', () => {
    const ch = straight(3)
    ch[0].before = [say()]
    const { g, l } = lay(ch)
    sane(g, l)
    const xs = g.nodes.map((n) => l.nodes[n.id].x)
    expect(xs).toEqual([...xs].sort((a, b) => a - b))
    expect(new Set(g.nodes.map((n) => l.nodes[n.id].y)).size).toBe(1)
    expect(l.unreachedBand).toBeNull()
  })

  it('draws a goto branch on its own row; the main story stays on the first row', () => {
    const ch = straight(4)
    ch[0].after = [ask('Which road?', [['Valley', [{ goto: 'ch2' }]], ['Summit', [{ goto: 'ch3' }]]])]
    ch[1].next = 'ch4'; ch[2].next = 'ch4'
    const { g, l } = lay(ch)
    sane(g, l)
    const y = (id: string) => l.nodes[id].y
    expect(y('ch2:level')).not.toBe(y('ch3:level'))
    expect(l.nodes['ch4:level'].x).toBeGreaterThan(l.nodes['ch2:level'].x)
    expect(l.nodes['ch2:level'].x).toBe(l.nodes['ch3:level'].x) // both are one step after ch1
    expect(l.edges[g.edges.find((e) => e.label === '“Summit”')!.id].label).not.toBeNull()
  })

  it('a loop back does not move any chapter and still has a path', () => {
    const ch = straight(4)
    ch[3].after = [ask('Again?', [['Once more', [{ goto: 'ch2' }]], ['Enough']])]
    const plain = lay(straight(4))
    const { g, l } = lay(ch)
    sane(g, l)
    for (const c of g.chapters) expect(l.nodes[`${c.id}:level`].x).toBe(plain.l.nodes[`${c.id}:level`].x)
    const back = g.edges.find((e) => e.backward)!
    expect(l.edges[back.id].path.split('M').length).toBe(2)
    // a loop leaves below the row (the free band) and climbs in front of its target
    expect(l.edges[back.id].bounds.y + l.edges[back.id].bounds.h).toBeGreaterThan(l.nodes['ch4:after'].y + BOX_H)
  })

  it('a two-chapter cycle and a chapter that goes to itself lay out', () => {
    const ch = straight(2)
    ch[0].after = [{ goto: 'ch2' }]
    ch[1].after = [{ goto: 'ch1' }, { goto: 'ch2' }]
    const { g, l } = lay(ch)
    sane(g, l)
  })

  it('sets unreachable chapters apart in a band below, with their own rows', () => {
    const ch = straight(4)
    ch[0].next = 'ch2'; ch[1].next = 'ch4'
    const { g, l } = lay(ch)
    sane(g, l)
    expect(g.unreached).toEqual(['ch3'])
    expect(l.unreachedBand).not.toBeNull()
    expect(l.nodes['ch3:level'].y).toBeGreaterThan(l.nodes['ch2:level'].y + BOX_H)
    expect(l.unreachedBand!.y).toBeGreaterThan(l.nodes['ch2:level'].y + BOX_H)
    expect(l.unreachedBand!.y).toBeLessThan(l.nodes['ch3:level'].y)
    // its arrow to ch4 does not push ch4 along: ch3 stands in the first column of its own band
    expect(l.nodes['ch3:level'].x).toBe(l.nodes['ch1:level'].x)
    expect(l.nodes['ch4:level'].x).toBeGreaterThan(l.nodes['ch2:level'].x)
  })

  it('an expanded level grows with its events and groups; the row grows with it', () => {
    const events = [
      { id: 's', when: { battle: 'start' as const }, do: [] }, { id: 't', when: { turn: 2 }, do: [] }, { id: 't2', when: { turn: 3 }, do: [] },
    ]
    const doc = campaignDoc(straight(2), { a: bareMap(events) })
    const g = buildTimeline(doc)!
    const small = layoutTimeline(g)
    const big = layoutTimeline(g, new Set(['ch1']))
    expect(small.nodes['ch1:level'].h).toBe(BOX_H)
    expect(big.nodes['ch1:level'].h).toBe(boxHeight(g.nodes[0], true))
    expect(big.nodes['ch1:level'].h).toBeGreaterThan(BOX_H + 3 * 56)
    expect(big.height).toBeGreaterThan(small.height)
  })

  it('handles 40 chapters with branches quickly, with no box on another', () => {
    const ch = straight(40)
    ch[0].before = [ask('Tutorial?', [['Yes', [{ goto: 'ch40' }]], ['No']])]
    ch[39].next = 'ch2'
    for (const i of [4, 9, 14, 19, 24, 29]) ch[i].after = [ask('Which way?', [['Left', [{ goto: `ch${i + 3}` }]], ['Right']])]
    ch[34].after = [{ goto: 'ch10' }]
    ch[20].next = 'ch23' // ch22 (and the ones only it leads to) are left out
    const t = Date.now()
    const { g, l } = lay(ch)
    expect(Date.now() - t).toBeLessThan(800)
    sane(g, l)
    expect(g.unreached.length).toBeGreaterThan(0)
    expect(l.width).toBeLessThan(MAX_ROW_W + 1200) // folded into several lines, not one very long one
    expect(l.height).toBeGreaterThan(3 * BOX_H)
  })

  it('folds a long straight story into lines that still read left to right, and the arrow across the fold comes in from the left', () => {
    const { g, l } = lay(straight(40))
    sane(g, l)
    const first = l.nodes['ch1:level'], last = l.nodes['ch40:level']
    expect(last.y).toBeGreaterThan(first.y)
    expect(l.width).toBeLessThan(MAX_ROW_W + 1200)
    const ys = [...new Set(g.nodes.map((n) => l.nodes[n.id].y))]
    expect(ys.length).toBeGreaterThan(2)
    // a chapter's successor is to its right on the same line, or the first column of the next line
    for (const e of g.edges) {
      const a = l.nodes[e.from], b = l.nodes[e.to]
      if (b.y === a.y) expect(b.x).toBeGreaterThan(a.x)
      else { expect(b.y).toBeGreaterThan(a.y); expect(b.x).toBeLessThan(a.x) }
    }
  })

  it('an empty campaign lays out to nothing', () => {
    const l = layoutTimeline({ nodes: [], edges: [], chapters: [], unreached: [] })
    expect(Object.keys(l.nodes)).toEqual([])
  })
})

describe('edits', () => {
  const ids = (d: ReturnType<typeof campaignDoc>) => d.campaign!.chapters.map((c) => c.id)

  it('adds a chapter at the end, in the middle and at the start, on a sensible map', () => {
    let d = campaignDoc([{ id: 'a1', name: 'A', map: 'm1' }, { id: 'b1', name: 'B', map: 'm2' }])
    d = insertChapter(d, 'Middle', 1)
    expect(ids(d)).toEqual(['a1', 'middle', 'b1'])
    expect(d.campaign!.chapters[1]).toEqual({ id: 'middle', name: 'Middle', map: 'm1' })
    d = insertChapter(d, 'Last', 99)
    d = insertChapter(d, 'First', 0)
    expect(ids(d)).toEqual(['first', 'a1', 'middle', 'b1', 'last'])
    expect(d.campaign!.chapters[0].map).toBe('m1')
  })

  it('a new chapter takes over the "Next chapter" that pointed at its right neighbour, so the story goes through it', () => {
    const ch = straight(3)
    ch[0].next = 'ch2'
    ch[1].next = 'ch3'
    let d = insertChapter(campaignDoc(ch), 'Mid', 2) // between ch2 and ch3
    expect(d.campaign!.chapters.map((c) => [c.id, c.next])).toEqual([['ch1', 'ch2'], ['ch2', 'mid'], ['mid', undefined], ['ch3', undefined]])
    expect(buildTimeline(d)!.unreached).toEqual([])
    // a "Next chapter" that points somewhere else is left alone
    d = insertChapter(campaignDoc(straight(3).map((c, i) => (i === 0 ? { ...c, next: 'ch3' } : c))), 'Mid', 1)
    expect(d.campaign!.chapters[0].next).toBe('ch3')
  })

  it('moves a chapter and leaves the document alone when nothing moves', () => {
    const d = campaignDoc(straight(4))
    expect(ids(moveChapterTo(d, 'ch1', 2))).toEqual(['ch2', 'ch3', 'ch1', 'ch4'])
    expect(moveChapterTo(d, 'ch1', 0)).toBe(d)
    expect(moveChapterTo(d, 'nope', 1)).toBe(d)
  })

  it('adds an empty scene (saved as []) and removes a scene; neither touches the other', () => {
    let d = campaignDoc(straight(1))
    d = addScene(d, 'ch1', 'before')
    expect(d.campaign!.chapters[0].before).toEqual([])
    expect(buildTimeline(d)!.nodes.map((n) => n.id)).toEqual(['ch1:before', 'ch1:level'])
    expect(addScene(d, 'ch1', 'before')).toBe(d)
    d = addScene(d, 'ch1', 'after')
    d = removeScene(d, 'ch1', 'before')
    expect('before' in d.campaign!.chapters[0]).toBe(false)
    expect(d.campaign!.chapters[0].after).toEqual([])
    expect(removeScene(d, 'ch1', 'before')).toBe(d)
  })

  it('removing a chapter cleans every goto and "next" that named it, in one step', () => {
    const ch = straight(3)
    ch[0].after = [{ goto: 'ch3' }]
    ch[1].next = 'ch3'
    const d = deleteChapter(campaignDoc(ch), 'ch3')
    expect(ids(d)).toEqual(['ch1', 'ch2'])
    expect(d.campaign!.chapters[0].after).toEqual([])
    expect(d.campaign!.chapters[1].next).toBeUndefined()
  })

  it('works out where a dragged chapter lands: nearest chapter, left half before it, right half after it', () => {
    const ch = straight(4)
    const { l } = lay(ch)
    const order = ch.map((c) => c.id)
    const r = (id: string) => l.strips.find((s) => s.chapterId === id)!.rect
    const left = (id: string) => ({ x: r(id).x + 4, y: r(id).y + 40 })
    const right = (id: string) => ({ x: r(id).x + r(id).w - 4, y: r(id).y + 40 })
    expect(dropIndex(l.strips, order, 'ch1', right('ch3').x, right('ch3').y)).toBe(2)
    expect(dropIndex(l.strips, order, 'ch1', left('ch3').x, left('ch3').y)).toBe(1)
    expect(dropIndex(l.strips, order, 'ch4', left('ch1').x, left('ch1').y)).toBe(0)
    expect(dropIndex(l.strips, order, 'ch2', left('ch2').x, left('ch2').y)).toBe(1)
    expect(dropIndex(l.strips, order, 'ch2', right('ch1').x, right('ch1').y)).toBe(1) // just after ch1 = where it already is
    expect(dropIndex(l.strips, order, 'ch1', 100000, 100000)).toBe(3)
  })
})
