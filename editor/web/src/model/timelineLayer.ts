// Which column ("layer") and which row ("lane") each chapter of the timeline goes in.
//  - Layers: every chapter is one layer after the latest chapter that leads to it, over the arrows that go forward. An arrow that
//    leads back to a chapter still being walked (found by a depth-first walk from chapter 1) is a loop: it is left out here and
//    drawn as a wrap-around arrow, so loops never break the layout.
//  - Arrows from a chapter nothing leads to, and from a box that never plays, do not push the chapters they point at.
//  - Lanes: a chapter takes the lane of the chapter that leads to it by the normal order (so the main story stays on one line);
//    chapters reached only by a goto take the nearest free lane next to it. Chapters nothing leads to get their own lanes below.
import type { TimelineGraph } from './timelineTypes'

export interface Placement {
  layer: number[]
  lane: number[]
  /** Edge ids that are laid out as ordinary forward arrows; every other arrow wraps around. */
  forward: Set<string>
  /** Lanes of the reachable chapters (0..n-1); the unreached chapters' lanes follow, after one blank lane. */
  reachedLanes: number
}

export function placeChapters(g: TimelineGraph): Placement {
  const n = g.chapters.length
  const nodeById = new Map(g.nodes.map((x) => [x.id, x]))
  const unreached = new Set(g.chapters.filter((c) => c.unreached).map((c) => c.index))
  // The arrows that may decide columns: between two different chapters, from a box that plays.
  const usable = g.edges.filter((e) => e.fromChapter !== e.toChapter && !nodeById.get(e.from)?.skipped
    && !(unreached.has(e.fromChapter) && !unreached.has(e.toChapter)))
  const adj: Array<typeof usable> = Array.from({ length: n }, () => [])
  for (const e of usable) adj[e.fromChapter].push(e)

  // Depth-first walk: an arrow to a chapter still on the stack is a loop.
  const back = new Set<string>()
  const state = new Array<number>(n).fill(0) // 0 new, 1 on the stack, 2 done
  const post: number[] = []
  const walk = (start: number) => {
    const stack: Array<{ v: number; i: number }> = [{ v: start, i: 0 }]
    state[start] = 1
    while (stack.length) {
      const top = stack[stack.length - 1]
      const e = adj[top.v][top.i++]
      if (!e) { state[top.v] = 2; post.push(top.v); stack.pop(); continue }
      if (state[e.toChapter] === 1) back.add(e.id)
      else if (state[e.toChapter] === 0) { state[e.toChapter] = 1; stack.push({ v: e.toChapter, i: 0 }) }
    }
  }
  for (let v = 0; v < n; v++) if (state[v] === 0) walk(v)

  const fwd = usable.filter((e) => !back.has(e.id))
  const into: Array<typeof usable> = Array.from({ length: n }, () => [])
  for (const e of fwd) into[e.toChapter].push(e)
  const layer = new Array<number>(n).fill(0)
  for (const v of post.slice().reverse()) for (const e of adj[v]) if (!back.has(e.id)) layer[e.toChapter] = Math.max(layer[e.toChapter], layer[v] + 1)

  // Lanes, one layer at a time.
  const lane = new Array<number>(n).fill(0)
  const maxLayer = Math.max(0, ...layer)
  const spineIn = (v: number) => into[v].filter((e) => e.kind !== 'goto')
  const primary = (v: number): number | null => {
    const list = spineIn(v).length ? spineIn(v) : into[v]
    return list.length ? Math.min(...list.map((e) => e.fromChapter)) : null
  }
  let reachedLanes = 1
  const pools: Array<Map<number, Set<number>>> = [new Map(), new Map()] // per pool: layer -> taken lanes
  for (let pass = 0; pass < 2; pass++) {
    const pool = pools[pass]
    for (let l = 0; l <= maxLayer; l++) {
      const here = [...Array(n).keys()].filter((v) => layer[v] === l && (unreached.has(v) ? 1 : 0) === pass)
      here.sort((a, b) => Number(spineIn(b).length > 0) - Number(spineIn(a).length > 0) || a - b)
      const taken = pool.get(l) ?? new Set<number>()
      pool.set(l, taken)
      for (const v of here) {
        const p = primary(v)
        const pref = p === null ? 0 : lane[p]
        const cands = [pref]
        for (let k = 1; k < n + 2; k++) cands.push(pref + k, pref - k)
        const pick = cands.find((c) => c >= 0 && !taken.has(c)) ?? 0
        taken.add(pick)
        lane[v] = pick
      }
    }
    if (pass === 0) reachedLanes = Math.max(1, ...g.chapters.filter((c) => !c.unreached).map((c) => lane[c.index] + 1))
    // The unreached chapters' lanes start after the reachable ones (shifted below), one lane per chain.
  }
  for (const v of unreached) lane[v] += reachedLanes
  return { layer, lane, forward: new Set(fwd.map((e) => e.id)), reachedLanes }
}
