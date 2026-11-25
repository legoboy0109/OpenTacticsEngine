// Wording of the Dialogue style card on the Rules screen (placeholder wording for the owner to review; docs/EDITOR_UI.md).
import type { BubblePortrait, DialogueStyle } from '../../model/dialogueStyle'

export const DS = {
  title: 'Dialogue style',
  help: 'How story lines are shown. Only the look changes: the rules, saves and computer players are not affected.',
  style: 'Look of a line',
  styleChoices: {
    box: { label: 'A box at the bottom', help: 'Every line appears in the box at the bottom of the screen, as always.' },
    bubble: { label: 'A speech bubble', help: 'A line said by a unit standing on the map appears in a bubble pointing at that unit. Lines with nobody to point at (a narrator, a name with no unit, a scene with no map) still use the box.' },
  } as Record<DialogueStyle, { label: string; help: string }>,
  portrait: 'Portrait in a bubble',
  portraitChoices: {
    beside: { label: 'Beside the bubble', help: 'The portrait sits next to the bubble.' },
    inside: { label: 'Inside the bubble', help: 'The portrait is drawn inside the bubble.' },
    none: { label: 'None', help: 'Bubbles show no portrait.' },
  } as Record<BubblePortrait, { label: string; help: string }>,
  names: 'Show the speaker’s name on the bubble',
  namesHelp: 'The name goes on the bubble’s first row.',
}
