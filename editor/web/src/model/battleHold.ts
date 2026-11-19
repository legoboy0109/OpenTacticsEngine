// Holding a pose until the result has been shown (docs/BATTLE_SCENE.md "Holding a pose"): the animation keys holdFrame / startFrame / startDelayMs and the
// scene-wide durations (data/graphics.json "battle" > "scene"). Pure edits on the document, and the checks that mirror engine/core/data/loader_battle_art.cpp
// (checkHolds, readSceneTiming) in plain words. Defaults are never written.
import { DODGE_ANIMS, HIT_ANIMS, HOLD_ANIMS, HOLD_DELAY_MAX, battleOf, type AnimKey } from './battleArt'
import { withBattle } from './battleTables'
import type { BattleAnimDef, BattleSceneTimingDef, ProjectDoc } from './types'

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
  return out
}
