// Wording of the door, chest and place details in the map editor's structure panel, and the computer unit's "goes for" (docs/SRPG.md section 10;
// placeholders for the owner to review).
export const Pl = {
  title: 'Door, chest or place',
  storyName: 'Story name',
  storyNameHelp: 'The name events use to point at it (for example “open the cell door”). Optional; letters, digits, _ and - only.',
  storyNamePlaceholder: 'for example cell_door',
  suggest: 'Suggest a name',
  nameInUse: 'Events point at this name. Remove or change them first, or pick another name.',

  locked: 'Locked',
  lockedKind: (locked: boolean) => `Like its kind (${locked ? 'locked' : 'not locked'})`,
  lockedYes: 'Locked',
  lockedNo: 'Not locked',
  lockedHelp: 'A locked one needs a key (or another way you allowed on its kind). One that is not locked opens for anyone next to it.',

  contents: 'What is inside',
  contentsHelp: 'Up to 8 things. The unit that opens the chest gets them (a full pack uses the shared storage, or the chest stays shut). Money and the like go to its side.',
  contentsEmpty: 'Nothing inside yet. A chest needs at least one thing, or the game will not load.',
  addItem: 'Add an item',
  addResource: 'Add an amount',
  noItems: 'The game has no items yet.',
  noResources: 'The game has no resources yet.',
  item: (n: number) => `Thing ${n}: item`,
  uses: 'Uses left',
  usesHelp: 'Empty: as new.',
  resource: (n: number) => `Thing ${n}: resource`,
  amount: 'Amount',
  removeThing: (n: number) => `Remove thing ${n}`,
  full: 'A chest holds at most 8 things.',

  who: 'Who may visit',
  whoHelp: 'Tick sides and units to limit it. Nothing ticked: the owner’s side may visit, or anyone when it has no owner.',
  whoSides: 'Sides',
  whoUnits: 'Units (by story name)',
  whoNoUnits: 'No unit of this map has a story name yet.',

  goesFor: 'Goes for',
  goesForHelp: 'What this computer unit heads for besides fighting. A unit with nothing ticked ignores chests, places and doors.',
  goesForKinds: {
    chests: 'Chests: it opens one it can and takes what is inside (the item drops if it is defeated), and walks toward the nearest.',
    villages: 'Villages: it visits a place it may, destroys one that belongs to its enemy, and walks toward the nearest.',
    doors: 'Doors: only when a closed door is all that keeps it from its enemies, it opens one it can (unlocked, or with a key it carries).',
    steal: 'Steal: if it has a steal ability, it steals from a foe in reach instead of just waiting or moving up. It does not hunt foes down to rob them.',
  },
  goesForHuman: 'This unit is on a side a person plays, so it cannot be sent anywhere. Set the side to the computer to use this.',
  goesForShort: { chests: 'Chests', villages: 'Villages', doors: 'Doors', steal: 'Steal' },
}
