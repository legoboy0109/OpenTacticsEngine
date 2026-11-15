// Done-test helpers of the "Story events" lesson (lessons/story.ts). They read the events and objectives of a map
// (default: the game's first map).
import type { ProjectDoc } from '../../model/types'
import type { Command, StoryEvent } from '../../model/storyTypes'
import { mapOf } from '../helpers'

export const eventsOf = (doc: ProjectDoc, mapId?: string): StoryEvent[] => mapOf(doc, mapId)?.events ?? []

/** True when the command is "Say something" with some text in it. */
export const saysSomething = (c: Command): boolean => typeof c.say === 'string' && c.say.trim() !== ''

/** Events that wait for the battle to start. */
export const startEvents = (doc: ProjectDoc, mapId?: string): StoryEvent[] => eventsOf(doc, mapId).filter((e) => e.when?.battle === 'start')

/** Events that wait for a unit to reach an area. */
export const reachEvents = (doc: ProjectDoc, mapId?: string): StoryEvent[] => eventsOf(doc, mapId).filter((e) => e.when?.reach !== undefined)

/** A new "reaches an area" trigger points at the top-left square; an area anywhere else was picked on purpose. */
export const areaPicked = (e: StoryEvent): boolean => {
  const r = e.when?.reach
  return !!r && !(r.x === 0 && r.y === 0 && r.w === 1 && r.h === 1)
}

export const hasSay = (e: StoryEvent): boolean => (e.do ?? []).some(saysSomething)
