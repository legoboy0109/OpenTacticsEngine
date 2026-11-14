// All wording on the Items screen (placeholders: the project owner reviews them).
import type { ItemKind } from '../../model/types'

export const KIND_LABEL: Record<ItemKind, string> = { usable: 'Usable', weapon: 'Weapon', accessory: 'Accessory' }

export const T = {
  title: 'Items',
  noun: 'item',
  intro: 'Things units can carry: potions, weapons, rings. Give them to units on the Units screen, sell them in shops on the Structures screen, or put them on a map.',
  empty: 'No items yet. Add one, such as a Potion.',
  newTitle: 'New item',
  newLabel: 'Name of the item',
  newHelp: 'For example Potion or Iron Sword.',
  name: 'Name',
  nameHelp: 'What players see.',
  description: 'Description',
  descriptionHelp: 'One short line shown to players. Optional.',
  deleteWhat: (n: string) => `the item “${n}”`,
  checksTitle: 'The game will not load until these are fixed',

  kind: 'Kind',
  kindHelp: 'Usable items are used up on a unit. Weapon items are carried and fought with. Accessories give a bonus to whoever carries them.',
  kindUsable: KIND_LABEL.usable,
  kindWeapon: KIND_LABEL.weapon,
  kindAccessory: KIND_LABEL.accessory,
  kindWeaponLocked: 'Weapon items need weapons to be carried as items. Change that under Items on the Rules screen.',
  rulesLink: 'Open Rules',

  uses: 'Uses',
  usesHelp: 'How many times it can be used before it breaks. A weapon uses one each time it strikes.',
  unlimited: 'Unlimited',
  unlimitedHelp: 'It never wears out.',
  usesCount: 'Number of uses',

  price: 'Price',
  priceHelp: 'What a shop charges. Leave every box empty if shops cannot sell it.',
  priceCol: 'Price',
  priceNone: 'This game has no resources yet, so nothing can be priced. Add a resource (such as Gold) on the Rules screen.',
  notSold: 'Not sold in shops',

  target: 'Who it can be used on',
  targetHelp: 'A neighbouring ally is a unit of the same team on a square next to the user.',
  targetSelf: 'Itself',
  targetAlly: 'A neighbouring ally',
  heals: 'Heals',
  healsHelp: 'How much health it gives back. Leave empty for none.',
  healsAmount: 'Amount of health restored',
  healsAll: 'All HP',
  raises: 'Raises for good',
  raisesHelp: 'Added to the stat permanently when used. Leave empty for none. A usable item needs to heal or raise something.',
  raisesCol: 'Raise by',

  weapon: 'Which weapon',
  weaponHelp: 'The weapon this item lets a unit fight with (its range and damage come from the Weapons screen).',
  weaponNone: 'Pick a weapon',
  noWeapons: 'There are no weapons yet. Create some on the Weapons screen.',

  bonus: 'Bonus while carried',
  bonusHelp: 'Added to the unit’s stats while it carries this. An accessory needs a bonus to at least one stat.',
  bonusCol: 'Bonus',

  advanced: 'Advanced',
  id: 'Internal name',
  idHelp: 'Made when the item was created and never changed by renaming. Other parts of the game point to the item with it.',
  subtitle: (kind: ItemKind, uses: number | undefined) =>
    kind === 'accessory' || uses === undefined ? KIND_LABEL[kind] : `${KIND_LABEL[kind]}  ·  ${uses} use${uses === 1 ? '' : 's'}`,
}
