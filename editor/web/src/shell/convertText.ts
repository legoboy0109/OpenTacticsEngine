// All wording of the "update an older game" offer.
const list = (items: string[]): string => (items.length <= 1 ? items.join('') : `${items.slice(0, -1).join(', ')} and ${items[items.length - 1]}`)

export const C = {
  bannerTitle: 'This game was made with an older version of the editor.',
  bannerBody: 'Buildings such as cities and factories are now structures that stand on top of the ground. Until the game is updated it will not load, and Play does not work.',
  review: 'See what will change…',
  dialogTitle: 'Update this game for structures',
  intro: 'Buildings used to be kinds of terrain. They are now structures that stand on top of the ground, so each can have its own size, picture and owner. The editor can update this game for you. This is what will change:',
  kindsTitle: 'Terrain that becomes structures',
  kind: (name: string, settings: string[], squares: number) =>
    `“${name}”${settings.length ? ` (${list(settings)})` : ''} becomes a structure with the same name, colour and letter and those same settings. ${squares === 0 ? 'No map uses it.' : `${squares} square${squares === 1 ? '' : 's'} on your maps get one.`}`,
  groundStays: 'The terrain itself stays. It keeps its movement costs and combat numbers and stays under the structure, so the game plays exactly as before. It only loses the settings that moved: capture points, income, units it can build and the headquarters label.',
  ownersTitle: 'Who owns what',
  ownersKept: (n: number) => `${n} owned square${n === 1 ? '' : 's'} move to the structures: each structure now belongs to that side.`,
  ownersDropped: (n: number) => `${n} owned square${n === 1 ? '' : 's'} sit${n === 1 ? 's' : ''} on ground that is not one of those buildings. They never had any effect in a battle and are left out.`,
  looks: 'Pictures and colours of those terrains are copied to the new structures (including each style’s colour), so they look the same afterwards.',
  safe: 'Nothing else changes. You can undo all of it in one step with the Undo button.',
  cancel: 'Not now',
  convert: 'Convert this game',
  done: 'The game now uses structures. You can undo this with the Undo button.',
} as const
