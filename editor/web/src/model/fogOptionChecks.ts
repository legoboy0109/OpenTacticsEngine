// Checks on the fog options of the data files (docs/ECONOMY.md "Fog of war"): terrain "blocksSight", a structure kind's "light", a torch item's
// "light" and the skill effect "vision". They mirror the loader (engine/core/data/loader_fog.cpp, loader_items.cpp, loader_skill_blocks.cpp).
import type { FogCheck } from './fogChecks'
import type { ProjectDoc } from './types'

const isInt = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)

export function fogOptionChecks(doc: ProjectDoc, out: FogCheck[]): void {
  for (const t of doc.terrain ?? [])
    if ((t as { blocksSight?: unknown }).blocksSight !== undefined && typeof t.blocksSight !== 'boolean') out.push({ section: 'terrain', text: `${t.name}: “blocks sight” must be on or off.` })
  for (const s of doc.structures ?? [])
    if (s.light !== undefined && (!isInt(s.light) || s.light < 1 || s.light > 10)) out.push({ section: 'structures', text: `${s.name}: the light must be a whole number from 1 to 10 (leave it out for no light).` })
  for (const it of doc.items ?? []) {
    if (it.light === undefined) continue
    const say = (text: string) => out.push({ section: 'items', text: `${it.name}: ${text}` })
    const l = it.light
    if (it.kind !== 'usable') say('only a usable item can give light.')
    if (typeof l !== 'object' || l === null) { say('the light must have a radius and, if you like, turns and shrink.'); continue }
    if (!isInt(l.radius) || l.radius < 1 || l.radius > 10) say('the light’s radius must be a whole number from 1 to 10.')
    if (l.turns !== undefined && (!isInt(l.turns) || l.turns < 1 || l.turns > 10)) say('the light’s turns must be a whole number from 1 to 10.')
    if (l.shrink !== undefined && (!isInt(l.shrink) || l.shrink < 1 || l.shrink > 10 || (isInt(l.radius) && l.shrink >= l.radius))) say('the light’s shrink must be a whole number from 1 to one less than its radius.')
    if (it.target === 'ally') say('a light item lights the square of whoever uses it, so its target must be “the carrier”.')
    if (doc.maps && !Object.values(doc.maps).some((m) => m?.fog === true)) out.push({ section: 'items', text: `${it.name}: no map has fog of war, so a light does nothing yet.` })
  }
  // The skill effect "vision" is kept and judged here; the Skills screen does not offer it (see docs/EDITOR_UI.md "Map changes and fog options").
  for (const sk of doc.skills ?? [])
    (sk.blocks ?? []).forEach((b, bi) => {
      for (const e of ((b as { do?: Array<Record<string, unknown>> }).do ?? [])) {
        const v = e.vision
        if (v === undefined) continue
        if (!isInt(v) || v < -10 || v > 10 || v === 0) out.push({ section: 'skills', text: `${sk.name}: block ${bi + 1}: the vision must be a whole number from -10 to 10, not 0.` })
        else if ((b as { when?: unknown }).when !== undefined && (b as { when?: unknown }).when !== 'always') out.push({ section: 'skills', text: `${sk.name}: block ${bi + 1}: vision belongs in an “always” block.` })
      }
    })
}
