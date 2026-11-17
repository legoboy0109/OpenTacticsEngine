// Wording of the door, chest and visit parts of the Structures screen (docs/SRPG.md section 10; placeholders for the owner to review).
import type { OpenWay, PlaceFrom } from '../../model/types'

export const D = {
  title: 'Doors, chests and places',
  help: 'Switch on what this structure is. A structure is at most one of these. Opening, visiting or destroying one turns it into another structure kind, which has its own look and its own movement rules.',

  door: 'It is a door',
  doorHelp: 'It blocks the way until it is opened.',
  chest: 'It is a chest',
  chestHelp: 'Something is inside; opening it gives that to the unit that opens it.',
  visit: 'It is a place that can be visited',
  visitHelp: 'A village, a shop or a shrine. Visiting is where a map’s events give the reward.',
  blockedOther: 'A structure is only one of these. Switch the other one off first.',
  blockedTwin: 'First make the other look of it as its own structure kind with the same size (for example an open gate), then come back here.',

  opens: 'When opened it becomes',
  opensHelpDoor: 'Another structure kind of the same size, such as the open gate. Give it its own look, and let units through under “Moving through it”.',
  opensHelpChest: 'Another structure kind of the same size, such as the open chest.',
  doorBlocksNobody: 'This door stops no kind of unit yet. Under “Moving through it”, set “Cannot enter” for the kinds it should stop, or opening it changes nothing.',
  locked: 'Starts locked',
  lockedHelp: 'A map can say otherwise for each one it places. Unlocked: anyone next to it can open it without a key.',

  ways: 'What can open it',
  waysHelpDoor: 'A story event and an unlock staff can always open it, whatever is ticked here.',
  waysHelpChest: 'A story event and an unlock staff can always open it, whatever is ticked here.',
  ways_: { key: 'A key', trait: 'A unit with a lockpicking ability', break: 'Breaking it down' } satisfies Record<OpenWay, string>,
  waysHelpEach: {
    key: 'A unit next to it uses a key item that fits.',
    trait: 'A unit that has an Unlock ability (Abilities screen).',
    break: 'Units attack it like an enemy until its hit points run out.',
  } satisfies Record<OpenWay, string>,
  storyOnly: 'With nothing ticked, only an event or an unlock staff opens it.',
  noKeysInGame: 'No item in this game is a key yet. Make one on the Items screen (“It opens”).',

  keys: 'Only these keys fit',
  keysHelp: 'Leave all unticked to let any key that opens this kind fit.',
  keysNone: 'No key opens this kind yet. Make one on the Items screen (“It opens”).',
  keyNotFitting: (name: string) => `${name} (does not open this kind)`,

  hp: 'Hit points',
  hpHelp: 'How much damage it takes to break it. Every weapon a unit can use works on it.',
  stats: 'Numbers its damage is worked out with',
  statsHelp: 'The weapon’s damage formula reads these as b.name, such as b.def for a defence of its own. Empty ones count as 0.',
  statsCol: 'Value',

  from: 'Where a unit stands to use it',
  fromKinds: { on: 'On its own square', next: 'Next to it', either: 'On it or next to it' } satisfies Record<PlaceFrom, string>,
  fromHelp: 'A door or chest that blocks the way is used from next to it.',
  blocksSight: 'It blocks sight',
  blocksSightHelp: 'With fog of war: units cannot see past it while it is shut. Squares of the structure itself are still seen.',

  becomes: 'After a visit it becomes',
  becomesNone: 'Nothing (it stays as it is)',
  becomesHelp: 'For example a village that looks plundered or quiet. A place that changes cannot be visited again.',
  ruin: 'When destroyed it becomes',
  ruinNone: 'Nothing: it cannot be destroyed',
  ruinHelp: 'An enemy of the owner can destroy a place that has a ruin. Without one it cannot be destroyed.',
  repeat: 'It can be visited again and again',
  repeatHelp: 'For a shop or a shrine. It cannot be combined with “After a visit it becomes”.',
  repeatBlocked: 'Set “After a visit it becomes” to Nothing first.',
  noTwinsVisit: 'No other structure kind has the same size, so there is nothing for it to become yet.',
}
