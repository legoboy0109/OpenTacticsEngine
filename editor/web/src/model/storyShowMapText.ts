// The sentence for a showMap command in the command list.
import { squareText, unitText, type SumCtx } from './storySummary'
import type { Command } from './storyTypes'

export function showMapText(c: SumCtx, cmd: Command): string {
  const id = typeof cmd.showMap === 'string' ? cmd.showMap : ''
  const name = id === '' ? 'no map yet' : `“${c.doc.game?.maps.find((m) => m.id === id)?.name || id}”`
  const f = cmd.focus
  const camera = typeof f === 'string' ? `, the view starts on ${f === '' ? 'a unit' : unitText({ ...c, map: c.doc.maps[id] ?? c.map }, f)}` : f && typeof f === 'object' ? `, the view starts at ${squareText(f.x, f.y)}` : ''
  return `Shows the map ${name}${camera}`
}
