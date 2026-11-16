// Wording of the staff form on the Items screen (placeholders: the project owner reviews them).
import type { AreaMode, HealKind, ReachMode } from '../../../model/staff'
import type { ItemTarget } from '../../../model/types'

export const S = {
  useTitle: 'How it is used',
  useHelp: 'A staff is used on another unit from a distance, and using it ends the unit’s turn.',
  group: 'Group',
  groupHelp: 'The kind of staff it is. A class can use it when it can wield this group (tick it on the Units screen). Ranks are kept per group too.',
  groupPlaceholder: 'staff',

  target: 'Who it can be used on',
  targets: { ally: 'Another unit on the user’s side', self: 'Only the user', team: 'Anyone on the user’s side, the user included', any: 'Any other unit the user’s side can see', foe: 'An enemy (healing cannot do this)' } satisfies Record<ItemTarget, string>,
  targetHelp: 'Healing works on friends; “any other unit” also heals units of other sides.',

  reach: 'How far it reaches',
  reachHelp: 'Counted in squares from where the user stands.',
  reachModes: { upto: 'Up to a number of squares', between: 'From one number of squares to another', formula: 'A formula' } satisfies Record<ReachMode, string>,
  reachSquares: 'Squares',
  reachFrom: 'From (squares)',
  reachTo: 'To (squares)',
  reachFormula: 'Formula for the farthest square',
  reachFormulaHelp: (example: string) => `The user’s stats are read as a.name, such as a.str for Strength. The result is rounded down. Example: ${example}`,
  reachSelf: 'Used on its user, so it needs no reach.',

  area: 'What it affects',
  areaModes: { single: 'One unit', radius: 'Everyone within some squares of the unit picked', all: 'Everyone in reach' } satisfies Record<AreaMode, string>,
  areaHelp: 'Each unit in the area is judged on its own: a unit that needs nothing is left out.',
  areaRadius: 'Squares around the unit picked',

  healTitle: 'What it does',
  heal: 'Healing',
  healKinds: { number: 'A number of HP', percent: 'A percentage of the unit’s maximum HP', full: 'All HP', formula: 'A formula' } satisfies Record<HealKind, string>,
  healNumber: 'HP restored',
  healPercent: 'Percent of maximum HP',
  healFormula: 'Formula for the HP restored',
  healFormulaHelp: (example: string) => `a is the user, b the unit healed, w this staff and its numbers (w.power). Example: ${example}`,
  healOther: 'This staff does something the editor cannot show. It is kept as it is.',
  healMore: (n: number) => `${n === 1 ? 'One more effect' : `${n} more effects`} come after the healing. They are kept as they are.`,

  miss: 'It can miss',
  missHelp: 'Off: it always works. On: each unit is rolled for on its own.',
  hit: 'Chance of working, in percent',
  hitHelp: (example: string) => `A number from 0 to 100, or a formula. Example: ${example}`,

  xp: 'Experience for the user',
  xpHelp: (example: string) => `Given once per use, even when it misses. A number, or a formula such as ${example}. Leave empty for none. Needs experience to be turned on in the Rules.`,
  xpPlaceholder: 'none',

  numbers: 'Numbers for formulas',
  numbersHelp: 'Named numbers the formulas can read as w.name, such as w.power for the staff’s strength.',
  numbersEmpty: 'None yet.',
  numberCol: 'Number',
  addNumber: 'Add a number (for example power)',

  worthALook: 'Worth a look',

  condTitle: 'Only if',
  condHelp: 'The staff can be used only when all of these are true. Leave empty for no conditions. “The target” is the unit it is used on.',
}
