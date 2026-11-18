// Range bands of the battle scene's fixed stage (docs/BATTLE_SCENE.md "Range bands"): data/graphics.json "battle" > "stage" > "ranges". Pure edits
// on the document, and the checks mirror engine/core/data/loader_battle_stage.cpp (readRanges). A band is the layout of fights from `from` squares
// apart; `view: "each"` shows one fighter at a time and moves a camera (`pan`) over a world wider than the stage.
import { withBattle } from './battleTables'
import { stageOf } from './battleStage'
import { patchObj } from './util'
import type { BattleStageDef, ProjectDoc, StageBandDef, StagePoint } from './types'

export type BandSide = 'left' | 'right'
export type BandGroup = 'feet' | 'platform'
export const BAND_FROM_MIN = 2
export const BAND_FROM_MAX = 99
export const BAND_MAX = 16
export const WORLD_MAX = 8191
export const DEFAULT_PAN_MS = 250

export const bandsOf = (doc: ProjectDoc): StageBandDef[] => stageOf(doc)?.ranges ?? []
export const bandEach = (b: StageBandDef): boolean => b.view === 'each'
export const bandPanMs = (b: StageBandDef): number => b.pan?.panMs ?? DEFAULT_PAN_MS

const withBands = (doc: ProjectDoc, fn: (list: StageBandDef[], s: BattleStageDef) => StageBandDef[]): ProjectDoc => {
  const s = stageOf(doc)
  if (!s) return doc
  const next = fn(s.ranges ?? [], s)
  return withBattle(doc, (b) => ({ ...b, stage: patchObj(s, { ranges: next.length ? next : undefined }) }))
}
const withBand = (doc: ProjectDoc, i: number, fn: (b: StageBandDef, s: BattleStageDef) => StageBandDef): ProjectDoc =>
  withBands(doc, (list, s) => (i < 0 || i >= list.length ? list : list.map((b, k) => (k === i ? fn(b, s) : b))))

const copyPoint = (p: StagePoint): StagePoint => [p[0], p[1]]

/** A new band from the first free distance (4, else the next free one): a copy of the layout at range, both fighters in the picture. */
export function addBand(doc: ProjectDoc): ProjectDoc {
  return withBands(doc, (list, s) => {
    if (list.length >= BAND_MAX) return list
    let from = 4
    while (list.some((b) => b.from === from) && from < BAND_FROM_MAX) from += 1
    const src = s.feetFar ?? s.feet
    const band: StageBandDef = { from, feet: { left: copyPoint(src.left), right: copyPoint(src.right) } }
    return [...list, band].sort((a, b) => a.from - b.from)
  })
}
export const removeBand = (doc: ProjectDoc, i: number): ProjectDoc => withBands(doc, (list) => list.filter((_, k) => k !== i))
/** Changes `from`; the list stays in order of `from` (the game sorts it too). */
export const setBandFrom = (doc: ProjectDoc, i: number, v: number): ProjectDoc =>
  withBands(doc, (list) => list.map((b, k) => (k === i ? { ...b, from: v } : b)))
export function setBandPoint(doc: ProjectDoc, i: number, group: BandGroup, side: BandSide, axis: 0 | 1, v: number): ProjectDoc {
  return withBand(doc, i, (b) => {
    const cur = b[group]
    if (!cur) return b
    const pt = copyPoint(cur[side])
    pt[axis] = v
    return { ...b, [group]: { ...cur, [side]: pt } }
  })
}
/** The band's own platform places (a copy of a place under each foot point), or none (each platform is then centred on its foot point). */
export function setBandPlatform(doc: ProjectDoc, i: number, on: boolean): ProjectDoc {
  return withBand(doc, i, (b) => {
    if (!on) return patchObj(b, { platform: undefined })
    if (b.platform) return b
    const at = (p: StagePoint): StagePoint => [Math.max(0, p[0] - 32), Math.max(0, p[1] - 8)]
    return { ...b, platform: { left: at(b.feet.left), right: at(b.feet.right) } }
  })
}
/** One fighter at a time ("each") needs a pan; the points then live in a world twice as wide as the stage (the right fighter and platform move over by one stage width, the right view
 *  starts there). Back to both in one picture: the right fighter's points move back and the pan goes. */
export function setBandView(doc: ProjectDoc, i: number, view: 'both' | 'each'): ProjectDoc {
  return withBand(doc, i, (b, s) => {
    if ((view === 'each') === bandEach(b)) return b
    const shift = (p: StagePoint, by: number): StagePoint => [Math.max(0, Math.min(WORLD_MAX, p[0] + by)), p[1]]
    if (view === 'each') {
      const feet = { left: b.feet.left, right: shift(b.feet.right, s.width) }
      const platform = b.platform ? { left: b.platform.left, right: shift(b.platform.right, s.width) } : undefined
      return patchObj({ ...b, view: 'each', feet, pan: { panMs: DEFAULT_PAN_MS, leftView: 0, rightView: s.width } }, { platform })
    }
    const back = b.pan?.rightView ?? s.width
    const feet = { left: b.feet.left, right: shift(b.feet.right, -back) }
    const platform = b.platform ? { left: b.platform.left, right: shift(b.platform.right, -back) } : undefined
    return patchObj({ ...b, view: undefined, feet, pan: undefined }, { platform })
  })
}
export function setBandPan(doc: ProjectDoc, i: number, key: 'panMs' | 'leftView' | 'rightView', v: number): ProjectDoc {
  return withBand(doc, i, (b) => (b.pan ? { ...b, pan: { ...b.pan, [key]: v } } : b))
}

/** pan "backdrop": fixed is the default and is written as no key. */
export const bandBackdrop = (b: StageBandDef): 'fixed' | 'scroll' => (b.pan?.backdrop === 'scroll' ? 'scroll' : 'fixed')
export const setBandBackdrop = (doc: ProjectDoc, i: number, v: 'fixed' | 'scroll'): ProjectDoc =>
  withBand(doc, i, (b) => (b.pan ? { ...b, pan: patchObj(b.pan, { backdrop: v === 'scroll' ? 'scroll' : undefined }) as NonNullable<StageBandDef['pan']> } : b))
/** pan "startMs": when the camera leaves for the struck fighter, after the strike starts; `undefined` = timed to arrive at the hit (no key). */
export const PAN_START_MAX = 15000
export const setBandPanStart = (doc: ProjectDoc, i: number, v: number | undefined): ProjectDoc =>
  withBand(doc, i, (b) => (b.pan ? { ...b, pan: patchObj(b.pan, { startMs: v }) as NonNullable<StageBandDef['pan']> } : b))

const whole = (n: unknown): n is number => typeof n === 'number' && Number.isInteger(n)
const pairProblems = (b: StageBandDef, group: BandGroup, label: string, maxX: number, height: number, bad: (t: string) => void) => {
  const pair = b[group]
  if (!pair) return
  for (const side of ['left', 'right'] as const) {
    const p = pair[side]
    if (!Array.isArray(p) || p.length !== 2 || !whole(p[0]) || !whole(p[1])) { bad(`the ${side} one of the ${label} is not a pair of whole numbers.`); continue }
    if (p[0] < 0 || p[1] < 0 || p[0] > maxX || p[1] >= height) bad(`the ${side} one of the ${label} (${p[0]}, ${p[1]}) is outside the ${b.view === 'each' ? 'world' : 'stage'} (${maxX + 1} × ${height}).`)
  }
}

/** What the loader would refuse in the stage's range bands, in plain words. */
export function bandProblems(doc: ProjectDoc): string[] {
  const s = stageOf(doc)
  const list = s?.ranges
  if (!s || list === undefined) return []
  const out: string[] = []
  if (!Array.isArray(list) || list.length < 1 || list.length > BAND_MAX) return [`The stage's range layouts must be a list of 1 to ${BAND_MAX} (remove the list to have none).`]
  list.forEach((b, i) => {
    const bad = (t: string) => out.push(`Range layout ${i + 1}: ${t}`)
    if (!whole(b.from) || b.from < BAND_FROM_MIN || b.from > BAND_FROM_MAX) bad(`“from” must be a distance between ${BAND_FROM_MIN} and ${BAND_FROM_MAX} squares (1 is the close layout).`)
    else if (list.some((o, j) => j < i && o.from === b.from)) bad(`another layout also starts at ${b.from}.`)
    if (b.view !== undefined && b.view !== 'both' && b.view !== 'each') bad('it must show both fighters or one at a time.')
    const each = b.view === 'each'
    if (b.pan !== undefined && !each) bad('a pan belongs to “one fighter at a time”.')
    if (each && b.pan === undefined) bad('“one fighter at a time” needs the pan settings.')
    if (!b.feet) { bad('it needs the foot points.'); return }
    const whole2 = whole(s.width) && whole(s.height)
    if (whole2) {
      pairProblems(b, 'feet', 'foot points', each ? WORLD_MAX : s.width - 1, s.height, bad)
      pairProblems(b, 'platform', 'platform places', each ? WORLD_MAX : s.width - 1, s.height, bad)
    }
    if (each && b.pan) {
      const p = b.pan
      if (p.panMs !== undefined && (!whole(p.panMs) || p.panMs < 0 || p.panMs > 5000)) bad('the pan time must be between 0 and 5000 milliseconds (0 = the camera cuts).')
      if (p.backdrop !== undefined && p.backdrop !== 'fixed' && p.backdrop !== 'scroll') bad('the backdrop must stay fixed or scroll with the camera.')
      if (p.startMs !== undefined && (!whole(p.startMs) || p.startMs < 0 || p.startMs > PAN_START_MAX)) bad(`the camera's start time must be between 0 and ${PAN_START_MAX} milliseconds.`)
      for (const k of ['leftView', 'rightView'] as const)
        if (!whole(p[k]) || p[k] < 0 || p[k] > WORLD_MAX) bad(`the ${k === 'leftView' ? 'left' : 'right'} view must be a whole number between 0 and ${WORLD_MAX}.`)
      if (whole(s.width)) {
        const views = { left: p.leftView, right: p.rightView }
        for (const side of ['left', 'right'] as const) {
          const x = b.feet[side]?.[0]
          if (whole(x) && whole(views[side]) && (x < views[side] || x >= views[side] + s.width))
            bad(`the ${side} fighter's foot point (${x}) is not inside the picture while the camera is on it (the ${side} view ${views[side]} shows ${views[side]} to ${views[side] + s.width - 1}).`)
        }
      }
    }
  })
  return out
}
