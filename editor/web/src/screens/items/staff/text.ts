// Wording of the staff form on the Items screen (placeholders: the project owner reviews them).
import type { AreaMode, ReachMode } from '../../../model/staff'
import type { HealKind, RepairKind } from '../../../model/staffEffects'
import type { MoveKind } from '../../../model/staffFx'

type MoveKindOrNone = 'none' | MoveKind
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
  healOther: 'This healing is written in a way the editor cannot show. It is kept as it is.',
  keptEffects: (n: number) => `${n === 1 ? 'One more effect that the editor cannot show is' : `${n} more effects that the editor cannot show are`} kept as ${n === 1 ? 'it is' : 'they are'}.`,
  effectsHelp: 'Switch on what the staff does. A staff needs at least one effect, so the last one cannot be switched off.',
  healOn: 'It heals',
  healNoFoe: 'Healing cannot work on an enemy, so it cannot be switched on while the staff is used on enemies.',
  fourMax: 'A staff has at most four effects.',

  move: 'Moving a unit',
  moveKinds: { none: 'It does not move anyone', warp: 'Warp: send the unit to a free square', pull: 'Pull: bring the unit next to the user', swap: 'Swap: the user and the unit trade places' } satisfies Record<MoveKindOrNone, string>,
  moveHelp: 'A staff moves a unit in one way at most.',
  moveRepair: 'Moving a unit cannot be put together with repairing an item. Switch repairing off first.',
  moveSelf: 'Pull and swap need another unit, so they are not offered while the staff is used only on the user.',
  warpReach: 'How far the unit can be sent',
  warpReachHelp: 'Counted in squares from the user, not from the unit.',
  warpFarthest: 'Farthest square it can land on',
  warpNearest: 'Nearest square it can land on',
  warpFormula: 'Formula for the farthest landing square',

  repairOn: 'It repairs an item',
  repairOnHelp: 'The player picks which of the unit’s items to mend. Items can opt out on the Items screen.',
  repairBlocked: 'Repairing cannot be put together with moving a unit. Switch moving off first.',
  repair: 'Repairing',
  repairKinds: { full: 'All the uses of the item', uses: 'A number of uses' } satisfies Record<RepairKind, string>,
  repairUses: 'Uses restored',

  lightOn: 'It lights up an area',
  lightOnHelp: 'Used on a square, not a unit, and it only matters on maps with fog of war. A light staff does nothing else and has no target, area, chance of missing or conditions: switching it on removes those and the other effects (Undo brings them back).',
  lightUnknown: 'It cannot be switched on while the staff has an effect the editor cannot show, because a light staff has no other effects.',
  lightRadius: 'Radius of the light (squares)',
  lightTurns: 'Turns it lasts',
  lightTurnsHelp: 'Counted in the user’s side’s turns: 1 lasts until that side’s next turn starts.',
  lightUse: 'A light staff is used on a square, not a unit, so it has no “who it can be used on” and no “what it affects”.',
  areaSingleOnly: 'Moving a unit or repairing works on the one unit picked.',
  targetNoSelf: 'Pull and swap need another unit, so “Only the user” is not offered.',

  miss: 'It can miss',
  missHelp: 'Off: it always works. On: each unit is rolled for on its own.',
  hit: 'Chance of working, in percent',
  hitHelp: (example: string) => `A number from 0 to 100, or a formula. a is the user and b the unit it is used on, so a formula can set one against the other; dist is the distance in squares. Example: ${example}`,

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
  condUser: 'The unit using the staff',
  condHelp: 'The staff can be used only when all of these are true. Leave empty for no conditions. “The target” is the unit it is used on.',
}
