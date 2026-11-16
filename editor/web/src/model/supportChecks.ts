// Editor-side checks for supports (docs/SRPG.md "Checks / Problems"), beside the loader's: the loader refuses what cannot load; these
// point at what loads but will never do anything. Each check says which pair it is about (`pair` = `pairKey`).
import { sceneCommands } from './campaignScene'
import { forEachCommand } from './storyWalk'
import { levelName, pairKey, pairLabel, pairsOf, samePair } from './supports'
import type { ProjectDoc } from './types'

export interface SupportCheck { pair: string; text: string }

/** Which characters each map has: placed ones and ones an event adds. */
function castOf(doc: ProjectDoc): Array<Set<string>> {
  const out: Array<Set<string>> = []
  for (const m of Object.values(doc.maps)) {
    if (!m) continue
    const cast = new Set<string>()
    for (const u of m.units ?? []) if (typeof u.character === 'string') cast.add(u.character)
    for (const e of m.events ?? []) forEachCommand(e.do, '', (c) => { if (typeof c.addUnit?.character === 'string') cast.add(c.addUnit.character) })
    out.push(cast)
  }
  return out
}

/** True when some map event or campaign scene gives points to the pair (`addSupport`), so a level can be reached without any rule. */
function storyGivesPoints(doc: ProjectDoc, a: string, b: string): boolean {
  let found = false
  const look = (list: Parameters<typeof forEachCommand>[0]) => forEachCommand(list, '', (c) => {
    if (Array.isArray(c.addSupport) && samePair({ a: String(c.addSupport[0]), b: String(c.addSupport[1]) }, a, b) && (c.by ?? 0) > 0) found = true
  })
  for (const m of Object.values(doc.maps)) for (const e of m?.events ?? []) look(e.do)
  for (const ch of doc.campaign?.chapters ?? []) { look(sceneCommands(ch.before)); look(sceneCommands(ch.after)) }
  return found
}

export function supportChecks(doc: ProjectDoc): SupportCheck[] {
  const pairs = pairsOf(doc)
  if (pairs.length === 0) return []
  const out: SupportCheck[] = []
  const rules = doc.supports?.rules
  const sources = rules?.points ?? {}
  const anySource = !!(sources.endTurnAdjacent || sources.fightTogether || sources.heal)
  // A map that could not be read might hold both characters: say nothing about meeting rather than something wrong.
  const mapsKnown = !Object.values(doc.maps).some((m) => m === null)
  const casts = castOf(doc)
  const hasTalk = pairs.some((p) => p.levels?.some((l) => l.talk?.length))
  const noCamp = (doc.campaign?.chapters?.length ?? 0) === 0

  for (const p of pairs) {
    const key = pairKey(p)
    if (mapsKnown && !casts.some((c) => c.has(p.a) && c.has(p.b))) out.push({ pair: key, text: 'These two never meet: no map has both of them.' })
    const start = p.start ?? 0
    if (!anySource && !storyGivesPoints(doc, p.a, p.b)) {
      const stuck = (p.levels ?? []).findIndex((l) => l.points > start)
      if (stuck >= 0) out.push({ pair: key, text: `Level ${levelName(rules, stuck)} can never be reached: no way of earning points is switched on, and no story event gives points to ${pairLabel(doc, p)}.` })
    }
  }
  if (hasTalk && rules?.conversations === 'camp' && noCamp) {
    const first = pairs.find((p) => p.levels?.some((l) => l.talk?.length))!
    out.push({ pair: pairKey(first), text: 'Conversations are only played between chapters, and this game has no campaign, so none of them will ever be seen.' })
  }
  return out
}
