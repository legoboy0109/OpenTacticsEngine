// Wording that says "square" on a square game and "hexagon" on a hexagon game. The screens' texts are written for squares;
// on a hexagon game they pass through `cellWords` so "Click a square" reads "Click a hexagon" (plain words, one place).
import { useMemo } from 'react'
import { gridOfGame, isHex } from '../model/gridSpec'
import { useDoc } from '../store/context'

export function cellWords(hex: boolean): (s: string) => string {
  if (!hex) return (s) => s
  return (s) => s.replace(/\bSquares\b/g, 'Hexagons').replace(/\bsquares\b/g, 'hexagons').replace(/\bSquare\b/g, 'Hexagon').replace(/\bsquare\b/g, 'hexagon')
}

/** True when the open game is a hexagon game. */
export function useIsHex(): boolean {
  const game = useDoc().game
  return isHex(gridOfGame(game))
}

/** `cellWords` for the open game. */
export function useCellWords(): (s: string) => string {
  const hex = useIsHex()
  return useMemo(() => cellWords(hex), [hex])
}
