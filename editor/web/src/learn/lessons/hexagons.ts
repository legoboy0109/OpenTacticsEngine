// Lesson: "Hexagons". Starts from hex-tactics-pointy: a hexagon map "crossing" (12 x 9) with six mountain hexagons, kinds city, factory,
// hq and a seven-hexagon fortress, sides blue and red with three blue units. Nothing below is already true at the start.
import { cellsOfTerrain, firstMapId, unitsOnMap } from '../helpers'
import { firstNewKindId, newKindsOnMap, someNewKind } from '../helpers/structures'
import type { Lesson } from '../types'

// What the hex-tactics-pointy template begins with; the steps look for anything beyond it.
const START = { structures: ['city', 'factory', 'hq', 'fortress'], mountains: 6, blueUnits: 3 }
const castle = firstNewKindId(START.structures)
// A kind that covers a hexagon and the six around it ("rings": 1).
const sevenHexagons = (s: { rings?: number }) => s.rings === 1

export const hexagons: Lesson = {
  id: 'hexagons',
  title: 'Hexagons',
  description: 'Paint a map made of hexagons, make a structure that covers seven of them, and play.',
  minutes: 6,
  template: 'hex-tactics-pointy',
  playable: true,
  summary: [
    'A hexagon map with a patch of mountains you painted.',
    'A structure that covers seven hexagons, standing on the map for Blue.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'new-kind',
      title: 'Make a big structure',
      text: 'This game’s maps are made of hexagons, not squares. On the Structures screen, click New structure and give it a name, like Castle.',
      where: { section: 'structures' },
      point: 'new-item',
      done: (doc) => (doc.structures ?? []).some((s) => !START.structures.includes(s.id)),
    },
    {
      id: 'covers',
      title: 'Make it cover seven hexagons',
      text: 'Under Size, choose “7 hexagons”. A structure then covers one hexagon and the six around it.',
      where: { section: 'structures', item: castle },
      point: 'structure-covers',
      done: (doc) => someNewKind(doc, START.structures, sevenHexagons),
    },
    {
      id: 'place',
      title: 'Put it on the map for Blue',
      text: 'On the Maps screen, choose the Structures tool, pick Blue Army under Owner and your Castle in the list, then click an empty hexagon with room around it.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-structures',
      done: (doc) => newKindsOnMap(doc, START.structures, { side: 'blue', want: sevenHexagons }) >= 1,
    },
    {
      id: 'paint',
      title: 'Paint hexagons',
      text: 'Terrain is painted one hexagon at a time. Choose the Terrain tool, pick Mountain in the list, and click or drag to paint at least four hexagons.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-terrain',
      done: (doc) => cellsOfTerrain(doc, 'mountain') >= START.mountains + 4,
    },
    {
      id: 'unit',
      title: 'Put a unit beside it',
      text: 'Choose the Units tool, pick Blue Army and any unit, and click an empty hexagon next to your Castle. On hexagons a unit can step in six directions.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-units',
      done: (doc) => unitsOnMap(doc, { side: 'blue' }) >= START.blueUnits + 1,
    },
    {
      id: 'play',
      title: 'Play your game',
      text: 'Click Play at the top, click a blue unit and move it around. Close Play when you are done.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
  ],
}
