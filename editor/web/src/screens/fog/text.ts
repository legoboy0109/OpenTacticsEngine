// Wording of the fog of war fields on the Units, Terrain and Structures screens.
export const T = {
  unitTitle: 'Sight',
  unitHelp: 'Only matters on maps with fog of war (switch it on in the map’s settings).',
  unitSight: 'Sight',
  unitSightHelp: (normal: number) => `How many squares away this unit’s team can see, counting steps up, down, left and right (0 to 20). Normal is ${normal}. Not changed by research or items.`,
  groundTitle: 'Sight and hiding',
  groundHelp: 'Only matters on maps with fog of war (switch it on in the map’s settings).',
  bonus: 'Extra sight for a unit standing here',
  bonusHelp: 'Squares added to the sight of a unit standing on it, for example a hill or a tower (0 to 10). With a structure on the ground, both add up.',
  hides: 'Hides a unit standing here until an enemy is next to it',
  hidesHelp: 'Like a forest: other teams see a unit here only from a square right beside it.',
}
