// Lesson: "Art". Starts from the blank template: terrain plain + wall, unit type soldier, one map "start" with a blue and a red soldier,
// and two ready-made images (tiles: ground pieces including a set of road pieces that join up; units: the soldier's pictures), so the
// maker needs no picture file of their own. Nothing below is already true at the start.
import { newTerrainIds, newUnitIds, joinedCells, joinedTerrainIds, pictureUnitIds } from '../helpers/art'
import { firstMapId, unitsOnMap } from '../helpers'
import type { Lesson } from '../types'

// What the blank template begins with; the steps look for anything beyond it.
const START = { terrain: ['plain', 'wall'], units: ['soldier'] }

export const art: Lesson = {
  id: 'art',
  title: 'Art',
  description: 'Give a terrain and a unit a picture from the images that come with your game, and paint a road that joins up.',
  minutes: 8,
  template: 'blank',
  playable: true,
  summary: [
    'A road terrain with pictures that join up as you paint it.',
    'A new unit with a picture of its own, standing on the map for Blue.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'new-terrain',
      title: 'Add a road',
      text: 'On the Terrain screen, click New terrain and name it Road. It starts as a plain coloured square; next you give it a picture.',
      where: { section: 'terrain' },
      point: 'new-item',
      done: (doc) => newTerrainIds(doc, START.terrain).length >= 1,
    },
    {
      id: 'open-chooser',
      title: 'Open the picture chooser',
      text: 'Your game comes with images to take pictures from. On your Road, find the Picture card and click Choose picture….',
      where: { section: 'terrain', item: (doc) => newTerrainIds(doc, START.terrain)[0] },
      point: 'picture-choose',
      done: (_doc, _screen, signals) => signals.has('pictureChooser'),
    },
    {
      id: 'join-picture',
      title: 'Pick pieces that join up',
      text: 'Under “What kind of picture?”, choose “Joins with neighbours”. Then click the top-left piece of the 16 beige road pieces on the left, a little below the grass, and click Use this picture.',
      point: 'picker-grid',
      done: (doc) => joinedTerrainIds(doc, START.terrain).length >= 1,
    },
    {
      id: 'paint',
      title: 'Paint a road',
      text: 'On the Maps screen, pick Road in the Terrain list, then drag across the map to paint at least five squares in a row. Watch the pieces join into one road.',
      where: { section: 'maps', item: firstMapId },
      point: 'terrain-picker',
      done: (doc) => joinedCells(doc, START.terrain) >= 5,
    },
    {
      id: 'new-unit',
      title: 'Make a unit',
      text: 'On the Units screen, click New unit and give it a name, like Archer.',
      where: { section: 'units' },
      point: 'new-item',
      done: (doc) => newUnitIds(doc, START.units).length >= 1,
    },
    {
      id: 'unit-picture',
      title: 'Give the unit a picture',
      text: 'On your new unit, click Choose picture…. In the Image list pick the units image, click a piece, then click Use this picture.',
      where: { section: 'units', item: (doc) => newUnitIds(doc, START.units)[0] },
      point: 'picture-choose',
      done: (doc) => pictureUnitIds(doc, START.units).length >= 1,
    },
    {
      id: 'place',
      title: 'Put it on the map',
      text: 'On the Maps screen, choose the Units tool, pick Blue and your new unit, then click an empty square. It is drawn with the picture you chose.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-units',
      done: (doc) => unitsOnMap(doc, { side: 'blue', notTypes: START.units }) >= 1,
    },
    {
      id: 'play',
      title: 'Play your game',
      text: 'Click Play at the top and find your road and your unit in the game. Close Play when you are done.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
  ],
}
