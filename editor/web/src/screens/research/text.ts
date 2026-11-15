// All wording on the Research screen (and the plain-language checks for every economy option, which show on the
// screens that hold the setting). Placeholder wording until the owner approves it.
export const T = {
  title: 'Research',
  noun: 'technology',
  intro: 'Technologies a side can research: pay, wait, and get something in return. Players start research from the battle menu.',
  empty: 'No technologies yet.',
  emptyExplain: 'Research lets a side pay and wait to unlock unit types, build structures up into better ones, or make its units stronger. Add a technology here.',
  emptyExplainStructure: 'This game needs a structure for research: tick “Research can be done here” on a structure kind.',
  emptyStructures: 'Open Structures',
  noResearchStructure: 'No structure kind allows research yet, so nobody can research these. Tick “Research can be done here” on a structure kind.',
  newTitle: 'New technology',
  newLabel: 'Name of the technology',
  newHelp: 'For example Masonry or Armor Plating.',
  subtitle: (cost: string, turns: number) => `${cost} · ${turns === 0 ? 'at once' : `${turns} turn${turns === 1 ? '' : 's'}`}`,
  free: 'Free',

  name: 'Name',
  nameHelp: 'What players see.',
  description: 'Description',
  descriptionHelp: 'Shown to the player in the research list.',

  cost: 'Cost',
  costHelp: 'What it costs to start the research. Leave a box empty for no cost in that resource.',
  costCol: 'Cost',
  costNone: 'Add a resource on the Rules screen to give research a cost.',
  turns: 'Turns it takes',
  turnsHelp: 'Counted in the researching side’s own turns. 0 means it is finished at once.',
  turnsNow: 'Finished at once',
  turnsN: (n: number) => (n === 1 ? 'Finished at the start of the side’s next turn' : `Finished at the start of the side’s turn ${n} turns later`),

  needs: 'Needs first',
  needsHelp: 'Technologies that must be finished before this one can be started.',
  needsNone: 'There is no other technology to pick yet.',
  needsLoop: (why: string) => `(${why})`,

  unlocks: 'Unlocks unit types',
  unlocksHelp: 'A unit type listed here cannot be built until one of the technologies that unlock it is finished.',
  unlocksSentence: 'Unit types ticked here cannot be built without this technology.',
  unlocksNone: 'There are no unit types yet.',

  bonus: 'Bonus for units',
  bonusOn: 'Makes units stronger',
  bonusOnHelp: 'Units of the side get the numbers below added to their own, new and existing ones.',
  bonusWho: 'Which units',
  bonusAll: 'All unit types',
  bonusOnly: 'Only these unit types',
  bonusWhoHelp: 'Un-tick the last unit type to go back to all of them.',
  bonusStats: 'Stat changes',
  bonusStatsHelp: 'Use a plus or minus number. An empty box changes nothing.',
  bonusCol: 'Change',
  bonusNoStats: 'The game has no stats yet. Add some on the Rules screen.',

  usedBy: 'Used by',
  usedByNone: 'Nothing uses this technology yet.',
  usedByList: (items: string[]) => `Needed by ${items.join(', ')}.`,

  advanced: 'Advanced',
  id: 'Internal name',
  idHelp: 'Made when the technology was created and never changed by renaming. Other parts of the game point to it with this.',

  deleteWhat: (n: string) => `the technology “${n}”`,
  blockedTitle: 'Research can’t be opened: data/research.json could not be read.',
  blockedBody: 'Nothing here will be changed or saved. Fix or restore that file (it is in the game folder), then reload this page. The other sections still work.',

  checksTitle: 'The game will not load until these are fixed',
} as const
