// All wording of the difficulty parts of the editor (docs/DIFFICULTY.md): the Rules card, a map's own numbers, a unit's "only on".
import type { DifficultyChange } from '../../model/difficultyTypes'

export const D = {
  title: 'Difficulty',
  help: 'Let players choose how hard the game is. You decide the list (easiest first), what each one changes, and the one that is chosen unless the player picks. A game with no difficulties asks nothing and plays as it is.',
  noneHelp: 'This game has no difficulties. Add some to let players choose Easy, Hard or anything you like.',
  startThree: 'Start with Easy, Normal and Hard',
  startOne: 'Start with one difficulty',
  startThreeHelp: 'You can rename them and change every number afterwards.',

  defaultLabel: 'Chosen unless the player picks',
  defaultHelp: 'The difficulty that is preselected on the choice screen, and used when nothing asks (a quick test, a save from before difficulties).',
  changeLabel: 'Can the player change it later?',
  changeHelp: 'The difficulty is chosen when a new game starts. This says whether the menu in a battle lets the player change it afterwards. A change applies from the next battle.',
  change: {
    never: { label: 'Never', help: 'It stays as chosen at the start.' },
    lowerOnly: { label: 'Only to an easier one', help: 'A player who finds it too hard can go down, never up.' },
    free: { label: 'Any time', help: 'The player can switch to any difficulty between battles.' },
  } as Record<DifficultyChange, { label: string; help: string }>,

  listTitle: 'The list, easiest first',
  listHelp: 'The order matters: “easier” and “at least” go by this order.',
  noun: 'difficulty',
  addPlaceholder: 'Add a difficulty',
  addButton: 'Add difficulty',
  entryName: (n: number) => `Name of difficulty ${n}`,
  moveUp: (n: string) => `Move ${n} up`,
  moveDown: (n: string) => `Move ${n} down`,
  remove: (n: string) => `Delete ${n}`,
  isDefault: 'default',
  makeDefault: (n: string) => `Make ${n} the default`,
  open: 'What it changes',

  description: 'What the player reads',
  descriptionHelp: 'One or two lines on the choice screen. Say what changes in plain words.',
  enemyTitle: 'Enemies',
  enemyHelp: 'Applies to the computer sides that fight a human side. Allies and neutral sides are never touched. A boss or one unit can opt out in the Units tool.',
  levels: 'Enemy levels',
  levelsHelp: 'Enemies are built as if placed this many levels higher (negative: lower), never below level 1. Their stats grow by the average of their growths for each level. A stat you type on a map unit is never changed.',
  statsAdd: 'Added to enemy stats',
  statsAddHelp: 'A flat amount per stat, added after the levels.',
  statsPercent: 'Enemy stats percent',
  statsPercentHelp: 'After the levels and the added amounts the stats below are scaled by this percent. 100 changes nothing.',
  percentStatsTitle: 'Stats the percent applies to',
  percentStatsHelp: 'Leave all unticked for every stat except the movement stat.',
  xpPercent: 'Experience percent',
  xpPercentHelp: 'How much experience your units earn. 100 = as usual.',
  resourcePercent: 'Resources percent',
  resourcePercentHelp: 'How much the human sides gain: income each turn, resources given by the story, and loot. Costs are not changed. 100 = as usual.',
  readByStory: 'Events and formulas can read the difficulty: use the condition “The difficulty is …” in a story event, or the number `difficulty` (0 = the first in this list) and the text `difficultyId` in a formula.',

  deleteTitle: (n: string) => `the difficulty “${n}”`,
  deleteLast: 'It is the last one: the game then has no difficulties.',

  // a map's own numbers
  mapTitle: 'Difficulty on this map',
  mapHelp: 'Change what a difficulty does on this map only. A number you set here replaces the game’s number for this map; leave a box empty to keep the game’s. For example: Hard adds 3 enemy levels here.',
  mapNone: 'The game has no difficulties yet. Add them on the Rules screen.',
  mapClear: 'Use the game’s numbers',
  mapGames: (v: string) => `Game: ${v}`,

  // placements
  onlyOnTitle: 'Only on these difficulties',
  onlyOnHelp: 'Untick a difficulty to leave this unit out of it. All ticked = it appears on every difficulty.',
  onlyOnGone: (id: string) => `Limited to “${id}”, which no longer exists`,
  shiftTitle: 'Keep the same on every difficulty',
  shiftHelp: 'The difficulty’s enemy numbers (levels, added stats, percent) never change this unit: use it for a boss. Stats you type on this unit are always kept as they are.',
  sideShift: 'Difficulty changes this side',
  sideShiftHelp: 'Off: the difficulty’s enemy numbers never touch this computer side’s units.',

  // the Play panel
  playLabel: 'Difficulty',
  playAsk: 'Ask me',
  playTitle: 'Which difficulty the preview plays at. “Ask me” shows the choice screen like a player would see it.',
}
