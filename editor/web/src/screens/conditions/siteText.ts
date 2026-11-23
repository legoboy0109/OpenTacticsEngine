// Wording of the places that use a status condition: a staff's two parts, a usable item's "Cures" and "Inflicts", a weapon's "May inflict a condition on a hit"
// (placeholders: the project owner reviews them).
import type { ChanceKind, CureMode } from '../../model/conditionSites'

export const C = {
  pick: 'Condition',
  turns: 'Number of turns',
  turnsHelp: 'Leave empty to use the condition’s own number of turns.',
  turnsNo: 'This condition does not count turns, so there is nothing to set here.',
  none: 'The game has no conditions yet. Add them on the Conditions screen.',
  notOne: (id: string) => `${id} (not a condition of this game)`,

  // a staff
  staffInflictOn: 'It inflicts a condition',
  staffInflictHelp: 'The unit it is used on gets the condition when the staff works. A unit that is immune to it, or has it already for as long, is not a target.',
  staffInflictOther: 'This is written in a way the editor cannot show. It is kept as it is.',
  staffCureOn: 'It cures',
  staffCureHelp: 'Takes these conditions off the unit it is used on. A unit that has none of them is not a target.',
  staffCureWhich: 'It cures',
  staffCureKinds: { list: 'The conditions I pick', all: 'Every condition', harmful: 'Every harmful condition', helpful: 'Every helpful condition' },
  staffCureList: 'Conditions it cures',
  cureNeedsOne: 'It cures nothing yet. Pick at least one condition, or choose every condition. Until then this part is not saved.',
  staffCureOther: 'This is written in a way the editor cannot show. It is kept as it is.',
  staffLight: 'A light staff or an unlock staff does nothing else, so it cannot inflict or cure.',
  staffFull: 'A staff has at most four effects.',
  staffNone: 'The game has no conditions yet. Add them on the Conditions screen first.',

  // a usable item
  itemTitle: 'Conditions',
  itemCures: 'Cures',
  itemCureKinds: { none: 'No condition', list: 'The conditions I pick', all: 'Every condition', harmful: 'Every harmful condition', helpful: 'Every helpful condition' } satisfies Record<CureMode, string>,
  itemCureHelp: 'Taken off the unit the item is used on. An item that would change nothing (nothing to cure) is not offered.',
  itemCureList: 'Conditions it cures',
  itemInflict: 'It puts a condition on the unit it is used on',
  itemInflictHelp: 'Meant for items that are risky to use. A unit that is immune to it, or has it already for as long, is not offered the item.',
  itemNone: 'The game has no conditions yet, so there is nothing to cure or inflict. Add them on the Conditions screen.',

  // a weapon
  weaponTitle: 'May inflict a condition on a hit',
  weaponHelp: 'A hit may put a condition on the unit struck. It only takes hold when the strike hits.',
  weaponOn: 'It may inflict a condition',
  weaponChance: 'How often',
  weaponChanceKinds: { always: 'On every hit', percent: 'A percent chance on a hit', formula: 'A formula' } satisfies Record<ChanceKind, string>,
  weaponPercent: 'Chance on a hit',
  weaponFormula: 'Formula for the chance, in percent',
  weaponFormulaHelp: (example: string) => `a is the unit using the weapon and b the unit struck, so stats can be set against each other. Example: ${example}`,
  weaponOther: 'This is written in a way the editor cannot show. It is kept as it is.',
}
