// What the loader refuses about carrying / pair up (docs/SRPG.md section 5), said in plain words for the Rules screen.
import type { SrpgCheck } from './abilityChecks'
import { rescueMode, rescueOf } from './rescue'
import type { ProjectDoc } from './types'

export function rescueChecks(doc: ProjectDoc): SrpgCheck[] {
  const out: SrpgCheck[] = []
  const stats = new Set((doc.game?.stats ?? []).map((s) => s.id))
  const hp = doc.game?.roles.hp
  const r = rescueOf(doc)
  const say = (text: string) => out.push({ section: 'rules', text })
  const checkStats = (what: string, keys: string[], section: SrpgCheck['section'] = 'rules', ids?: string[]) => {
    for (const k of keys) {
      if (!stats.has(k)) out.push({ section, ids, text: `${what}: “${k}” is not a stat of this game.` })
      else if (k === hp) out.push({ section, ids, text: `${what}: the health stat cannot be changed here.` })
    }
  }
  if (r) {
    checkStats('While carrying', Object.keys(r.carrierStats ?? {}))
    checkStats('The partner behind gives', Object.keys(r.pairBonus ?? {}))
    if (r.pairBonus && rescueMode(doc) !== 'pairUp') say('The partner’s bonus belongs to Pair up, but the game is not set to Pair up.')
  }
  for (const u of doc.units ?? []) {
    if (u.pairBonus && rescueMode(doc) !== 'pairUp') out.push({ section: 'units', ids: [u.id], text: `${u.name}: “gives when behind” needs the game to be set to Pair up on the Rules screen.` })
    checkStats(`${u.name}, gives when behind`, Object.keys(u.pairBonus ?? {}), 'units', [u.id])
  }
  return out
}
