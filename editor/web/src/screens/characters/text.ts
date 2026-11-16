// All wording on the Characters screen and the map's character picker (placeholders: the project owner reviews it).
// `word` is "class" or "unit type" (model/characters.ts `classWord`): a game with characters says "class".
export const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1)

export const T = {
  title: 'Characters',
  noun: 'character',
  empty: 'No characters yet. Add one, such as the hero of your story.',
  newTitle: 'New character',
  newLabel: 'Name of the character',
  newHelp: 'For example Eirik or Mara.',
  noClasses: 'A character needs a class to start as. Add one on the Units screen first.',
  subtitle: (cls: string, level: number | undefined) => [cls, level && level > 1 ? `level ${level}` : ''].filter(Boolean).join('  ·  '),
  name: 'Name',
  nameHelp: 'What players see.',
  description: 'Description',
  descriptionHelp: 'One short line about this person. Optional.',
  classHelp: (word: string) => `What the character starts as. Its numbers below are added to the ${word}’s. On a map you can still place it as a different ${word}.`,
  level: 'Joins at level',
  levelHelp: (max: number) => `From 1 to ${max}. Leave empty for level 1.`,
  deleteWhat: (n: string) => `the character “${n}”`,

  statsTitle: 'Stats',
  statsHelp: (word: string) => `The first column is what the ${word} gives. Type this person’s own bonus in the second column (a minus makes them weaker). The third column is what the character starts with.`,
  growthsTitle: 'Growth % each level',
  growthsHelp: (word: string) => `The chance, each level, that a stat goes up by one. The personal bonus is added to the ${word}’s chance.`,
  colBonus: 'Personal',
  colTotal: 'Total',
  noStats: 'There are no stats yet. Add some on the Rules screen.',

  itemsOwn: 'Starts with its own items',
  itemsOwnOff: (names: string[], word: string) => `Off: it starts with what its ${word} starts with${names.length ? ` (${names.join(', ')})` : ' (nothing)'}.`,
  itemsOwnOn: (word: string) => `On: this list is what it starts with, instead of the ${word}’s.`,
  itemsLabel: 'Starting items',

  portraitTitle: 'Portrait',
  portraitHelp: 'The picture shown beside this person’s dialogue. It is saved under the character’s internal name, so a Say command that names it uses it.',
  portraitNone: 'No portrait yet.',
  portraitNeedImage: 'Add an image on the Art screen first, then pick a piece of it here.',
  portraitAdd: 'Add a portrait',
  portraitChoose: 'Choose picture…',
  portraitRemove: 'Remove portrait',
  portraitLabel: (n: string) => `Portrait of ${n}`,

  advanced: 'Advanced',
  id: 'Internal name',
  idHelp: 'Made when the character was created and never changed by renaming. Events, saves and the campaign point to the person with it.',

  checksTitle: 'Worth a look',

  // Map editor
  pickerLabel: 'What to place',
  pickerCharacters: 'Characters',
  noCharacters: 'No characters yet. Add some on the Characters screen.',
  onMap: 'On the map',
  characterUnitHelp: 'This unit is a character. Its name, level and numbers come from the Characters screen; the boxes below change them for this map only.',
  openCharacter: 'Open character',
  classOnMap: 'Its own',
  classOnMapHelp: 'Pick a different one to make this character a different class on this map only.',
  characterGone: (id: string) => `The character “${id}” does not exist any more.`,
}
