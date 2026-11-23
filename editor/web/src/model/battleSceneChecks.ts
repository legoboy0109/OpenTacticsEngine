// Checks of the two game.json choices of the battle scene (docs/BATTLE_SCENE.md): what the loader refuses, and a note where a choice has
// nothing to do. (Kept apart from battleTables.ts, which holds the edits.)
import { SCENE_SIDES, platformFrom, platformList, sceneSides, sceneStaves } from './battleTables'
import { staffItems } from './battleEffects'
import type { ProjectDoc } from './types'

export function sceneChoiceProblems(doc: ProjectDoc): string[] {
  const g = doc.game
  const out: string[] = []
  if (g?.battleBackgroundFrom !== undefined && g.battleBackgroundFrom !== 'defender' && g.battleBackgroundFrom !== 'attacker') out.push('“Background and platform come from” must be the attacked fighter or the attacker.')
  if (g?.battlePlatformFrom !== undefined && g.battlePlatformFrom !== 'same' && g.battlePlatformFrom !== 'own') out.push('“Each fighter’s platform” must come from the same square as the background or from the fighter’s own square.')
  if (platformFrom(doc) === 'own' && platformList(doc).length === 0) out.push('The game has no platforms yet, so this changes nothing: it only decides which platform each fighter stands on.')
  if (g?.battleSceneSides !== undefined && !SCENE_SIDES.includes(g.battleSceneSides)) out.push('“Which side each fighter stands on” must be by the map, your fighter on the right or your fighter on the left.')
  if (g?.battleSceneStaves !== undefined && typeof g.battleSceneStaves !== 'boolean') out.push('“Staves in the battle scene” must be on or off.')
  if (g?.quotesPerFight !== undefined && g.quotesPerFight !== 'both' && g.quotesPerFight !== 'one') out.push('“Battle quotes per fight” must be both fighters or one.')
  if (sceneStaves(doc) && staffItems(doc).length === 0) out.push('The game has no staves yet, so this changes nothing: it only applies to a staff used on one other unit.')
  if (sceneSides(doc) !== 'map') {
    const maps = Object.values(doc.maps).filter((m): m is NonNullable<typeof m> => !!m)
    const mixed = maps.some((m) => (m.sides ?? []).some((s) => s.controller === 'ai') && (m.sides ?? []).some((s) => s.controller !== 'ai'))
    if (maps.length > 0 && !mixed) out.push('No map has both a computer side and a player side, so this choice changes nothing yet: fights between sides of one kind are placed by the map.')
  }
  return out
}
