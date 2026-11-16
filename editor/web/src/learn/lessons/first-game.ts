// Lesson 1: "Your first game". Starts from the blank template: terrain plain + wall, unit type soldier, weapon sword,
// one map ("start") with a blue and a red soldier. Nothing below is already true at the start.
import { addedCount, cellsOfOtherTerrain, firstMapId, nameChanged, unitTypesWithWeapon, unitsOnMap } from '../helpers'
import type { Lesson } from '../types'

// What the blank template begins with; the steps look for anything beyond it.
const START = { terrain: ['plain', 'wall'], units: ['soldier'], weapons: ['sword'] }

export const firstGame: Lesson = {
  id: 'first-game',
  title: 'Your first game',
  description: 'Make a small game from nothing: name it, paint a map, make a unit with a weapon, and play it.',
  minutes: 10,
  template: 'blank',
  playable: true,
  summary: [
    'A game with your own name and a map with a new kind of ground.',
    'A new unit with a weapon of its own, standing on the map for both sides.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'name',
      title: 'Name your game',
      text: 'This practice game is yours to change. On the Game screen, replace the name with anything you like.',
      where: { section: 'game' },
      point: 'game-name',
      // Typing ticks the step after a pause, not on the first letter.
      settleMs: 1200,
      done: (doc) => nameChanged(doc, 'Practice:'),
    },
    {
      id: 'new-terrain',
      title: 'Add a new kind of ground',
      text: 'Terrain is the ground your maps are made of. On the Terrain screen, click New terrain and give it a name, like Water or Forest.',
      where: { section: 'terrain' },
      point: 'new-item',
      done: (doc) => addedCount(doc.terrain, START.terrain) >= 1,
    },
    {
      id: 'paint',
      title: 'Paint it on the map',
      text: 'On the Maps screen, pick your new terrain in the Terrain list, then click or drag on the map to paint at least four squares.',
      where: { section: 'maps', item: firstMapId },
      point: 'terrain-picker',
      done: (doc) => cellsOfOtherTerrain(doc, START.terrain) >= 4,
    },
    {
      id: 'new-unit',
      title: 'Make a new unit',
      text: 'A unit is a kind of character, like a Knight or an Archer. On the Units screen, click New unit and give it a name.',
      where: { section: 'units' },
      point: 'new-item',
      done: (doc) => addedCount(doc.units, START.units) >= 1,
    },
    {
      id: 'new-weapon',
      title: 'Make a weapon',
      text: 'On the Weapons screen, click New weapon and give it a name, like Bow or Spear.',
      where: { section: 'weapons' },
      point: 'new-item',
      done: (doc) => addedCount(doc.weapons, START.weapons) >= 1,
    },
    {
      id: 'give-weapon',
      title: 'Give your unit the weapon',
      text: 'A unit can only attack with the weapons it has. On the Units screen, click your new unit in the list (not the Soldier), then tick a weapon for it.',
      where: { section: 'units', item: (doc) => doc.units?.find((u) => !START.units.includes(u.id))?.id },
      point: 'unit-weapons',
      done: (doc) => unitTypesWithWeapon(doc, START.units) >= 1,
    },
    {
      id: 'place-blue',
      title: 'Put your unit on the map for Blue',
      text: 'On the Maps screen, choose the Units tool, pick Blue and your new unit, then click an empty square on the map.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-units',
      done: (doc) => unitsOnMap(doc, { side: 'blue', notTypes: START.units }) >= 1,
    },
    {
      id: 'place-red',
      title: 'Put one on the map for Red too',
      text: 'Pick Red in the Side list, keep your new unit chosen, and click another empty square. Now both sides have one.',
      where: { section: 'maps', item: firstMapId },
      point: 'tool-units',
      done: (doc) => unitsOnMap(doc, { side: 'red', notTypes: START.units }) >= 1,
    },
    {
      id: 'play',
      title: 'Play your game',
      text: 'Click Play at the top. Then click a blue unit and click a square to move it. Close Play when you are done.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
    {
      id: 'problems',
      title: 'Check for problems',
      text: 'Click the badge next to Play. When it says “No problems”, your game is in good shape. If it lists problems, click one to fix it.',
      point: 'problems',
      done: (_doc, _screen, signals) => signals.has('problemsChecked'),
    },
  ],
}
