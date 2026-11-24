// Wording for "Downed units and revive" (docs/SRPG.md). Placeholders: the project owner reviews every sentence here.
import type { ReviveKind } from '../../model/downed'

export const D = {
  // the Rules screen card
  title: 'Downed units',
  help: 'Off: a unit that reaches 0 HP falls at once. On: it lies on the board, downed, and can be stood up again by a revive.',
  on: 'Units are downed first',
  onHelp: 'Switch this on to use revive abilities, items, staves and conditions. A downed unit takes up its square until it is revived or gone.',
  afterBattle: 'A unit still downed when the battle ends',
  afterBattleHelp: 'Back: it is with the army again for the next battle. Fallen: it counts as fallen, and the campaign’s own rule for fallen units decides what happens.',
  afterBattleOptions: { return: 'Comes back for the next battle', fallen: 'Counts as fallen' },
  removeAfter: 'Turns it lies there before it is gone for good',
  removeAfterHelp: 'Counted in its side’s own turns. 0 = never: it stays until revived or the battle ends. From 0 to 99.',

  // an amount of HP
  how: 'Stands up with',
  kinds: { amount: 'A number of hit points', full: 'All its hit points', percent: 'A share of its maximum', formula: 'A formula' } satisfies Record<ReviveKind, string>,
  amount: 'Hit points',
  amountHelp: 'From 1 to 999.',
  percent: 'Percent of the maximum',
  percentHelp: 'From 1 to 100. At least 1 point.',
  formula: 'Formula',
  formulaHelp: 'a is the unit reviving, b is the unit stood up. Example: a.mag * 2. Rounded down; below 1 counts as 1.',

  // ability, item, staff, condition
  abilityRevive: 'Stands a downed unit up. Only downed units are affected, so with an area it acts on the downed units in it.',
  needsRule: 'Switch on “Downed units” on the Rules screen, or this cannot be used.',
  itemOn: 'Revives a downed unit',
  itemOnHelp: 'Used on a downed neighbour. Switching it on takes away healing, raising, curing, conditions, promotion, opening and light, which cannot go with it.',
  staffOn: 'Revives a downed unit instead',
  staffOnHelp: 'Its only effect. Used on a downed ally, the team or anyone; not on foes or the user.',
  conditionOn: 'Stands the unit up when it would be downed',
  conditionOnHelp: 'A unit with this condition that would be downed gets up at once with this much HP, and the condition ends. The condition needs no other rule.',
}
