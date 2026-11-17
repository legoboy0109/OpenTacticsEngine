// The timeline's graph (model/timelineGraph.ts): boxes, arrows, branches, loops, unreachable chapters, markers, event order.
import { describe, expect, it } from 'vitest'
import type { Chapter } from '../../src/model/campaignTypes'
import { alwaysGoes } from '../../src/model/timelineFlow'
import { buildTimeline } from '../../src/model/timelineGraph'
import { orderedEvents } from '../../src/model/timelineEvents'
import { countMarkers } from '../../src/model/timelineMarkers'
import type { Command, StoryEvent } from '../../src/model/storyTypes'
import { bareMap, campaignDoc, say, straight } from './timelineKit'

const build = (chapters: Chapter[], ...rest: Parameters<typeof campaignDoc> extends [unknown, ...infer R] ? R : never) => buildTimeline(campaignDoc(chapters, ...rest))!
const arrows = (g: ReturnType<typeof build>) => g.edges.map((e) => `${e.from}>${e.to}${e.label ? ` [${e.label}]` : ''} (${e.kind})`)
const ask = (q: string, answers: Array<[string, Command[]?]>): Command => ({ choice: q, answers: answers.map(([text, d]) => ({ text, ...(d ? { do: d } : {}) })) })

describe('a straight campaign', () => {
  it('is one level per chapter, joined in list order, and the last one ends the story', () => {
    const g = build(straight(3))
    expect(g.nodes.map((n) => n.id)).toEqual(['ch1:level', 'ch2:level', 'ch3:level'])
    expect(arrows(g)).toEqual(['ch1:level>ch2:level (order)', 'ch2:level>ch3:level (order)'])
    expect(g.chapters.map((c) => c.ends)).toEqual([false, false, true])
    expect(g.unreached).toEqual([])
    expect(g.edges.filter((e) => e.between).map((e) => e.between)).toEqual([{ before: 'ch1', after: 'ch2' }, { before: 'ch2', after: 'ch3' }])
  })

  it('draws a before scene, the level and an after scene in play order, then the next chapter', () => {
    const ch = straight(2)
    ch[0].before = [say()]
    ch[0].after = [say(), say()]
    ch[1].before = [say()]
    const g = build(ch)
    expect(g.nodes.map((n) => n.id)).toEqual(['ch1:before', 'ch1:level', 'ch1:after', 'ch2:before', 'ch2:level'])
    expect(arrows(g)).toEqual(['ch1:before>ch1:level (order)', 'ch1:level>ch1:after (order)', 'ch1:after>ch2:before (order)', 'ch2:before>ch2:level (order)'])
    expect(g.nodes.find((n) => n.id === 'ch1:after')!.commandCount).toBe(2)
    expect(g.chapters[0]).toMatchObject({ head: 'ch1:before', tail: 'ch1:after' })
  })

  it('shows a chapter with no scenes as its level alone, and a scene that is only music or empty still is a box', () => {
    const ch = straight(1)
    ch[0].before = { music: 'theme' }
    ch[0].after = []
    const g = build(ch)
    expect(g.nodes.map((n) => n.id)).toEqual(['ch1:before', 'ch1:level', 'ch1:after'])
    expect(build(straight(1)).nodes.map((n) => n.id)).toEqual(['ch1:level'])
  })

  it('a chapter whose "Next chapter" is not the following one gets a "next" arrow, and the skipped chapter is unreached', () => {
    const ch = straight(3)
    ch[0].next = 'ch3'
    const g = build(ch)
    expect(arrows(g)).toContain('ch1:level>ch3:level (next)')
    expect(g.unreached).toEqual(['ch2'])
    expect(g.nodes.find((n) => n.id === 'ch2:level')!.unreached).toBe(true)
  })

  it('ignores a "Next chapter" that names nothing, without breaking', () => {
    const ch = straight(2)
    ch[0].next = 'nowhere'
    const g = build(ch)
    expect(g.edges).toEqual([])
    expect(g.unreached).toEqual(['ch2'])
  })
})

describe('branches', () => {
  // ch1 asks "Play the tutorial?" (ch2); the story goes on to ch3 either way.
  const tutorial = (): Chapter[] => {
    const ch = straight(3)
    ch[0].before = [ask('Play the tutorial?', [['Yes', [{ goto: 'ch2' }]], ['No']])]
    ch[0].next = 'ch3'
    return ch
  }

  it('a choice whose answer jumps draws a labelled arrow, and the order arrow stays because "No" falls through', () => {
    const g = build(tutorial())
    expect(arrows(g)).toContain('ch1:before>ch2:level [“Yes”] (goto)')
    expect(arrows(g)).toContain('ch1:before>ch1:level (order)')
    expect(arrows(g)).toContain('ch1:level>ch3:level (next)')
    expect(g.nodes.find((n) => n.id === 'ch1:before')!.markers.questions).toBe(1)
    expect(g.unreached).toEqual([])
  })

  it('the tutorial chapter leads on to ch3 by the plain order, and only the last chapter ends the story', () => {
    const g = build(tutorial())
    expect(g.edges.find((x) => x.from === 'ch2:level')).toMatchObject({ to: 'ch3:level', kind: 'order', backward: false })
    expect(g.chapters.map((c) => c.ends)).toEqual([false, false, true])
  })

  it('a question whose answers jump nowhere is a marker on the box, not a fork', () => {
    const ch = straight(2)
    ch[0].after = [ask('Stay?', [['Stay', [say()]], ['Go', [{ setSwitch: 'crossed', to: true }]]])]
    const g = build(ch)
    expect(g.edges.filter((e) => e.kind === 'goto')).toEqual([])
    expect(g.nodes.find((n) => n.id === 'ch1:after')!.markers).toMatchObject({ questions: 1, flags: 1 })
    expect(arrows(g)).toContain('ch1:after>ch2:level (order)')
  })

  it('a goto inside an if is labelled with a short description, and else says "otherwise"', () => {
    const ch = straight(3)
    ch[0].after = [{ if: [{ switch: 'crossed' }], then: [{ goto: 'ch3' }], else: [{ goto: 'ch2' }] }]
    const g = build(ch)
    const labels = g.edges.filter((e) => e.kind === 'goto').map((e) => e.label)
    expect(labels).toEqual(['if the switch “Crossed” is on', 'otherwise'])
    // both sides jump, so the plain order arrow is dropped
    expect(arrows(g).some((a) => a.startsWith('ch1:after>ch2:level (order)'))).toBe(false)
  })

  it('an if with only a then keeps the order arrow (the scene can fall through)', () => {
    const ch = straight(3)
    ch[0].after = [{ if: [{ switch: 'crossed' }], then: [{ goto: 'ch3' }] }]
    expect(arrows(build(ch))).toContain('ch1:after>ch2:level (order)')
  })

  it('a nested question gives one arrow labelled with both answers', () => {
    const ch = straight(3)
    ch[0].after = [ask('Which road?', [['The valley', [ask('Fast or safe?', [['Fast', [{ goto: 'ch3' }]], ['Safe', [{ goto: 'ch2' }]]])]], ['Home']])]
    const labels = build(ch).edges.filter((e) => e.kind === 'goto').map((e) => e.label)
    expect(labels).toEqual(['“The valley” › “Fast”', '“The valley” › “Safe”'])
  })

  it('a bare goto in a before scene: the chapter is reached but its level and after scene never play', () => {
    const ch = straight(3)
    ch[1].before = [say(), { goto: 'ch3' }]
    ch[1].after = [say()]
    const g = build(ch)
    expect(arrows(g)).toContain('ch2:before>ch3:level (goto)')
    expect(arrows(g).some((a) => a.startsWith('ch2:before>ch2:level'))).toBe(false)
    const n = (id: string) => g.nodes.find((x) => x.id === id)!
    expect(n('ch2:before')).toMatchObject({ unreached: false, skipped: false })
    expect(n('ch2:level').skipped).toBe(true)
    expect(n('ch2:after').skipped).toBe(true)
    expect(g.unreached).toEqual([]) // ch3 is reached through the goto
  })

  it('a goto to a chapter that does not exist draws nothing', () => {
    const ch = straight(2)
    ch[0].after = [{ goto: 'ghost' }]
    expect(build(ch).edges.filter((e) => e.kind === 'goto')).toEqual([])
  })
})

describe('loops and unreachable chapters', () => {
  it('a goto back to an earlier chapter is marked backward', () => {
    const ch = straight(4)
    ch[3].after = [ask('Again?', [['Once more', [{ goto: 'ch2' }]], ['Enough']])]
    const g = build(ch)
    const loop = g.edges.find((e) => e.kind === 'goto')!
    expect(loop).toMatchObject({ from: 'ch4:after', to: 'ch2:level', backward: true, label: '“Once more”' })
    expect(g.unreached).toEqual([])
  })

  it('a goto to its own chapter from the after scene is a backward arrow to its first box', () => {
    const ch = straight(1)
    ch[0].before = [say()]
    ch[0].after = [{ goto: 'ch1' }]
    const e = build(ch).edges.find((x) => x.kind === 'goto')!
    expect(e).toMatchObject({ from: 'ch1:after', to: 'ch1:before', backward: true })
  })

  it('chapters nothing leads to are listed as unreached, with all their boxes', () => {
    const ch = straight(4)
    ch[0].next = 'ch2'
    ch[1].next = 'ch4'
    ch[2].before = [say()]
    const g = build(ch)
    expect(g.unreached).toEqual(['ch3'])
    expect(g.nodes.filter((n) => n.unreached).map((n) => n.id)).toEqual(['ch3:before', 'ch3:level'])
  })

  it('stays fast and complete with 40 chapters and several branches', () => {
    const ch = straight(40)
    ch[0].before = [ask('Tutorial?', [['Yes', [{ goto: 'ch40' }]], ['No']])]
    ch[39].next = 'ch2'
    for (const i of [4, 9, 14, 19, 24, 29]) ch[i].after = [ask('Which way?', [['Left', [{ goto: `ch${i + 3}` }]], ['Right']])]
    ch[34].after = [{ goto: 'ch10' }] // a loop
    const t = Date.now()
    const g = build(ch)
    expect(Date.now() - t).toBeLessThan(500)
    expect(g.chapters).toHaveLength(40)
    expect(g.edges.filter((e) => e.kind === 'goto')).toHaveLength(8)
    expect(g.edges.find((e) => e.from === 'ch35:after')).toMatchObject({ backward: true })
  })
})

describe('markers and events', () => {
  const ev = (id: string, when: StoryEvent['when'], d: Command[] = []): StoryEvent => ({ id, when, do: d })

  it('counts what changes through ifs and answers', () => {
    const list: Command[] = [
      { addUnit: { type: 'x', side: 'blue', x: 0, y: 0 } as never },
      { removeUnit: { unit: 'a' } },
      { giveItem: 'salve', to: { unit: 'a' } },
      { setSwitch: 'crossed', to: true }, { setVariable: 'pack', to: 2 }, { addVariable: 'pack', by: 1 },
      { if: [{ switch: 'crossed' }], then: [{ setSwitch: 'crossed', to: false }], else: [ask('Q?', [['A', [{ addUnit: { type: 'x', side: 'blue', x: 1, y: 1 } as never }]], ['B']])] },
    ]
    expect(countMarkers(list)).toEqual({ join: 2, leave: 1, items: 1, shop: 0, flags: 4, questions: 1 })
  })

  it('orders events: start, turn numbers ascending, every turn, then conditional ones grouped by kind', () => {
    const events = [
      ev('late', { defeated: { unit: 'boss' } }), ev('t5', { turn: 5 }), ev('talk', { talk: 'a', to: 'b' }), ev('every', { turn: 'every' }),
      ev('reach', { reach: { x: 1, y: 1, w: 1, h: 1 } }), ev('t2', { turn: 2 }), ev('start', { battle: 'start' }, [{ addUnit: { type: 'x', side: 'blue', x: 0, y: 0 } as never }]),
      ev('won', { battle: 'won' }),
    ]
    const doc = campaignDoc(straight(1), { a: bareMap(events) })
    expect(orderedEvents(doc, 'ch1', 'a').map((e) => e.eventId)).toEqual(['start', 't2', 't5', 'every', 'reach', 'talk', 'late', 'won'])
    const g = buildTimeline(doc)!
    expect(g.nodes[0].events[0]).toMatchObject({ eventId: 'start', group: 'start', when: 'The battle starts' })
    expect(g.nodes[0].markers.join).toBe(1)
  })

  it('counts a shop: a placed structure whose kind sells', () => {
    const map = bareMap([], { structures: [{ type: 'market', x: 1, y: 1 }, { type: 'wall', x: 2, y: 2 }] })
    const doc = campaignDoc(straight(1), { a: map }, { structures: [{ id: 'market', name: 'Market', sells: ['salve'] }, { id: 'wall', name: 'Wall' }] as never })
    expect(buildTimeline(doc)!.nodes[0].markers.shop).toBe(1)
  })

  it('a chapter on a map the game does not have has a level box with no events', () => {
    const g = build([{ id: 'x', name: 'X', map: 'gone' }], { gone: null })
    expect(g.nodes[0]).toMatchObject({ mapKnown: true, events: [] })
    const h = buildTimeline(campaignDoc([{ id: 'x', name: 'X', map: 'nowhere' }], {}, { game: { name: 'G', maps: [] } as never }))!
    expect(h.nodes[0]).toMatchObject({ mapKnown: false, events: [] })
  })
})

describe('alwaysGoes', () => {
  it('knows when no way through a scene skips the jump', () => {
    expect(alwaysGoes([])).toBe(false)
    expect(alwaysGoes([say(), { goto: 'a' }])).toBe(true)
    expect(alwaysGoes([ask('Q?', [['A', [{ goto: 'a' }]], ['B', [{ goto: 'b' }]]])])).toBe(true)
    expect(alwaysGoes([ask('Q?', [['A', [{ goto: 'a' }]], ['B']])])).toBe(false)
    expect(alwaysGoes([{ if: [], then: [{ goto: 'a' }], else: [{ goto: 'b' }] }])).toBe(true)
    expect(alwaysGoes([{ if: [], then: [{ goto: 'a' }] }])).toBe(false)
  })
})
