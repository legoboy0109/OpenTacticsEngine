// All wording of the Conditions screen, its form and the starter set (placeholders: the project owner reviews them). The rule cards reuse the Skills screen's
// words (screens/skills/text.ts). Words kept out on purpose, as for skills: "block", "aura", "proc", "buff", "debuff", "status effect".
import type { Duration } from '../../model/conditions'

export const T = {
  title: 'Conditions',
  noun: 'condition',
  empty: 'No conditions yet. Add one, or add the starter set (poison, sleep, silence, berserk, stun and curse) to begin with.',
  newTitle: 'New condition',
  newLabel: 'Name of the condition',
  newHelp: 'For example Poison or Sleep.',
  deleteWhat: (n: string) => `the condition “${n}”`,
  checksTitle: 'Worth a look',
  manyUnused: (n: number) => `${n} conditions are not put on any unit yet. Let staves, items, weapons, skills or events inflict them.`,
  limit: 'A game can have up to 100 conditions.',

  // the starter set
  starterButton: 'Add the starter set',
  starterHelp: 'Adds poison, sleep, silence, berserk, stun and curse. Ones you already have are skipped.',
  starterAdded: (n: number) => `Added ${n} ${n === 1 ? 'condition' : 'conditions'}.`,
  starterSkippedExisting: (names: string) => `Left alone, because you already have them: ${names}.`,
  starterSkippedStat: (names: string) => `Not added, because the game has no stat to lower for them: ${names}.`,
  starterNothing: 'All of the starter conditions are in the game already.',

  // about
  name: 'Name',
  nameHelp: 'What players see.',
  description: 'Description',
  descriptionHelp: 'One short line shown to players. Optional.',
  tag: 'Tag on the map',
  tagHelp: 'One to four characters, shown on the unit while it has this condition. If you leave it empty the first two letters of the name are used.',
  tagPlaceholder: (name: string) => name.slice(0, 2),

  // how long
  lastTitle: 'How long it lasts',
  lastHelp: 'The turns are counted at the start of each of the unit’s own turns. Re-applying a condition the unit already has starts it over.',
  duration: 'It lasts',
  durations: { turns: 'A number of the unit’s own turns', cured: 'Until it is cured', mapEnd: 'Until the map ends' } satisfies Record<Duration, string>,
  turns: 'Number of turns',
  turnsHelp: 'From 1 to 99. A condition given with 3 turns affects the unit’s next three turns.',
  untilHelp: 'It never counts down. Staves, items and events can cure it. (A condition never goes on to the next map.)',
  wearOff: 'It can wear off early',
  wearOffHelp: 'Each turn there is a chance it ends by itself.',
  wearOffChance: 'Chance each turn that it wears off',
  endsWhenStruck: 'It ends when the unit is struck',
  endsWhenStruckHelp: 'A strike that does damage to the unit takes it off, as waking up a sleeper does.',
  replaces: 'It replaces these conditions',
  replacesHelp: 'Putting this condition on a unit first takes these off it.',
  replacesNone: 'There are no other conditions to replace.',

  // behaviour
  behaviourTitle: 'What it stops and forces',
  behaviourHelp: 'Switch on what this condition does to the unit. Anything else it does is a rule below.',
  cannotAct: 'The unit cannot act',
  cannotActHelp: 'Its turn is skipped.',
  cannotActBerserk: 'A unit that cannot act cannot act on its own either, so this cannot go together with “acts on its own”. Switch that off first.',
  counter: 'It can still counterattack',
  counterHelp: 'Off: a unit that cannot act answers nothing.',
  dodge: 'It can still dodge',
  dodgeHelp: 'Off: every strike on a unit that cannot act hits.',
  cannotMove: 'The unit cannot move',
  cannotMoveHelp: 'It can still act, from the square it stands on.',
  berserk: 'The unit acts on its own',
  berserkHelp: 'It attacks the nearest unit, friend or foe, by itself at the start of its turn, and takes no orders.',
  berserkCannotAct: 'A unit that cannot act cannot act on its own either, so this cannot go together with “cannot act”. Switch that off first.',
  forbid: 'The unit cannot use some things',
  forbidHelp: 'Pick what it is stopped from using.',
  forbidKinds: 'Kinds of item it cannot use',
  forbidKindNames: { weapon: 'Weapons', staff: 'Staves', usable: 'Usable items' } as Record<string, string>,
  forbidGroups: 'Groups it cannot use',
  forbidGroupsHelp: 'Weapon and staff groups of this game.',
  forbidGroupsNone: 'The game has no weapon or staff groups yet.',
  forbidNothing: 'Tick at least one kind or group, or switch this off.',
  last: 'A condition has to do something, so the last thing it does cannot be switched off. Add another rule first.',
  full: 'A condition can have at most 8 rules, so there is no room for this. Delete a rule first.',

  // rules
  rulesTitle: 'Other rules',
  rulesHelp: 'The same rules as on the Skills screen: stat changes, hit, avoid, damage, hurting or healing at the start of the turn, immunity, and more.',
  addRule: 'Add a rule',
  ruleLimit: 'A condition can have up to 8 rules (the switches above count as one).',

  summaryTitle: 'In short',
  advanced: 'Advanced',
  id: 'Id',
  idHelp: 'The short name the game files use. It never changes.',
  notValid: 'Not valid yet',
}
