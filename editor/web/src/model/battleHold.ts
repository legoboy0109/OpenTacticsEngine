// Holding a pose until the result has been shown (docs/BATTLE_SCENE.md "Holding a pose"): the animation keys holdFrame / startFrame / startDelayMs and the
// scene-wide durations (data/graphics.json "battle" > "scene"). Pure edits on the document, and the checks that mirror engine/core/data/loader_battle_art.cpp
// (checkHolds, readSceneTiming) in plain words. Defaults are never written.
import { DODGE_ANIMS, HIT_ANIMS, HOLD_ANIMS, HOLD_DELAY_MAX, battleOf, type AnimKey } from './battleArt'
import { withBattle } from './battleTables'
import type { BattleAnimDef, BattleSceneTimingDef, HitHoldDef, ProjectDoc } from './types'

/** What the scene waits after the blow when the game does not say (milliseconds), and the extra wait before the same fighter strikes again. */
export const RESULT_HOLD_DEFAULT = 533
export const RESULT_HOLD_AGAIN_DEFAULT = 0
export const RESULT_HOLD_MAX = 5000

export interface SceneTiming { resultHoldMs: number; resultHoldAgainMs: number }
export const sceneTiming = (doc: ProjectDoc): SceneTiming => ({
  resultHoldMs: battleOf(doc).scene?.resultHoldMs ?? RESULT_HOLD_DEFAULT,
  resultHoldAgainMs: battleOf(doc).scene?.resultHoldAgainMs ?? RESULT_HOLD_AGAIN_DEFAULT,
})

/** Changes the scene durations; a value that is the default is written as no key, and an emptied "scene" section goes. */
export function setSceneTiming(doc: ProjectDoc, patch: Partial<SceneTiming>): ProjectDoc {
  return withBattle(doc, (b) => {
    const cur = b.scene ?? {}
    const next: BattleSceneTimingDef = { ...cur }
    if (patch.resultHoldMs !== undefined) {
      if (patch.resultHoldMs === RESULT_HOLD_DEFAULT) delete next.resultHoldMs
      else next.resultHoldMs = patch.resultHoldMs
    }
    if (patch.resultHoldAgainMs !== undefined) {
      if (patch.resultHoldAgainMs === RESULT_HOLD_AGAIN_DEFAULT) delete next.resultHoldAgainMs
      else next.resultHoldAgainMs = patch.resultHoldAgainMs
    }
    const { scene: _old, ...rest } = b
    return Object.keys(next).length ? { ...rest, scene: next } : rest
  })
}

/** "hitHold" (docs/BATTLE_SCENE.md "Holding a pose"): a hit waits by the hit points it changed. Keys, their upper limits (ms), as the loader. */
export const HIT_HOLD_KEYS = ['minMs', 'baseMs', 'perHpMs', 'killMs', 'noChangeMs'] as const
export type HitHoldKey = typeof HIT_HOLD_KEYS[number]
export const HIT_HOLD_MAX: Record<HitHoldKey, number> = { minMs: 5000, baseMs: 5000, perHpMs: 1000, killMs: 10000, noChangeMs: 5000 }
export const hitHoldOf = (doc: ProjectDoc): HitHoldDef | undefined => battleOf(doc).scene?.hitHold

/** Turns "hitHold" on (an empty object: every key at its default) or off (the key goes; an emptied "scene" section goes). */
export function setHitHoldOn(doc: ProjectDoc, on: boolean): ProjectDoc {
  return withBattle(doc, (b) => {
    const next: BattleSceneTimingDef = { ...(b.scene ?? {}) }
    if (on) next.hitHold = next.hitHold ?? {}
    else delete next.hitHold
    const { scene: _old, ...rest } = b
    return Object.keys(next).length ? { ...rest, scene: next } : rest
  })
}

/** Sets one key of "hitHold" (undefined = left out: its default). */
export function setHitHold(doc: ProjectDoc, key: HitHoldKey, v: number | undefined): ProjectDoc {
  return withBattle(doc, (b) => {
    const hh: HitHoldDef = { ...(b.scene?.hitHold ?? {}) }
    if (v === undefined) delete hh[key]
    else hh[key] = v
    return { ...b, scene: { ...(b.scene ?? {}), hitHold: hh } }
  })
}

/** Does any animation of the game hold a pose? (Only then do the scene durations matter.) */
export const usesHolds = (doc: ProjectDoc): boolean =>
  Object.values(battleOf(doc).sets ?? {}).some((s) => Object.values(s.animations ?? {}).some((a) => a.holdFrame !== undefined || a.startFrame !== undefined))

const whole = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)

/** What the game would refuse in the hold keys of one animation (the loader's checkHolds and the range checks of readAnim). `frames` = how many frames it has. */
export function holdProblems(setId: string, key: AnimKey, a: BattleAnimDef): string[] {
  const out: string[] = []
  const n = a.frames.length
  const bad = (t: string) => out.push(`Set “${setId}”: “${key}” ${t}`)
  // (frames are counted from 1 in the editor's words, from 0 in the file)
  const frame = (name: string, v: number | undefined): boolean => {
    if (v === undefined || (whole(v) && v >= 0 && v < n)) return true
    bad(`has ${name} that is not one of its frames (frames 1 to ${n}).`)
    return false
  }
  const holdOk = frame('a frame to wait on', a.holdFrame)
  const startOk = frame('a start frame', a.startFrame)
  if (a.startDelayMs !== undefined && (!whole(a.startDelayMs) || a.startDelayMs < 0 || a.startDelayMs > HOLD_DELAY_MAX)) bad(`has a delay that must be between 0 and ${HOLD_DELAY_MAX} milliseconds.`)
  const used = a.holdFrame !== undefined || a.startFrame !== undefined || (a.startDelayMs ?? 0) > 0
  if (!used) return out
  const dodge = DODGE_ANIMS.includes(key)
  if (!HOLD_ANIMS.includes(key)) { bad('cannot wait for a result: only the strikes, the dodges and the hurt animation can (a standing animation or a fall never waits).'); return out }
  if (a.startFrame !== undefined && !dodge) bad('cannot have a start frame: only the dodges have a brace before the blow.')
  if ((a.startDelayMs ?? 0) > 0 && !dodge) bad('cannot start later after the miss: only the dodges can.')
  if ((a.startDelayMs ?? 0) > 0 && a.holdFrame === undefined && a.startFrame === undefined) bad('has a start delay but neither a start frame nor a hold frame to apply it to.')
  if (holdOk && startOk && a.holdFrame !== undefined && a.startFrame !== undefined && a.holdFrame < a.startFrame) bad(`waits on frame ${a.holdFrame + 1}, which comes before its start frame ${a.startFrame + 1} (the wait is a frame of the dodge itself, not of the brace).`)
  const hit = a.hit ?? n - 1
  if (holdOk && HIT_ANIMS.includes(key) && a.holdFrame !== undefined && a.holdFrame < hit) bad(`waits on frame ${a.holdFrame + 1}, which comes before the hit frame ${hit + 1} (it waits after the blow: on the hit frame or a later one).`)
  return out
}

/** What the game would refuse in the scene durations. */
export function sceneTimingProblems(doc: ProjectDoc): string[] {
  const s = battleOf(doc).scene
  if (s === undefined) return []
  const out: string[] = []
  const check = (name: string, v: number | undefined) => {
    if (v !== undefined && (!whole(v) || v < 0 || v > RESULT_HOLD_MAX)) out.push(`${name} must be between 0 and ${RESULT_HOLD_MAX} milliseconds.`)
  }
  check('How long a held pose waits after the result', s.resultHoldMs)
  check('The extra wait before the same fighter strikes again', s.resultHoldAgainMs)
  const hh = s.hitHold
  if (hh !== undefined) {
    if (typeof hh !== 'object' || hh === null || Array.isArray(hh)) out.push('The wait by the damage must be a group of settings.')
    else for (const [k, v] of Object.entries(hh)) {
      if (!(HIT_HOLD_KEYS as readonly string[]).includes(k)) { out.push(`The wait by the damage has a setting “${k}” the game does not know.`); continue }
      const max = HIT_HOLD_MAX[k as HitHoldKey]
      if (typeof v !== 'number' || !Number.isFinite(v) || v < 0 || v > max) out.push(`The wait by the damage: “${k}” must be a number from 0 to ${max} milliseconds.`)
    }
  }
  return out
}
