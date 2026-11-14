// What a square of the map really does when a structure stands on it. One place for the engine's combination rule
// (engine/core/data/database.cpp: enterCostOf and squareNumbers; docs/STRUCTURES.md "Rules"):
//   - movement: for each movement type the structure's cost replaces the terrain's when the structure lists one
//     (0 = cannot enter); otherwise the terrain's applies (a terrain that lists none costs 1);
//   - numbers (defense, avoid...): the terrain's numbers, with the structure's numbers of the same name replacing
//     them and the structure's other numbers added.
// Used by the Select tool, the map checks and unit placement, so they cannot disagree with the game.
import type { Structure, Terrain, Unit } from './types'

export type Source = 'ground' | 'structure'
export interface Cost { cost: number; from: Source }
export interface EffectiveNumber { key: string; value: number; from: Source }

/** A terrain that lists no cost for a movement type costs 1 to enter. */
export const DEFAULT_COST = 1

/** Cost of entering a square for one movement type. `cost <= 0` means it cannot be entered. */
export function enterCost(terrain: Terrain | undefined, structure: Structure | undefined, moveType: string): Cost {
  const own = structure?.moveCost?.[moveType]
  if (own !== undefined && own >= 0) return { cost: own, from: 'structure' }
  return { cost: terrain?.moveCost?.[moveType] ?? DEFAULT_COST, from: 'ground' }
}

/** True unless the unit's movement type cannot enter the square. A missing unit or terrain counts as enterable (nothing to judge). */
export function canEnter(terrain: Terrain | undefined, structure: Structure | undefined, unit: Unit | undefined): boolean {
  if (!terrain || !unit) return true
  return enterCost(terrain, structure, unit.moveType).cost > 0
}

/** The numbers formulas see on a square, each marked with where it comes from. Terrain order first, then added ones. */
export function effectiveNumbers(terrain: Terrain | undefined, structure: Structure | undefined): EffectiveNumber[] {
  const out: EffectiveNumber[] = Object.entries(terrain?.props ?? {}).map(([key, value]) => ({ key, value, from: 'ground' as Source }))
  for (const [key, value] of Object.entries(structure?.props ?? {})) {
    const at = out.findIndex((n) => n.key === key)
    if (at >= 0) out[at] = { key, value, from: 'structure' }
    else out.push({ key, value, from: 'structure' })
  }
  return out
}
