// The campaign timeline (docs/EDITOR_UI.md "Campaign timeline"): the whole story as boxes and arrows you can move around, zoom,
// rearrange and jump into. Everything it changes goes through the document's `edit`, like the list view, so Undo, "Unsaved
// changes" and the Problems list keep working. Only the boxes near the screen are in the page.
import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { deleteChapter, newChapterId } from '../../../model/campaign'
import { campaignUsagesOf } from '../../../model/campaignRefs'
import type { SceneKey } from '../../../model/campaignTypes'
import { buildTimeline } from '../../../model/timelineGraph'
import { addScene, dropIndex, dropSpot, insertChapter, moveChapterTo, removeScene } from '../../../model/timelineEdit'
import { layoutTimeline } from '../../../model/timelineLayout'
import { timelineProblems } from '../../../model/timelineProblems'
import type { Rect, TimelineNode } from '../../../model/timelineTypes'
import { navigate, projectPath } from '../../../router'
import { openProblemsList } from '../../../shell/openProblems'
import { useDoc, useEdit, useProject, useStore } from '../../../store/context'
import { requestOpenEvent } from '../../maps/editor/openRequest'
import { BoxNode, type BoxActions } from './BoxNode'
import { EdgesLayer } from './EdgesLayer'
import { Legend } from './Legend'
import { memoryOf } from './memory'
import { mapThumbnail } from './mapThumb'
import { TimelineBar, type SelectedActions } from './TimelineBar'
import { TimelineDialogs, type Asking } from './TimelineDialogs'
import { TL } from './text'
import { useGestures } from './useGestures'
import { fitView, revealRect, usePanZoom } from './usePanZoom'
import { neighbourBox } from '../../../model/timelineNeighbour'

const NEAR = 240 // world pixels around the screen that are still drawn

const hits = (a: Rect, b: Rect) => a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h

export function TimelineView() {
  const doc = useDoc()
  const { problems } = useProject()
  const edit = useEdit()
  const store = useStore()
  const mem = memoryOf(store.id)
  const graph = useMemo(() => buildTimeline(doc), [doc])
  const [expanded, setExpanded] = useState<Set<string>>(() => new Set(mem.expanded))
  const layout = useMemo(() => (graph ? layoutTimeline(graph, expanded) : null), [graph, expanded])
  const box = useMemo(() => (graph ? timelineProblems(doc, problems, graph) : null), [doc, problems, graph])
  const pz = usePanZoom(mem)
  const { view, size } = pz
  const [selectedId, setSelectedId] = useState<string | null>(mem.selected)
  const [focusId, setFocusId] = useState<string | null>(mem.returnTo)
  const [asking, setAsking] = useState<Asking | null>(null)
  const [helpOpen, setHelpOpen] = useState(false)
  const pendingFocus = useRef<string | null>(mem.returnTo)
  const pendingReveal = useRef<string | null>(null)

  const names = useMemo(() => {
    const m = new Map<string, string>()
    for (const n of graph?.nodes ?? []) m.set(n.id, n.kind === 'level' ? `${n.chapterName}` : `${n.chapterName} (${n.which === 'before' ? TL.sceneBeforeTitle : TL.sceneAfterTitle})`)
    return m
  }, [graph])
  const selected = graph?.chapters.find((c) => c.id === selectedId) ?? null
  const order = useMemo(() => graph?.chapters.map((c) => c.id) ?? [], [graph])

  const select = useCallback((id: string | null) => { mem.selected = id; setSelectedId(id) }, [mem])

  // First time: fit the whole story (not smaller than 55 %: past that the boxes cannot be read, the Fit button still shows all).
  useEffect(() => {
    if (!layout || mem.placed || size.w === 0) return
    mem.placed = true
    const f = fitView(size, layout)
    pz.set(f.k >= 0.55 ? f : { k: 0.8, x: -50, y: 12 })
  }, [layout, size, mem, pz])

  /** Brings a rectangle into view (after a keyboard move, or a chapter was added or moved). */
  const reveal = useCallback((r: Rect) => pz.set(revealRect(pz.ref.current, pz.size, r)), [pz])
  useEffect(() => {
    const id = pendingReveal.current
    if (!id || !layout || size.w === 0) return
    const s = layout.strips.find((x) => x.chapterId === id)
    if (s) { pendingReveal.current = null; reveal(s.rect) }
  }, [layout, size, reveal])

  // Keyboard focus follows `focusId` once its box is on the page.
  useEffect(() => {
    if (!pendingFocus.current || !graph) return
    const el = pz.viewportRef.current?.querySelector<HTMLElement>(`[data-node-open="${CSS.escape(pendingFocus.current)}"]`)
    if (el) { el.focus({ preventScroll: true }); pendingFocus.current = null }
  })
  const focusBox = useCallback((id: string) => {
    pendingFocus.current = id
    setFocusId(id)
    const r = layout?.nodes[id]
    if (r) reveal(r)
  }, [layout, reveal])
  // Back from an editor: the box that was open gets the focus back (a first visit: the first box is the tab stop).
  const stop = focusId && graph?.nodes.some((n) => n.id === focusId) ? focusId : graph?.nodes[0]?.id ?? null
  useEffect(() => { mem.returnTo = null }, [mem])

  // --- what is drawn now
  const near: Rect = useMemo(() => ({ x: -view.x / view.k - NEAR, y: -view.y / view.k - NEAR, w: size.w / view.k + 2 * NEAR, h: size.h / view.k + 2 * NEAR }), [view, size])
  const nodeVisible = (n: TimelineNode) => n.id === stop || (layout ? hits(layout.nodes[n.id], near) : false)
  const visibleEdges = useMemo(() => (graph && layout ? graph.edges.filter((e) => hits(layout.edges[e.id].bounds, near)) : []), [graph, layout, near])

  // --- doing things
  const go = useCallback((section: 'campaign' | 'maps', id?: string) => navigate(projectPath(store.id, section, id)), [store.id])
  const leave = useCallback((nodeId: string, scrollTo: SceneKey | null) => { mem.returnTo = nodeId; mem.scrollTo = scrollTo }, [mem])
  const actions: BoxActions = useMemo(() => ({
    open: (n) => {
      setFocusId(n.id)
      if (n.kind === 'scene') { leave(n.id, n.which!); go('campaign', n.chapterId) }
      else if (n.mapKnown) { leave(n.id, null); go('maps', n.mapId) }
      else { leave(n.id, null); go('campaign', n.chapterId) }
    },
    toggle: (chapterId) => setExpanded((cur) => { const next = new Set(cur); if (next.has(chapterId)) next.delete(chapterId); else next.add(chapterId); mem.expanded = next; return next }),
    openEvent: (e) => { requestOpenEvent(e.mapId, e.eventId); leave(`${e.chapterId}:level`, null); go('maps', e.mapId) },
    problems: openProblemsList,
  }), [go, leave, mem])

  const move = useCallback((id: string, to: number) => { pendingReveal.current = id; edit((d) => moveChapterTo(d, id, to)) }, [edit])
  const onDrop = useCallback((id: string, wx: number, wy: number) => {
    if (!layout) return
    const to = dropIndex(layout.strips, order, id, wx, wy)
    if (to !== order.indexOf(id)) { select(id); move(id, to) }
  }, [layout, order, move, select])
  const gestures = useGestures({ viewportRef: pz.viewportRef, viewRef: pz.ref, set: pz.set, onDrop })

  const sel: SelectedActions = {
    earlier: () => selected && move(selected.id, selected.index - 1),
    later: () => selected && move(selected.id, selected.index + 1),
    addBefore: () => selected && setAsking({ kind: 'new', index: selected.index }),
    addAfter: () => selected && setAsking({ kind: 'new', index: selected.index + 1 }),
    settings: () => { if (selected) { leave(selected.head, null); go('campaign', selected.id) } },
    addScene: (which) => { if (selected) { pendingReveal.current = selected.id; edit((d) => addScene(d, selected.id, which)) } },
    removeScene: (which) => {
      if (!selected) return
      const n = graph!.nodes.find((x) => x.id === `${selected.id}:${which}`)
      if (n && n.commandCount === 0 && n.markers.questions === 0) edit((d) => removeScene(d, selected.id, which))
      else setAsking({ kind: 'scene', chapterId: selected.id, which, commands: n?.commandCount ?? 0 })
    },
    remove: () => selected && setAsking({ kind: 'delete', chapterId: selected.id }),
    clear: () => select(null),
  }

  const onKeyDown = (e: React.KeyboardEvent<HTMLDivElement>) => {
    const t = e.target as HTMLElement
    if (e.ctrlKey || e.metaKey || e.altKey) return
    const onBox = t.matches('.tl-open')
    const id = t.closest<HTMLElement>('[data-node-id]')?.dataset.nodeId
    const node = graph?.nodes.find((n) => n.id === id)
    if (e.key === '+' || e.key === '=') { e.preventDefault(); pz.zoomBy(1.25) }
    else if (e.key === '-') { e.preventDefault(); pz.zoomBy(1 / 1.25) }
    else if (e.key === '0') { e.preventDefault(); if (layout) pz.set(fitView(pz.size, layout)) }
    else if (e.key === '[' && selected) { e.preventDefault(); if (selected.index > 0) move(selected.id, selected.index - 1) }
    else if (e.key === ']' && selected) { e.preventDefault(); if (selected.index < order.length - 1) move(selected.id, selected.index + 1) }
    else if ((e.key === 'Delete' || e.key === 'Backspace') && selected && onBox) { e.preventDefault(); sel.remove() }
    else if (onBox && node && graph && layout) {
      const i = graph.nodes.indexOf(node)
      let to: string | undefined
      if (e.key === 'ArrowRight') to = graph.nodes[i + 1]?.id
      else if (e.key === 'ArrowLeft') to = graph.nodes[i - 1]?.id
      else if (e.key === 'Home') to = graph.nodes[0]?.id
      else if (e.key === 'End') to = graph.nodes[graph.nodes.length - 1]?.id
      else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') to = neighbourBox(graph, layout, node.id, e.key === 'ArrowDown' ? 1 : -1)
      else if (e.key === ' ') { e.preventDefault(); select(selectedId === node.chapterId ? null : node.chapterId); return }
      else if ((e.key === 'e' || e.key === 'E') && node.kind === 'level' && node.events.length > 0) { actions.toggle(node.chapterId); return }
      else return
      e.preventDefault()
      if (to) focusBox(to)
    }
  }
  // The tab stop follows the focus, however it got there (Tab, a click, a script). Boxes far from the screen are not in the page, and the
  // one that is the tab stop always is: without this a box focused by Tab was not it, so when a keyboard move ([ or ]) reshuffled the
  // layout for a frame the focused box could leave the page and its focus was lost.
  const onFocus = (e: React.FocusEvent<HTMLDivElement>) => {
    const id = (e.target as HTMLElement).dataset.nodeOpen
    if (id && id !== focusId) setFocusId(id)
  }
  // Space activates a button on key release: stop that for the select shortcut.
  const onKeyUp = (e: React.KeyboardEvent<HTMLDivElement>) => { if (e.key === ' ' && (e.target as HTMLElement).matches('.tl-open')) e.preventDefault() }

  if (!graph || !layout || !box) return null
  if (graph.chapters.length === 0) return <p className="help">{TL.noChapters}</p>

  const spot = gestures.drag ? dropSpot(layout.strips, gestures.drag.wx, gestures.drag.wy) : null
  const showSpot = spot && spot.chapterId !== gestures.drag!.chapterId
  const selectedStrip = layout.strips.find((s) => s.chapterId === selectedId)
  const band = layout.unreachedBand

  return (
    <div className="timeline" data-testid="timeline">
      <TimelineBar scale={view.k} onZoom={(f) => pz.zoomBy(f)} onFit={() => pz.set(fitView(pz.size, layout))} onAddEnd={() => setAsking({ kind: 'new', index: graph.chapters.length })}
        helpOpen={helpOpen} onHelp={() => setHelpOpen(!helpOpen)} selected={selected} count={graph.chapters.length} actions={sel} />
      <Legend />
      <p className="help tl-hint" id="timeline-hint">{TL.regionHelp}</p>
      <div className="tl-view" ref={pz.viewportRef} role="region" aria-label={TL.region} aria-describedby="timeline-hint" data-testid="timeline-view"
        data-dragging={gestures.drag ? '' : undefined} onKeyDown={onKeyDown} onKeyUp={onKeyUp} onFocus={onFocus} {...gestures.handlers}>
        <div className="tl-world" ref={pz.worldRef} style={{ width: layout.width, height: layout.height, transform: `translate(${view.x}px, ${view.y}px) scale(${view.k})` }}>
          {band && (
            <div className="tl-band" style={{ top: band.y, height: band.h, width: layout.width }} data-testid="timeline-unreached">
              <strong>{TL.bandTitle}</strong>
              <span>{TL.bandNote}</span>
            </div>
          )}
          {selectedStrip && <div className="tl-strip-mark" style={{ left: selectedStrip.rect.x - 8, top: selectedStrip.rect.y - 4, width: selectedStrip.rect.w + 16, height: selectedStrip.rect.h + 8 }} />}
          <EdgesLayer graph={graph} layout={layout} visible={visibleEdges} names={names} onAddBetween={(b, a) => setAsking({ kind: 'new', index: order.indexOf(a), between: [b, a] })} />
          {graph.chapters.map((c) => {
            const strip = layout.strips.find((s) => s.chapterId === c.id)!
            const nodes = c.nodes.map((id) => graph.nodes.find((n) => n.id === id)!)
            const shown = nodes.filter(nodeVisible)
            const chipVisible = hits(strip.rect, near) || c.id === selectedId
            if (!chipVisible && shown.length === 0) return null
            return (
              <div key={c.id} className="tl-chapter" data-chapter-id={c.id}>
                {chipVisible && (
                  <button type="button" className="tl-chip" data-drag-chapter={c.id} data-testid={`chip-${c.id}`} aria-pressed={c.id === selectedId} tabIndex={-1}
                    aria-label={TL.chipSelect(c.name)} data-unreached={c.unreached || undefined}
                    style={{ left: strip.rect.x, top: strip.rect.y, maxWidth: Math.max(strip.rect.w, 330) }} onClick={() => select(c.id === selectedId ? null : c.id)}>
                    {TL.chip(c.index + 1, c.name)}{c.unreached && <span className="tl-chip-note"> · {TL.unreached}</span>}
                  </button>
                )}
                {shown.map((n) => (
                  <BoxNode key={n.id} node={n} rect={layout.nodes[n.id]} expanded={expanded.has(n.chapterId)} problems={box.byNode.get(n.id) ?? []} eventProblems={box.byEvent}
                    thumb={n.kind === 'level' && n.mapKnown && hits(layout.nodes[n.id], near) ? mapThumbnail(doc, n.mapId!) : null}
                    selected={c.id === selectedId} roving={n.id === stop} dragging={gestures.drag?.chapterId === c.id} actions={actions} />
                ))}
              </div>
            )
          })}
          {showSpot && <div className="tl-drop" data-testid="timeline-drop" style={{ left: (spot.after ? spot.rect.x + spot.rect.w + 14 : spot.rect.x - 22), top: spot.rect.y, height: spot.rect.h }} />}
        </div>
      </div>
      <TimelineDialogs asking={asking} onClose={() => setAsking(null)}
        onCreate={(name, index) => { const id = newChapterId(doc, name); pendingReveal.current = id; edit((d) => insertChapter(d, name, index, id)); select(id); setAsking(null) }}
        usages={(id) => campaignUsagesOf(doc, [{ kind: 'chapter', id }])}
        onDelete={(id) => { edit((d) => deleteChapter(d, id)); select(null); setAsking(null) }}
        onRemoveScene={(id, which) => { edit((d) => removeScene(d, id, which)); setAsking(null) }}
        nameOf={(id) => graph.chapters.find((c) => c.id === id)?.name ?? id} />
    </div>
  )
}
