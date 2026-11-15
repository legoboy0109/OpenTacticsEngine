// Where the story can go: each chapter leads to its "Next chapter" (or, when none is set, the following one) and to every
// chapter an "After the battle" scene sends it to with "Go to a chapter". The flow preview, the "nothing leads to this
// chapter" hint and the checks all read this one place. (The first chapter is where the story starts.)
import { chapterName, type Campaign } from './campaignTypes'
import { sceneCommands } from './campaignScene'
import { forEachCommand } from './storyWalk'

export interface FlowTarget { id: string; name: string; kind: 'next' | 'goto'; known: boolean }
export interface FlowRow {
  id: string
  name: string
  /** 1-based place in the list. */
  number: number
  leadsTo: FlowTarget[]
  /** The last chapter with nothing after it: the story ends here. */
  ends: boolean
  /** No chapter leads here (and it is not the first). */
  unreached: boolean
}

/** The chapter ids each chapter leads to. */
export function chapterLinks(c: Campaign): Map<string, Set<string>> {
  const out = new Map<string, Set<string>>()
  c.chapters.forEach((ch, i) => {
    const to = new Set<string>()
    const nextId = ch.next || c.chapters[i + 1]?.id
    if (nextId) to.add(nextId)
    forEachCommand(sceneCommands(ch.after), '', (cmd) => { if (typeof cmd.goto === 'string' && cmd.goto) to.add(cmd.goto) })
    out.set(ch.id, to)
  })
  return out
}

/** Chapters (other than the first) that no chapter leads to. */
export function unreachedChapters(c: Campaign): string[] {
  const led = new Set<string>()
  for (const to of chapterLinks(c).values()) for (const id of to) led.add(id)
  return c.chapters.slice(1).filter((ch) => !led.has(ch.id)).map((ch) => ch.id)
}

export function flowRows(c: Campaign): FlowRow[] {
  const unreached = new Set(unreachedChapters(c))
  const byId = new Map(c.chapters.map((ch) => [ch.id, ch]))
  return c.chapters.map((ch, i) => {
    const targets: FlowTarget[] = []
    const add = (id: string, kind: FlowTarget['kind']) => {
      if (!id || targets.some((t) => t.id === id)) return
      const t = byId.get(id)
      targets.push({ id, name: t ? chapterName(t) : id, kind, known: !!t })
    }
    add(ch.next || c.chapters[i + 1]?.id || '', 'next')
    forEachCommand(sceneCommands(ch.after), '', (cmd) => { if (typeof cmd.goto === 'string') add(cmd.goto, 'goto') })
    return { id: ch.id, name: chapterName(ch), number: i + 1, leadsTo: targets, ends: !ch.next && i === c.chapters.length - 1, unreached: unreached.has(ch.id) }
  })
}
