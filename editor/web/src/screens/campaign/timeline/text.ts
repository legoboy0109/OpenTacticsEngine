// All wording of the campaign timeline (placeholders: the project owner reviews them).
import type { Markers } from '../../../model/timelineTypes'
import type { EventGroup } from '../../../model/timelineTypes'

const n = (count: number, one: string, many: string) => `${count} ${count === 1 ? one : many}`

export const TL = {
  switchLabel: 'How to show the campaign',
  viewList: 'List',
  viewTimeline: 'Timeline',

  region: 'Campaign timeline',
  regionHelp: 'Drag empty space to move around, scroll or pinch to zoom. Drag a box to move its chapter. Arrow keys move between boxes, Enter opens one.',
  noChapters: 'No chapters yet. Add one.',

  zoomIn: 'Zoom in',
  zoomOut: 'Zoom out',
  fit: 'Fit all',
  addEnd: 'Add a chapter at the end',
  help: 'Keys',
  helpTitle: 'Keys for the timeline',
  helpLines: [
    'Arrow keys: move between boxes. Home / End: the first / last box.',
    'Enter: open the box (a scene, the map, or the event).',
    'Space: select the chapter of the box. Then [ moves it earlier, ] moves it later, Delete removes it.',
    '+ and −: zoom. 0: fit all.',
  ],

  selectedBar: 'Selected chapter',
  selectedName: (name: string) => `Selected: ${name}`,
  noSelection: 'Select a chapter (its name above the boxes) to move it, add chapters or scenes next to it, or remove it.',
  earlier: 'Move earlier',
  later: 'Move later',
  addBefore: 'Add a chapter before',
  addAfter: 'Add a chapter after',
  settings: 'Chapter settings',
  addBeforeScene: 'Add a scene before the battle',
  addAfterScene: 'Add a scene after the battle',
  removeBeforeScene: 'Remove the scene before the battle',
  removeAfterScene: 'Remove the scene after the battle',
  removeChapter: 'Remove the chapter',
  deselect: 'Clear selection',

  newChapterTitle: 'New chapter',
  newChapterLabel: 'Name of the chapter',
  newChapterHelp: 'For example The Ford or Escape from the Castle. It starts on the same map as its neighbour.',
  removeSceneTitle: (which: string, chapter: string) => `Remove the scene ${which} “${chapter}”?`,
  removeSceneBody: (commands: number) => (commands === 0 ? 'The scene is empty.' : `Its ${n(commands, 'command', 'commands')} will be removed.`),
  removeSceneUndo: 'You can undo this with the Undo button.',
  removeSceneConfirm: 'Remove',
  sceneBefore: 'before the battle of',
  sceneAfter: 'after the battle of',

  chip: (number: number, name: string) => `${number} · ${name}`,
  chipSelect: (name: string) => `Select the chapter ${name}`,
  addBetween: (a: string, b: string) => `Add a chapter between ${a} and ${b}`,

  sceneBeforeTitle: 'Before the battle',
  sceneAfterTitle: 'After the battle',
  openScene: (which: string, chapter: string) => `${which}, chapter ${chapter}: open the scene`,
  emptyScene: 'Empty scene',
  openLevel: (chapter: string, map: string) => `Level of chapter ${chapter}, map ${map}: open the map`,
  openLevelNoMap: (chapter: string) => `Level of chapter ${chapter}: no map chosen, open the chapter’s settings`,
  noMap: 'No map chosen',
  mapGone: (id: string) => `${id} (not there any more)`,
  skipped: 'Never played: the scene before it always jumps to another chapter.',
  unreached: 'Nothing leads here',

  expand: (count: number) => `Show ${n(count, 'event', 'events')}`,
  collapse: 'Hide events',
  noEvents: 'No events on this map',
  openEvent: (id: string, when: string) => `Open the event ${id} in the map editor: ${when}`,
  groups: {
    start: 'At the start of the battle',
    turn: 'On a turn',
    reach: 'A unit reaches a spot',
    talk: 'A unit talks to another',
    defeated: 'A unit is defeated',
    fight: 'A fight begins',
    promoted: 'A unit is promoted',
    support: 'A support level is reached',
    won: 'The battle is won',
    lost: 'The battle is lost',
    end: 'The battle ends',
    other: 'Other',
  } as Record<EventGroup, string>,

  badge: (count: number) => n(count, 'problem', 'problems'),
  badgeLabel: (count: number, list: string[]) => `${n(count, 'problem', 'problems')}: ${list.slice(0, 4).join(' ')}${list.length > 4 ? ` and ${list.length - 4} more.` : ''} Open the Problems list.`,
  badgeTitle: (list: string[]) => `${list.slice(0, 6).join('\n')}${list.length > 6 ? `\nand ${list.length - 6} more` : ''}`,

  marker: {
    join: (c: number) => `${n(c, 'unit joins', 'units join')}`,
    leave: (c: number) => `${n(c, 'unit leaves', 'units leave')}`,
    items: (c: number) => `${n(c, 'item given', 'items given')}`,
    shop: (c: number) => `${n(c, 'shop', 'shops')} on the map`,
    flags: (c: number) => `${n(c, 'switch or variable set', 'switches or variables set')}`,
    questions: (c: number) => `${n(c, 'question asked', 'questions asked')}`,
  } as Record<keyof Markers, (c: number) => string>,

  legendTitle: 'What the signs mean',
  legendMarkers: 'Small signs on a box (with how many):',
  legendMarkerNames: {
    join: 'Units join the player',
    leave: 'Units are taken out',
    items: 'Items are given',
    shop: 'A shop is on the map',
    flags: 'Switches or variables are set',
    questions: 'A question is asked',
  } as Record<keyof Markers, string>,
  legendArrows: 'Arrows:',
  legendOrder: 'The story goes on in order',
  legendNext: 'The chapter’s own “Next chapter”',
  legendGoto: '“Go to a chapter”, with what leads there',
  legendLoop: 'A loop back to an earlier chapter runs under the boxes',
  legendUnreached: 'A dashed band holds chapters nothing leads to',

  arrowLabel: (from: string, to: string, label: string, kind: string) => `${kind}: ${from} to ${to}${label ? `, ${label}` : ''}`,
  arrowOrder: 'Then',
  arrowNext: 'Next chapter',
  arrowGoto: 'Jump',
  boxFrom: (chapter: string, part: string) => `${part} of ${chapter}`,
  partLevel: 'level',

  bandTitle: 'Not reached',
  bandNote: 'Nothing leads to these chapters from chapter 1, so the player never gets to them. Point to one with “Go to a chapter” in a scene, or make the chapter before it lead on to it.',

  backToTimeline: 'Back to the timeline',
}
