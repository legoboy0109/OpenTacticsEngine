// Which commands of an event run "on another map" (docs/EVENTS.md "Showing another map"): the top-level commands after a `showMap` up to the
// matching `returnMap` (or the end of the event), and everything nested inside them. No document is needed, so the story walk can use it.
// The loader is engine/core/data/loader_story_show_map.cpp: it reads a section against the shown map's units, sides and size.
import { commandKind, type Command } from './storyTypes'

/** One `showMap` ... `returnMap` stretch of an event's own command list. */
export interface RawSection {
  /** The showMap command that opens it. */
  show: Command
  /** The returnMap that closes it (missing = the end of the event closes it). */
  back?: Command
  /** Every command of the section, nested ones included (not the showMap / returnMap themselves). */
  cmds: Command[]
  /** A second showMap before the returnMap (the loader refuses it). */
  again: Command[]
}

const nested = (c: Command): Command[] => [...(c.then ?? []), ...(c.else ?? []), ...(c.answers ?? []).flatMap((a) => a.do ?? [])]
function collect(list: Command[] | undefined, into: Command[]): void {
  for (const c of Array.isArray(list) ? list : []) { into.push(c); collect(c.then, into); collect(c.else, into); for (const a of c.answers ?? []) collect(a.do, into) }
}

/** The sections of one command list (an event's `do`), and the returnMaps that close nothing. */
export function rawSections(list: Command[] | undefined): { sections: RawSection[]; stray: Command[] } {
  const sections: RawSection[] = []
  const stray: Command[] = []
  let open: RawSection | null = null
  for (const c of Array.isArray(list) ? list : []) {
    const k = commandKind(c)
    if (k === 'showMap') {
      if (open) open.again.push(c)
      else { open = { show: c, cmds: [], again: [] }; sections.push(open) }
    } else if (k === 'returnMap') {
      if (open) { open.back = c; open = null } else stray.push(c)
    } else if (open) {
      collect([c], open.cmds)
    }
  }
  return { sections, stray }
}

/** Every command that acts on a shown map, plus the showMap commands themselves (their `focus` is a unit of the shown map). */
export function shownMembers(list: Command[] | undefined): Set<Command> {
  const out = new Set<Command>()
  for (const s of rawSections(list).sections) { out.add(s.show); for (const c of s.cmds) out.add(c) }
  return out
}

export { nested as nestedCommands }
