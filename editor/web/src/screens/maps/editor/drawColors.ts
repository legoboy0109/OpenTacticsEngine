// Colour helpers shared by the map drawing code (drawMap.ts, drawStructures.ts).
export const FONT = 'system-ui, -apple-system, "Segoe UI", Roboto, sans-serif'

export type Rgb = [number, number, number]
export function parseHex(c: string | undefined, fallback: Rgb = [128, 128, 128]): Rgb {
  const m = /^#?([0-9a-f]{6})$/i.exec(c ?? '')
  if (!m) return fallback
  const n = parseInt(m[1], 16)
  return [n >> 16, (n >> 8) & 255, n & 255]
}
export const css = (c: Rgb, a = 1) => `rgba(${c[0]},${c[1]},${c[2]},${a})`
const luma = (c: Rgb) => 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
/** Black or white, whichever reads on `c` (the game's readableOn). */
export const readableOn = (c: Rgb): Rgb => (luma(c) > 140 ? [18, 18, 24] : [245, 245, 250])
export const mix = (a: Rgb, b: Rgb, t: number): Rgb => [0, 1, 2].map((i) => Math.round(a[i] + (b[i] - a[i]) * t)) as Rgb

