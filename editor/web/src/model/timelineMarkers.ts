// "What changes" counts for a list of commands: units joining or leaving, items given, switches / variables set, questions.
// (A shop is not a command: it is a structure on the map, counted by `shopCount`.)
import { commandKind, type Command } from './storyTypes'
import { forEachCommand } from './storyWalk'
import type { MapFile, ProjectDoc } from './types'
import type { Markers } from './timelineTypes'

export const noMarkers = (): Markers => ({ join: 0, leave: 0, items: 0, shop: 0, flags: 0, questions: 0 })

export function addMarkers(a: Markers, b: Markers): Markers {
  return { join: a.join + b.join, leave: a.leave + b.leave, items: a.items + b.items, shop: a.shop + b.shop, flags: a.flags + b.flags, questions: a.questions + b.questions }
}

/** Counts through `if` branches and every answer (a nested question counts too). */
export function countMarkers(list: Command[] | undefined): Markers {
  const m = noMarkers()
  forEachCommand(list, '', (c) => {
    switch (commandKind(c)) {
      case 'addUnit': m.join++; break
      case 'removeUnit': m.leave++; break
      case 'giveItem': m.items++; break
      case 'setSwitch': case 'setVariable': case 'addVariable': m.flags++; break
      case 'choice': m.questions++; break
      default: break
    }
  })
  return m
}

/** Structures on the map whose kind sells items (docs/ITEMS.md "Shops"). */
export function shopCount(doc: ProjectDoc, map: MapFile | null): number {
  if (!map?.structures?.length || !doc.structures?.length) return 0
  const sells = new Set(doc.structures.filter((s) => (s.sells?.length ?? 0) > 0).map((s) => s.id))
  return map.structures.filter((s) => sells.has(s.type)).length
}

export const hasMarkers = (m: Markers): boolean => Object.values(m).some((n) => n > 0)
