// Lesson: "Structures". Starts from economy-tactics: kinds city, factory, hq, a map "crossroads" with sides blue and red and the
// rules for capturing, funds and building already on. Nothing below is already true at the start.
import { firstMapId } from '../helpers'
import { firstNewKindId, newKindsOnMap, someNewKind, total } from '../helpers/structures'
import type { Lesson } from '../types'

// What the economy-tactics template begins with; the steps look for anything beyond it.
const START = { structures: ['city', 'factory', 'hq'] }
const mine = firstNewKindId(START.structures)

export const structures: Lesson = {
  id: 'structures',
  title: 'Structures',
  description: 'Make a building that can be captured and earns funds, put it on the map and give it to a side.',
  minutes: 6,
  template: 'economy-tactics',
  playable: true,
  summary: [
    'A new kind of structure that can be captured and earns funds every turn.',
    'One standing on the map with nobody owning it, and one that belongs to Blue.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'new-kind',
      title: 'Make a structure',
      text: 'Structures are buildings that stand on the map, like a city or a fort. On the Structures screen, click New structure and give it a name, like Mine.',
      where: { section: 'structures' },
      point: 'new-item',
      done: (doc) => (doc.structures ?? []).some((s) => !START.structures.includes(s.id)),
    },
    {
      id: 'capture',
      title: 'Let it be captured',
      text: 'Tick “Can be captured”. Units that stand on a structure take it over bit by bit; one that cannot be captured stays with its owner.',
      where: { section: 'structures', item: mine },
      point: 'structure-capture',
      done: (doc) => someNewKind(doc, START.structures, (s) => (s.capturePoints ?? 0) > 0),
    },
    {
      id: 'income',
      title: 'Make it earn funds',
      text: 'Under Income, type a number in the Funds row. Whoever owns the structure gets that many funds at the start of each of their turns.',
      where: { section: 'structures', item: mine },
      point: 'structure-income',
      // Typing a number: wait for a pause so the "1" of "1000" does not tick the step.
      settleMs: 1200,
      done: (doc) => someNewKind(doc, START.structures, (s) => total(s.income) > 0),
    },
    {
      id: 'place',
      title: 'Put it on the map',
      text: 'On the Maps screen, choose the Structures tool, pick your new structure in the list, and click an empty square on the map.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-structures',
      done: (doc) => newKindsOnMap(doc, START.structures) >= 1,
    },
    {
      id: 'give',
      title: 'Give one to Blue',
      text: 'Under Owner, pick Blue Army, keep your structure chosen, and click another empty square. Now Blue owns one and will earn its funds.',
      where: { section: 'maps', item: firstMapId },
      point: 'owner-picker',
      done: (doc) => newKindsOnMap(doc, START.structures, { side: 'blue' }) >= 1,
    },
    {
      id: 'play',
      title: 'Play your game',
      text: 'Click Play at the top. Move a blue unit onto a structure that nobody owns and choose Capture. Close Play when you are done.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
  ],
}
