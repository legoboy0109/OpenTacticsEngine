// Lesson: "Story events". Starts from the blank template: one map ("start", 8 x 6) with a blue and a red soldier, no events and
// no objectives. The steps build two events and one objective on that first map, all in the map editor's Events and
// Objectives tabs. Nothing below is already true at the start.
import { firstMapId } from '../helpers'
import { areaPicked, eventsOf, hasSay, reachEvents, startEvents } from '../helpers/story'
import type { Lesson } from '../types'

const onFirstMap = { section: 'maps' as const, item: firstMapId }

export const story: Lesson = {
  id: 'story',
  title: 'Story events',
  description: 'Make someone speak when the battle starts and when a unit reaches a place, and give the battle a goal.',
  minutes: 8,
  template: 'blank',
  playable: true,
  summary: [
    'An opening event: a line that is said as soon as the battle starts.',
    'A second event that happens when a unit reaches the squares you picked.',
    'An objective that tells the player how to win.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'opening-event',
      title: 'Add an opening event',
      text: 'An event is something that happens during the battle. On the Maps screen, open the Events tab beside the map and click Add an event. A new event waits for the battle to start.',
      where: onFirstMap,
      point: 'add-event|map-tab-events|list-item',
      done: (doc) => startEvents(doc).length >= 1,
    },
    {
      id: 'opening-say',
      title: 'Make it say something',
      text: 'Under Then, choose Add a command and pick Say something. Type a line, like “Hold the bridge!”. It appears in a box when the battle starts.',
      where: onFirstMap,
      point: 'say-text|add-command|event-open|list-item',
      // The line is typed, so the tick waits for a short pause.
      settleMs: 1200,
      done: (doc) => startEvents(doc).some(hasSay),
    },
    {
      id: 'second-event',
      title: 'Add a second event',
      text: 'Click All events to go back to the list, then click Add an event again. This one will react to a unit moving.',
      where: onFirstMap,
      point: 'add-event|events-back|list-item',
      done: (doc) => eventsOf(doc).length >= 2,
    },
    {
      id: 'reach-when',
      title: 'Wait for a unit to arrive',
      text: 'In the new event, change When from Battle starts to A unit reaches an area.',
      where: onFirstMap,
      point: 'event-when|list-item',
      done: (doc) => reachEvents(doc).length >= 1,
    },
    {
      id: 'reach-area',
      title: 'Pick the place',
      text: 'Click Pick on the map, then drag over a few squares of the map, like the far right column. The squares you picked are drawn on the map.',
      where: onFirstMap,
      point: 'pick-area|list-item',
      done: (doc) => reachEvents(doc).some(areaPicked),
    },
    {
      id: 'reach-say',
      title: 'Say something there',
      text: 'Under Then, add Say something again and type what is said when a unit arrives, like “You made it across!”.',
      where: onFirstMap,
      point: 'say-text|add-command|list-item',
      settleMs: 1200,
      done: (doc) => reachEvents(doc).some(hasSay),
    },
    {
      id: 'objective-text',
      title: 'Tell the player the goal',
      text: 'Open the Objectives tab and type what the player has to do, like “Survive 5 rounds”. It is shown when the battle starts.',
      where: onFirstMap,
      point: 'objective-text|map-tab-objectives|list-item',
      settleMs: 1200,
      done: (doc) => (doc.maps[firstMapId(doc) ?? '']?.objectives?.text ?? '').trim() !== '',
    },
    {
      id: 'objective-win',
      title: 'Add a way to win',
      text: 'Under Win when any of these happens, choose Survive some rounds. The battle is won when your units last that long.',
      where: onFirstMap,
      point: 'add-win|map-tab-objectives|list-item',
      done: (doc) => (doc.maps[firstMapId(doc) ?? '']?.objectives?.win?.length ?? 0) >= 1,
    },
    {
      id: 'play',
      title: 'Play it',
      text: 'Click Play. Your opening line appears first. Then move a blue unit onto the squares you picked to see the second line.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
  ],
}
