// Wording of the character's "Extra weapon groups" card (placeholders: the project owner reviews them).
export const W = {
  title: 'Extra weapon groups',
  label: 'Also can wield',
  help: 'Kinds of weapon this character can use in addition to what their class can. It never takes one away. Weapons get their group on the Weapons screen. Set the starting rank for a group under Personal ranks.',
  helpStaves: 'Kinds of staff this character can use in addition to what their class can. It never takes one away. Staves get their group on the Items screen. Set the starting rank for a group under Personal ranks.',
  helpBoth: 'Kinds of weapon or staff this character can use in addition to what their class can. It never takes one away. Weapons get their group on the Weapons screen, staves on the Items screen. Set the starting rank for a group under Personal ranks.',
  off: 'Only used for staves, or when weapons are carried as items (Rules screen).',
  none: 'No weapon or staff has a group yet.',
  byClass: (g: string) => `${g} (their class can already)`,
  classHas: (groups: string[]) => `Their class already wields: ${groups.join(', ')}.`,
}
