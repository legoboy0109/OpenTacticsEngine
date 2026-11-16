// Wording shared by the strategy-RPG screens and cards (placeholders: the project owner reviews them).
export const T = {
  // Rules card "Strategy-RPG features"
  cardTitle: 'Strategy-RPG features',
  cardHelp: 'Optional parts for games with named characters. Each one adds its own screen to the sidebar. Turning one off asks first, because it removes what you made there.',
  characters: 'This game has characters',
  charactersHelp: 'Named people with their own stats, skills and story. Units on a map can be one.',
  skills: 'This game has skills',
  skillsHelp: 'Bonuses and special effects that units, characters, weapons and items can have.',
  abilities: 'This game has abilities',
  abilitiesHelp: 'Extra commands in a unit’s menu, such as pushing a neighbour or summoning a helper.',
  supports: 'This game has supports',
  supportsHelp: 'Characters grow closer, unlock conversations and fight better side by side.',
  supportsNeedCharacters: 'Supports are between two characters: switch on characters first.',
  offTitle: (what: string) => `Remove ${what}?`,
  offBody: (what: string, count: number) => `This removes ${what} from the game${count > 0 ? `, including the ${count} you have made` : ''}. You can undo it with the Undo button.`,
  offConfirm: 'Remove',
  /** What each feature is called in the question above. */
  what: { characters: 'all characters', skills: 'all skills', abilities: 'all abilities', supports: 'all supports' },

  // The four screens
  screens: {
    characters: { title: 'Characters', intro: 'Named people with their own stats, skills and story.' },
    skills: { title: 'Skills', intro: 'Bonuses and special effects for units, characters, weapons and items.' },
    abilities: { title: 'Abilities', intro: 'Extra commands in a unit’s menu.' },
    supports: { title: 'Supports', intro: 'How closeness between two characters grows and what it unlocks.' },
  },
  notAvailableYet: 'Editing this is not available yet.',
  off: (title: string) => `${title} are switched off for this game.`,
  offHelp: 'Switch them on in the Rules screen, under Strategy-RPG features.',
  rulesLink: 'Open Rules',
  unreadable: (title: string, file: string) => `${title} can't be opened: ${file} could not be read.`,
  unreadableHelp: 'Nothing here will be changed or saved. Fix or restore that file (it is in the game folder), then reload this page.',
}
