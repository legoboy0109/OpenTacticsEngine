// Dims the squares a side's team would not see at the start (the "Show what a side sees" aid; model/fogSight.ts).
// Device pixels, one fill per square so a dimmed area has no seams.
export function drawFogDim(g: CanvasRenderingContext2D, seen: Uint8Array, mw: number, x0: number, x1: number, y0: number, y1: number, px: (c: number) => number, py: (c: number) => number): void {
  g.fillStyle = 'rgba(6,8,18,0.58)'
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      if (!seen[y * mw + x]) g.fillRect(px(x), py(y), px(x + 1) - px(x), py(y + 1) - py(y))
}
