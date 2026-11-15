// Lesson: "Economy". Starts from economy-tactics: structures city, factory, hq (none can be upgraded), unit types infantry, tank,
// artillery, no technologies, and a map "crossroads" without fog. Nothing below is already true at the start.
import { firstMapId, mapOf } from '../helpers'
import { firstNewKindId, newKindsOnMap, someNewKind, total } from '../helpers/structures'
import type { Lesson } from '../types'

// What the economy-tactics template begins with; the steps look for anything beyond it.
const START = { structures: ['city', 'factory', 'hq'] }
const mine = firstNewKindId(START.structures)

export const economy: Lesson = {
  id: 'economy',
  title: 'Economy',
  description: 'Let a structure build units, let another be upgraded, add a technology to research and hide the map in fog of war.',
  minutes: 8,
  template: 'economy-tactics',
  playable: true,
  summary: [
    'A structure that builds units, with one standing on the map for Blue.',
    'A City that can be upgraded for funds.',
    'A technology that unlocks a unit, and fog of war on the map.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'new-kind',
      title: 'Make a structure to build units in',
      text: 'On the Structures screen, click New structure and give it a name, like Barracks.',
      where: { section: 'structures' },
      point: 'new-item',
      done: (doc) => (doc.structures ?? []).some((s) => !START.structures.includes(s.id)),
    },
    {
      id: 'produces',
      title: 'Choose what it builds',
      text: 'Under “Units that can be built here”, tick Infantry. The owner of a Barracks can then pay funds to build infantry on it.',
      where: { section: 'structures', item: mine },
      point: 'structure-produces',
      done: (doc) => someNewKind(doc, START.structures, (s) => (s.produces?.length ?? 0) > 0),
    },
    {
      id: 'place',
      title: 'Give Blue a Barracks',
      text: 'On the Maps screen, choose the Structures tool, pick Blue Army under Owner and your Barracks in the list, then click an empty square.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-structures',
      done: (doc) => newKindsOnMap(doc, START.structures, { side: 'blue', want: (s) => (s.produces?.length ?? 0) > 0 }) >= 1,
    },
    {
      id: 'upgrade',
      title: 'Let a City be upgraded',
      text: 'On the Structures screen, open City. Under “Can be upgraded to”, click Add an upgrade, choose your Barracks, and type a cost in the Funds row.',
      where: { section: 'structures', item: () => 'city' },
      point: 'structure-upgrades',
      // The cost is typed: wait for a pause so the "5" of "500" does not tick the step.
      settleMs: 1200,
      done: (doc) => (doc.structures ?? []).some((s) => (s.upgrades ?? []).some((u) => total(u.cost) > 0)),
    },
    {
      id: 'new-tech',
      title: 'Add a technology',
      text: 'A technology is something a side pays for and waits for. On the Research screen, click New technology and give it a name, like Heavy Armor.',
      where: { section: 'research' },
      point: 'new-item',
      done: (doc) => (doc.research?.length ?? 0) >= 1,
    },
    {
      id: 'unlock',
      title: 'Make it unlock a unit',
      text: 'Under “Unlocks unit types”, tick Tank. Tanks then cannot be built until a side has finished this technology.',
      where: { section: 'research', item: (doc) => doc.research?.[0]?.id },
      point: 'tech-unlocks',
      done: (doc) => (doc.research ?? []).some((t) => (t.unlocks?.units?.length ?? 0) > 0),
    },
    {
      id: 'fog',
      title: 'Turn on fog of war',
      text: 'On the Maps screen, open the Map tab and tick “Fog of war”. Units then only see a short way around them, and the rest of the map stays hidden.',
      where: { section: 'maps', item: firstMapId },
      point: 'fog-setting',
      done: (doc) => mapOf(doc)?.fog === true,
    },
    {
      id: 'play',
      title: 'Play your game',
      text: 'Click Play at the top. Click your Barracks to build a unit, and see how little of the map is visible. Close Play when you are done.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
  ],
}
