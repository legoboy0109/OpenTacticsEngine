// Reading and writing the combat formulas in the game document. The chosen
// preset and its parameters are remembered under game.editor.formulas; the
// engine only ever sees the generated expression.
import { FORMULA_DEFS, FORMULA_KEYS, type FormulaKey } from './presetDefs'
import { CUSTOM, detect, fillParams, generate, missingStats, presetOf, summarize, type Ctx } from './presets'
import type { FormulaParams, FormulaState, Game, ProjectDoc } from './types'

export const ctxOf = (game: Game): Ctx => ({ stats: game.stats, roles: game.roles })

export function readExpr(game: Game, key: FormulaKey): string | undefined {
  if (key === 'xpHit' || key === 'xpKill') return game.rules?.progression?.[key]
  if (key === 'capture') return game.rules?.capture?.amount
  return game.formulas?.[key]
}

/** Writes (or, for undefined, removes) the expression, creating parents as needed. */
export function writeExpr(game: Game, key: FormulaKey, expr: string | undefined): Game {
  const put = <T extends object>(obj: T | undefined, k: string): T => {
    const c: Record<string, unknown> = { ...(obj ?? {}) }
    if (expr === undefined) delete c[k]; else c[k] = expr
    return c as T
  }
  if (key === 'xpHit' || key === 'xpKill') return { ...game, rules: { ...game.rules, progression: put(game.rules?.progression, key) } }
  if (key === 'capture') return { ...game, rules: { ...game.rules, capture: put(game.rules?.capture, 'amount') } }
  return { ...game, formulas: put(game.formulas, key) }
}

function storeState(game: Game, key: FormulaKey, state: FormulaState): Game {
  const old = game.editor?.formulas?.[key]
  return { ...game, editor: { ...game.editor, formulas: { ...game.editor?.formulas, [key]: { ...old, ...state } } } }
}

export interface FormulaView {
  key: FormulaKey
  preset: string
  params: FormulaParams
  expr: string | undefined
  summary: string
  /** Labels of stat choices that point at nothing (needs the user's attention). */
  missing: string[]
}

export function formulaView(game: Game, key: FormulaKey): FormulaView {
  const ctx = ctxOf(game)
  const def = FORMULA_DEFS[key]
  const expr = readExpr(game, key)
  const found = detect(def, expr, game.editor?.formulas?.[key], ctx)
  const preset = presetOf(key, found.preset)
  if (!preset) return { key, preset: CUSTOM, params: {}, expr, summary: 'A hand-written formula (see Advanced).', missing: [] }
  return {
    key, preset: preset.id, params: found.params, expr,
    summary: summarize(preset, found.params, ctx),
    missing: missingStats(preset, found.params, ctx).map((d) => d.label),
  }
}

/** Which formulas matter right now (experience / capture ones need their feature on). */
export function activeKeys(game: Game): FormulaKey[] {
  return FORMULA_KEYS.filter((k) => {
    if (k === 'xpHit' || k === 'xpKill') return game.rules?.progression?.enabled === true
    if (k === 'capture') return game.rules?.capture?.enabled === true
    return true
  })
}

export function attentionList(game: Game): Array<{ key: FormulaKey; label: string; missing: string[] }> {
  return activeKeys(game)
    .map((key) => ({ key, label: FORMULA_DEFS[key].label, missing: formulaView(game, key).missing }))
    .filter((a) => a.missing.length > 0)
}

const mapGame = (doc: ProjectDoc, fn: (g: Game) => Game): ProjectDoc => (doc.game ? { ...doc, game: fn(doc.game) } : doc)

const CUSTOM_STARTER: Partial<Record<FormulaKey, string>> = { damage: '1', hit: '100', crit: '0', strikes: '1' }

function applyParams(game: Game, key: FormulaKey, presetId: string, params: FormulaParams): Game {
  const preset = presetOf(key, presetId)!
  const full = fillParams(preset, params, ctxOf(game))
  const expr = generate(preset, full, ctxOf(game))
  return storeState(writeExpr(game, key, expr ?? undefined), key, { preset: presetId, params: full })
}

/** Choose a preset; parameters with the same name carry over from the old one. */
export function setFormulaPreset(doc: ProjectDoc, key: FormulaKey, presetId: string): ProjectDoc {
  return mapGame(doc, (game) => {
    const current = formulaView(game, key)
    if (current.preset === presetId) return game
    if (presetId === CUSTOM) {
      const keep = current.expr ?? CUSTOM_STARTER[key]
      return storeState(writeExpr(game, key, keep), key, { preset: CUSTOM, params: undefined })
    }
    if (!presetOf(key, presetId)) return game
    return applyParams(game, key, presetId, current.preset === CUSTOM ? {} : current.params)
  })
}

export function setFormulaParam(doc: ProjectDoc, key: FormulaKey, paramId: string, value: string | number | boolean): ProjectDoc {
  return mapGame(doc, (game) => {
    const v = formulaView(game, key)
    if (v.preset === CUSTOM) return game
    return applyParams(game, key, v.preset, { ...v.params, [paramId]: value })
  })
}

export function setFormulaExpression(doc: ProjectDoc, key: FormulaKey, text: string): ProjectDoc {
  if (text.trim() === '') return doc
  return mapGame(doc, (game) => storeState(writeExpr(game, key, text), key, { preset: CUSTOM, params: undefined }))
}

export function setCritMultiplier(doc: ProjectDoc, value: number | undefined): ProjectDoc {
  return mapGame(doc, (game) => {
    const f: Record<string, unknown> = { ...game.formulas }
    if (value === undefined) delete f.critMultiplier; else f.critMultiplier = value
    return { ...game, formulas: f }
  })
}

/** Formulas whose chosen presets name this stat (used by delete confirmations). */
export function formulasUsingStat(game: Game, statId: string): FormulaKey[] {
  return activeKeys(game).filter((k) => {
    const v = formulaView(game, k)
    const preset = presetOf(k, v.preset)
    return !!preset && preset.params.some((d) => d.kind === 'stat' && v.params[d.id] === statId)
  })
}

/** Custom expressions that mention the stat (`a.str` / `b.str`): cannot be fixed automatically. */
export function customFormulasMentioning(game: Game, statId: string): string[] {
  const re = new RegExp(`\\b[ab]\\.${statId}\\b`)
  return FORMULA_KEYS.filter((k) => formulaView(game, k).preset === CUSTOM && re.test(readExpr(game, k) ?? '')).map((k) => FORMULA_DEFS[k].label)
}

/** Before a stat is deleted: every preset using it forgets it (it then shows
 *  "needs attention" and writes 0 in its place, so the game still loads). */
export function repairFormulasForStat(game: Game, statId: string): Game {
  let g = game
  for (const key of formulasUsingStat(game, statId)) {
    const v = formulaView(g, key)
    const preset = presetOf(key, v.preset)!
    const cleared: FormulaParams = { ...v.params }
    for (const d of preset.params) if (d.kind === 'stat' && cleared[d.id] === statId) cleared[d.id] = ''
    // Not applyParams: fillParams would re-guess a stat for the blank slot.
    const expr = generate(preset, cleared, ctxOf({ ...g, stats: g.stats.filter((s) => s.id !== statId) }))
    g = storeState(writeExpr(g, key, expr ?? undefined), key, { preset: preset.id, params: cleared })
  }
  return g
}

export type DamageMode = 'multiplier' | 'percent'
export function damageMode(game: Game): DamageMode {
  return formulaView(game, 'damage').preset === 'damage-chart' ? 'percent' : 'multiplier'
}

/** Numbers a weapon should have for the chosen presets to work. */
export function suggestedWeaponProps(game: Game): Array<{ id: string; label: string; default: number }> {
  const out: Array<{ id: string; label: string; default: number }> = []
  const d = formulaView(game, 'damage')
  if (d.preset === 'attack-minus-defense' && d.params.weapon === true) out.push({ id: 'might', label: 'Might', default: 5 })
  const h = formulaView(game, 'hit')
  if (h.preset === 'accuracy' || h.preset === 'accuracy-vs-evasion') out.push({ id: 'hit', label: 'Accuracy', default: 90 })
  return out
}

/** Numbers a terrain should have for the chosen presets to work. */
export function suggestedTerrainProps(game: Game): Array<{ id: string; label: string }> {
  const out: Array<{ id: string; label: string }> = []
  const d = formulaView(game, 'damage')
  if ((d.preset === 'attack-minus-defense' || d.preset === 'damage-chart') && d.params.terrain === true) out.push({ id: 'defense', label: 'Defense' })
  const h = formulaView(game, 'hit')
  if (h.preset === 'accuracy-vs-evasion' && h.params.terrain === true) out.push({ id: 'avoid', label: 'Avoid' })
  return out
}
