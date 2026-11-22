// Wording of the "Teaches" card of an item (placeholders: the project owner reviews them).
export const TC = {
  title: 'Teaches',
  help: (points: string) => `Skills and abilities a unit may use while it has this item. Earning ${points} fills each one; when it is full the unit keeps it for good. Switch ${points} on under Rules.`,
  none: 'Teaches nothing yet.',
  ability: 'Ability',
  skill: 'Skill',
  kind: (n: number) => `Entry ${n}: kind`,
  entry: (n: number) => `Entry ${n}: which`,
  points: (n: number, points: string) => `Entry ${n}: ${points} to master it`,
  pointsHelp: 'The same on every item that teaches it.',
  remove: (n: number) => `Remove entry ${n}`,
  add: 'Add an entry',
  unknown: (id: string) => `${id} (not in this game)`,
}
