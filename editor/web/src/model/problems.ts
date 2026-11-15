// Which section owns a problem the helper reported (by file path).
import type { ProjectDoc, Problem, Section } from './types'

export interface ProblemTarget { section: Section; itemId?: string }

export function problemTarget(file: string, doc: ProjectDoc | null): ProblemTarget {
  if (file === 'data/units.json') return { section: 'units' }
  if (file === 'data/weapons.json') return { section: 'weapons' }
  if (file === 'data/terrain.json') return { section: 'terrain' }
  if (file === 'data/items.json') return { section: 'items' }
  if (file === 'data/research.json') return { section: 'research' }
  if (file === 'data/campaign.json') return { section: 'campaign' }
  if (file === 'data/structures.json') return { section: 'structures' }
  if (file === 'data/graphics.json') return { section: 'art' }
  if (file === 'data/audio.json') return { section: 'sound' }
  if (file.startsWith('maps/')) {
    const ref = doc?.game?.maps.find((m) => m.file === file)
    return { section: 'maps', itemId: ref?.id }
  }
  // game.json, scripts and "unknown": the Rules screen holds the formulas,
  // the Game screen everything else.
  return { section: 'game' }
}

/** Problems that mean a whole section could not be read (its data is null). */
export function sectionLoadProblems(problems: Problem[], file: string): Problem[] {
  return problems.filter((p) => p.file === file)
}
