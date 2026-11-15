// Lesson: "Items". Starts from the blank template: no items at all, one unit type (soldier), one map ("start") with a blue
// and a red soldier. Nothing below is already true at the start. The way of getting items in play is a drop from a
// defeated enemy (a shop would need resources and structures, which is another lesson).
import { addedCount } from '../helpers'
import { itemsWithChangedHeal, unitTypesDropping, unitTypesWithItem } from '../helpers/items'
import type { Lesson } from '../types'

// What the blank template begins with.
const START = { items: [] as string[] }

export const items: Lesson = {
  id: 'items',
  title: 'Items',
  description: 'Make a healing item, give it to your soldiers, and let defeated enemies drop it.',
  minutes: 6,
  template: 'blank',
  playable: true,
  summary: [
    'A usable item that heals, with a number you chose.',
    'Soldiers that start the battle carrying it.',
    'A drop: the one who defeats a soldier gets its item.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'new-item',
      title: 'Make an item',
      text: 'Items are things a unit can carry and use. On the Items screen, click New item and give it a name, like Potion.',
      where: { section: 'items' },
      point: 'new-item',
      done: (doc) => addedCount(doc.items, START.items) >= 1,
    },
    {
      id: 'heal',
      title: 'Choose how much it heals',
      text: 'A new item heals 10 HP. Under Heals, type a different number, or tick All HP, to make it your own.',
      where: { section: 'items', item: (doc) => doc.items?.[0]?.id },
      point: 'item-heal',
      // Typing "15" passes through "1", so the tick waits for a short pause.
      settleMs: 1200,
      done: (doc) => itemsWithChangedHeal(doc) >= 1,
    },
    {
      id: 'give',
      title: 'Give it to your soldiers',
      text: 'On the Units screen, open the Soldier. Under Starting items, choose your item from the list. Every Soldier on a map begins the battle carrying it.',
      where: { section: 'units', item: (doc) => doc.units?.[0]?.id },
      point: 'unit-start-items',
      done: (doc) => unitTypesWithItem(doc) >= 1,
    },
    {
      id: 'drop',
      title: 'Let it drop',
      text: 'On the same row, tick Drops when defeated. Now whoever defeats a Soldier takes its item.',
      where: { section: 'units', item: (doc) => doc.units?.[0]?.id },
      point: 'unit-start-items',
      done: (doc) => unitTypesDropping(doc) >= 1,
    },
    {
      id: 'play',
      title: 'Play it',
      text: 'Click Play, then attack the red Soldier with a blue one. When it is defeated, its item moves to your unit. A hurt unit can use its item from its menu.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
    {
      id: 'problems',
      title: 'Check for problems',
      text: 'Click the badge next to Play. When it says “No problems”, your items are set up correctly.',
      point: 'problems',
      done: (_doc, _screen, signals) => signals.has('problemsChecked'),
    },
  ],
}
