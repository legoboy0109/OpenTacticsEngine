// How an image is divided: the first guess when it is added, and what the grid means for an image size.

export interface Division { mode: 'one' | 'grid'; cellWidth: number; cellHeight: number }

/** At most twice the tile size both ways: one picture. Otherwise a grid of the tile size if that divides the image,
 *  else the first of 16, 32, 24, 48 that divides both sides, else the tile size anyway. */
export function guessDivision(width: number, height: number, tileSize: number): Division {
  const tile = Math.max(1, Math.round(tileSize) || 24)
  if (width <= tile * 2 && height <= tile * 2) return { mode: 'one', cellWidth: width, cellHeight: height }
  const size = gridSizeFor(width, height, tile)
  return { mode: 'grid', cellWidth: size, cellHeight: size }
}

/** The square piece size a grid should start with (also used when someone switches "One picture" to a grid). */
export function gridSizeFor(width: number, height: number, tileSize: number): number {
  const tile = Math.max(1, Math.round(tileSize) || 24)
  const fits = (n: number) => width % n === 0 && height % n === 0
  return fits(tile) ? tile : [16, 32, 24, 48].find(fits) ?? tile
}

export interface GridInfo { cols: number; rows: number; extraRight: number; extraBottom: number }

export function gridInfo(width: number, height: number, cellWidth: number, cellHeight: number): GridInfo {
  const cols = Math.floor(width / Math.max(1, cellWidth))
  const rows = Math.floor(height / Math.max(1, cellHeight))
  return { cols, rows, extraRight: width - cols * cellWidth, extraBottom: height - rows * cellHeight }
}

/** One picture = a single piece that covers the whole image. */
export const isOnePicture = (width: number, height: number, cellWidth: number, cellHeight: number): boolean => cellWidth >= width && cellHeight >= height

export const QUICK_SIZES = [16, 24, 32, 48]
