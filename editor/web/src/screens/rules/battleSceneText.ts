// Wording of the battle scene's background and side choices on the Rules screen (placeholder wording for the owner to review).
import type { SceneSides } from '../../model/battleTables'

export const BC = {
  from: 'Background and platform come from',
  fromHelp: 'Which fighter’s square decides the picture behind a fight and the platform under the fighters.',
  fromDefender: 'The fighter that is attacked',
  fromAttacker: 'The fighter that attacks',
  sides: 'Which side each fighter stands on',
  sidesHelp: 'In the battle scene, which fighter stands on the left and which on the right.',
  sidesChoices: { map: 'By their places on the map', playerRight: 'Your fighter on the right', playerLeft: 'Your fighter on the left' } as Record<SceneSides, string>,
  sidesChoicesHelp: {
    map: 'The fighter standing nearer the left edge of the map is on the left (the one who attacks, if they are in the same column).',
    playerRight: 'A fighter controlled by a player always stands on the right and the computer’s fighter on the left. If both or neither are the player’s, the map decides.',
    playerLeft: 'A fighter controlled by a player always stands on the left and the computer’s fighter on the right. If both or neither are the player’s, the map decides.',
  } as Record<SceneSides, string>,
}
