// Wording of the "It opens" part of a usable item (docs/SRPG.md section 10; placeholders for the owner to review).
export const K = {
  none: 'To make a key, first give the game a door or a chest on the Structures screen.',
  on: 'It opens doors and chests',
  onHelp: 'Makes the item a key: a unit next to a locked door or chest can use it to open it. Switching this on takes away the starting heal of a new item.',
  opens: 'It opens',
  opensHelp: 'Any door or chest, or only the kinds you tick. A key that opens “any door” and “any chest” is a master key.',
  usesHelp: 'Each opening uses up one of the item’s uses, and the key breaks at 0. Switch “Unlimited” on below for a key that never wears out.',
  notOne: (id: string) => `${id} (not a door or chest of this game)`,
}
