// All wording of the Quotes card of a character (placeholders: the project owner reviews it).
import type { QuoteOnce } from '../../../model/quotes'

export const QT = {
  title: 'Quotes',
  help: 'What this person says in a fight. The lines show in the dialogue box, with the speaker’s name and picture. A line with no speaker is said by this character; pick someone else to let a friend answer.',

  battleTitle: 'When it fights',
  battleHelp: 'Said when this character is in a fight on a map, before the first blow. A quote against one character wins over a quote against anyone. When both sides of a fight have a quote, the one attacked speaks first.',
  noBattle: 'No battle quotes yet.',
  add: 'Add a battle quote',
  quote: (n: number) => `Battle quote ${n}`,
  against: 'Fighting',
  againstHelp: 'Pick one character to say this only when they fight each other.',
  anyone: 'Anyone',
  once: 'How often',
  onceChoices: [
    { value: 'map', label: 'Once on each map' },
    { value: 'campaign', label: 'Once in the whole campaign' },
    { value: 'always', label: 'Every time' },
  ] as Array<{ value: QuoteOnce; label: string }>,
  onceHelp: 'Once on each map is the usual choice. In a game without a campaign, “whole campaign” means once per battle.',
  lines: 'Lines',
  linesName: (n: number) => `Battle quote ${n}`,
  moveUp: (n: number) => `Move battle quote ${n} up`,
  moveDown: (n: number) => `Move battle quote ${n} down`,
  remove: (n: number) => `Remove battle quote ${n}`,

  defeatTitle: 'When it is defeated',
  defeatHelp: 'Said right after this character is struck down in a fight, before the story’s own “defeated” events. A boss’s lines play before the victory.',
  fallsOnly: 'Defeat quote',
  fallsOnlyHelp: 'Said when this character is struck down.',
  falls: 'When it falls for good',
  fallsHelp: 'An enemy, or a unit of another side that does not come back.',
  retreats: 'When it retreats',
  retreatsHelp: 'One of your own units. In this game defeated units come back next chapter.',
  noLines: 'No lines. Nothing is said.',
  fallsName: 'Defeat quote',
  retreatsName: 'Retreat quote',
}
