// The shape of a lesson (docs/TUTORIAL.md "How to write a lesson").
import type { ProjectDoc, Section } from '../model/types'
import type { SignalName } from './signals'

/** Where the maker is: the game list, or one screen of the open game. */
export type ScreenName = 'games' | Section

/** Pure: true when the step's work is really done. Looks at the game document, never at clicks. */
export type DoneTest = (doc: ProjectDoc, screen: ScreenName, signals: ReadonlySet<SignalName>) => boolean

/** The screen a step happens on. `item` picks which entry to open there (a map, a unit...); missing = the screen's own list. */
export interface Where {
  section: Section
  item?: (doc: ProjectDoc) => string | undefined
}

export interface Step {
  /** Stable name; "skipped" is remembered by it. Unique within the lesson. */
  id: string
  /** Short, shown in the panel and on the slim bar. */
  title: string
  /** One to three plain sentences: what to do and why. */
  text: string
  where?: Where
  /** `data-learn` name of the control to ring while the maker is on `where`. Elsewhere the panel rings the sidebar link. */
  point?: string // alternatives: 'a|b' rings a when it is on screen, else b (docs/TUTORIAL.md)
  done: DoneTest
  /** For steps done by typing: the test must stay true this many milliseconds before the step ticks. */
  settleMs?: number
}

export interface Lesson {
  /** Stable name, used in the remembered state and the file name. */
  id: string
  title: string
  description: string
  /** Rough length, shown in the list. */
  minutes: number
  /** Template id of the practice game (GET /api/templates). */
  template: string
  steps: Step[]
  /** "What you made", one short sentence each; shown at the end. */
  summary: string[]
  /** The end of the lesson offers a Play button. */
  playable: boolean
}
