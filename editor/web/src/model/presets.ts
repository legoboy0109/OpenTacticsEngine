// Expression generation, recognition and summaries for formula presets.
// Pure functions over text; reading/writing the game document lives in
// formulas.ts.
import {
  FORMULA_DEFS, getNum, getStat,
  type FormulaDef, type FormulaKey, type ParamDef, type PresetDef, type StatParam, type Tok,
} from './presetDefs'
import type { FormulaParams, FormulaState, Named, Roles } from './types'

export const CUSTOM = 'custom'

/** What the presets need to know about the game. */
export interface Ctx { stats: Named[]; roles: Roles }
export interface Detected { preset: string; params: FormulaParams }

export function presetOf(key: FormulaKey, id: string): PresetDef | undefined {
  return FORMULA_DEFS[key].presets.find((p) => p.id === id)
}

const hasStat = (ctx: Ctx, id: string) => ctx.stats.some((s) => s.id === id)

/** Guess a stat for a slot ("attack stat" -> Strength) from names, then position. */
export function pickStat(def: StatParam, ctx: Ctx): string {
  const lower = (s: string) => s.toLowerCase()
  const byId = ctx.stats.find((s) => def.hints.includes(lower(s.id)))
  if (byId) return byId.id
  const byName = ctx.stats.find((s) => def.hints.includes(lower(s.name)))
  if (byName) return byName.id
  const plain = ctx.stats.filter((s) => s.id !== ctx.roles.hp && s.id !== ctx.roles.move)
  return plain[def.slot]?.id ?? ''
}

/** Complete parameter set: given values win, the rest get sensible defaults. */
export function fillParams(preset: PresetDef, given: FormulaParams | undefined, ctx: Ctx): FormulaParams {
  const out: FormulaParams = {}
  for (const d of preset.params) {
    const v = given?.[d.id]
    if (d.kind === 'stat') out[d.id] = typeof v === 'string' ? v : pickStat(d, ctx)
    else if (d.kind === 'number') out[d.id] = typeof v === 'number' ? v : d.default
    else out[d.id] = typeof v === 'boolean' ? v : d.default
  }
  return out
}

/** The Lua expression for a preset, or null when the key is to be removed.
 *  A stat that no longer exists becomes 0 so the game still loads. */
export function generate(preset: PresetDef, params: FormulaParams, ctx: Ctx): string | null {
  if (!preset.tokens) return null
  const p = fillParams(preset, params, ctx)
  return preset.tokens(p).map((t) => {
    if (typeof t === 'string') return t
    if (!t.who) return String(getNum(p, t.p, 0))
    const id = getStat(p, t.p)
    return hasStat(ctx, id) ? `${t.who}.${id}` : '0'
  }).join('')
}

/** Stat parameters that point at nothing: the user must pick one. */
export function missingStats(preset: PresetDef, params: FormulaParams, ctx: Ctx): ParamDef[] {
  return preset.params.filter((d) => d.kind === 'stat' && !hasStat(ctx, getStat(params, d.id)))
}

export function summarize(preset: PresetDef, params: FormulaParams, ctx: Ctx): string {
  const p = fillParams(preset, params, ctx)
  const nameOf = (id: string) => ctx.stats.find((s) => s.id === id)?.name ?? '(pick a stat)'
  return preset.summary(p, nameOf)
}

// --- recognising an expression -------------------------------------------

/** Spelling-insensitive form: spaces and the "or 0" defaults are ignored, so
 *  `(a.str + w.might) - b.def` is recognised as the same thing as the
 *  generated `(a.str + (w.might or 0)) - b.def`. */
export function canon(expr: string): string {
  return expr
    .replace(/\((w|tb)\.(\w+) or (?:0|100)\)/g, '$1.$2')
    .replace(/\s+/g, '')
}

const esc = (s: string) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')

/** Regex for one preset with the given toggle values, one capture group per
 *  parameter slot (a repeated slot becomes a back-reference). */
function patternFor(tokens: Tok[]): { re: RegExp; slots: string[] } {
  const slots: string[] = []
  let src = '^'
  for (const t of tokens) {
    if (typeof t === 'string') { src += esc(canon(t)); continue }
    const seen = slots.indexOf(t.p)
    if (seen >= 0) { src += `${t.who}\\.\\${seen + 1}`; continue }
    slots.push(t.p)
    src += t.who ? `${t.who}\\.([A-Za-z_]\\w*)` : '(-?\\d+(?:\\.\\d+)?)'
  }
  return { re: new RegExp(src + '$'), slots }
}

function boolCombos(preset: PresetDef): FormulaParams[] {
  const bools = preset.params.filter((d) => d.kind === 'bool')
  const out: FormulaParams[] = []
  for (let mask = 0; mask < 1 << bools.length; mask++) {
    const p: FormulaParams = {}
    bools.forEach((d, i) => { p[d.id] = Boolean(mask & (1 << i)) })
    out.push(p)
  }
  return out
}

/** Try to explain `canonExpr` as this preset; undefined when it does not fit. */
function matchPreset(preset: PresetDef, canonExpr: string, ctx: Ctx): FormulaParams | undefined {
  if (!preset.tokens) return undefined
  for (const bools of boolCombos(preset)) {
    const { re, slots } = patternFor(preset.tokens(fillParams(preset, bools, ctx)))
    const m = re.exec(canonExpr)
    if (!m) continue
    const params: FormulaParams = { ...bools }
    let ok = true
    slots.forEach((id, i) => {
      const def = preset.params.find((d) => d.id === id)
      if (def?.kind === 'stat') {
        if (hasStat(ctx, m[i + 1])) params[id] = m[i + 1]; else ok = false
      } else params[id] = Number(m[i + 1])
    })
    if (ok) return params
  }
  return undefined
}

/** Which preset does `expr` (undefined = key absent) correspond to? The
 *  stored choice wins when it still produces exactly this text; otherwise
 *  every preset is tried; otherwise "custom". */
export function detect(def: FormulaDef, expr: string | undefined, stored: FormulaState | undefined, ctx: Ctx): Detected {
  let text = expr
  if (text === undefined || text.trim() === '') {
    if (def.absent && 'preset' in def.absent) return { preset: def.absent.preset, params: {} }
    if (def.absent) text = def.absent.expr
  }
  if (text === undefined) return { preset: CUSTOM, params: {} }
  const c = canon(text)
  const alias = def.aliases?.[c]
  const target = alias ? canon(alias) : c

  const storedPreset = stored ? def.presets.find((p) => p.id === stored.preset) : undefined
  if (storedPreset) {
    const full = fillParams(storedPreset, stored?.params, ctx)
    const gen = generate(storedPreset, full, ctx)
    if (gen !== null && canon(gen) === target) return { preset: storedPreset.id, params: full }
  }
  const order = storedPreset ? [storedPreset, ...def.presets.filter((p) => p !== storedPreset)] : def.presets
  for (const preset of order) {
    const params = matchPreset(preset, target, ctx)
    if (params) return { preset: preset.id, params: fillParams(preset, params, ctx) }
  }
  return { preset: CUSTOM, params: {} }
}
