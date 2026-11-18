// Units that share a square and a story name because they are for different difficulties (docs/DIFFICULTY.md "One unit,
// different numbers per difficulty"). Kept free of other imports: the map model and the checks both use it.

/** Can two placements be in one battle? Their "only on" lists (missing = every difficulty): null when they never meet (disjoint
 *  lists), else the difficulties both exist on (an empty list = both are unrestricted, so every difficulty). The loader's rule
 *  for sharing a square or a story name (docs/DIFFICULTY.md "One unit, different numbers per difficulty"). */
export function coexistOn(a: string[] | undefined, b: string[] | undefined): string[] | null {
  if (a === undefined && b === undefined) return []
  if (a === undefined) return [...b!]
  if (b === undefined) return [...a]
  const both = a.filter((x) => b.includes(x))
  return both.length ? both : null
}
