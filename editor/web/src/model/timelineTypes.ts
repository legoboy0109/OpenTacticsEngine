// The campaign timeline's data (docs/EDITOR_UI.md "Campaign timeline"): a graph of boxes (a chapter's before scene, its level,
// its after scene) and arrows between them, built from the campaign and the maps by timelineGraph.ts, positioned by
// timelineLayout.ts. Nothing here is saved: it is recomputed from the document after every edit.
import type { SceneKey } from './campaignTypes'
import type { TriggerKind } from './storyTypes'

/** "What changes" counts of a list of commands (all branches of ifs and answers counted). Zero = not shown. */
export interface Markers { join: number; leave: number; items: number; shop: number; flags: number; questions: number }
export const MARKER_KEYS: Array<keyof Markers> = ['join', 'leave', 'items', 'shop', 'flags', 'questions']

/** Where an in-map event sits in the rough order: start of battle, turn numbers, then conditional ones by kind. */
export type EventGroup = 'start' | 'turn' | 'reach' | 'talk' | 'defeated' | 'fight' | 'promoted' | 'support' | 'won' | 'lost' | 'end' | 'other'

export interface TimelineEvent {
  /** Unique in the graph: chapter id + position in the map's list. */
  key: string
  chapterId: string
  mapId: string
  /** The event's own name in the map file (the Events tab opens it by this). */
  eventId: string
  group: EventGroup
  triggerKind: TriggerKind | null
  /** One plain line: "Turn 3, Red’s turn", "Mara talks to Tobin". */
  when: string
  markers: Markers
}

export interface TimelineNode {
  /** `<chapter id>:before`, `:level` or `:after`. */
  id: string
  kind: 'scene' | 'level'
  chapterId: string
  chapterIndex: number
  which?: SceneKey
  chapterName: string
  /** Levels: the map. `mapName` is the id (or "no map") when the map is not in the game. */
  mapId?: string
  mapName?: string
  mapKnown?: boolean
  markers: Markers
  /** Scenes: how many commands (all levels) it has; a scene that is only music has 0. */
  commandCount: number
  /** Nothing can lead to this chapter (unreachable from chapter 1). */
  unreached: boolean
  /** The chapter is reached but this box never plays: the scene before it always jumps away. */
  skipped: boolean
  /** Levels only: the map's events in rough order. */
  events: TimelineEvent[]
}

export type EdgeKind = 'order' | 'next' | 'goto'

export interface TimelineEdge {
  id: string
  from: string
  to: string
  kind: EdgeKind
  /** The words of an answer, a short description of an `if`, or "" (plain order, a bare goto). */
  label: string
  fromChapter: number
  toChapter: number
  /** The arrow goes back to a chapter at or before its own (a loop). */
  backward: boolean
  /** Order arrows between two neighbouring chapters of the list: where "add a chapter between" can go. */
  between?: { before: string; after: string }
}

export interface TimelineChapter {
  id: string
  index: number
  name: string
  nodes: string[]
  /** The first box played of this chapter (its before scene, else the level) and the last one (its after scene, else the level). */
  head: string
  tail: string
  unreached: boolean
  /** The story ends after this chapter (nothing follows). */
  ends: boolean
  hasBefore: boolean
  hasAfter: boolean
}

export interface TimelineGraph {
  /** In play order: chapter by chapter, before / level / after. This is also the keyboard order. */
  nodes: TimelineNode[]
  edges: TimelineEdge[]
  chapters: TimelineChapter[]
  unreached: string[]
}

export interface Rect { x: number; y: number; w: number; h: number }

export interface PlacedEdge {
  id: string
  /** SVG path (rounded corners). */
  path: string
  /** Where the label goes (the middle of a horizontal stretch). */
  label: { x: number; y: number } | null
  /** Where the "add a chapter here" button goes (order arrows between neighbours). */
  mid: { x: number; y: number } | null
  bounds: Rect
}

export interface TimelineLayout {
  nodes: Record<string, Rect>
  edges: Record<string, PlacedEdge>
  /** One per chapter: the rectangle of its boxes; the chapter's chip sits above it. */
  strips: Array<{ chapterId: string; rect: Rect }>
  width: number
  height: number
  /** The band of chapters nothing leads to (null = none). */
  unreachedBand: Rect | null
}
