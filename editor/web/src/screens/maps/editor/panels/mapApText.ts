// Wording of the map's own ability points (placeholders: the owner reviews).
export const AP = {
  label: (name: string) => `${name} for a won battle (this map)`,
  help: (game: number | undefined) => `Replaces the game’s number for battles on this map${game === undefined ? '' : ` (the game gives ${game})`}. Empty = the game’s number. 0 = none here.`,
}
