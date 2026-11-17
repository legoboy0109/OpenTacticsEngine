// Reading the effects list of a staff (docs/SRPG.md section 8): which effect of each kind is where. A leaf file (it imports types only),
// so staff.ts and staffEffects.ts can both ask the same questions. The file's order of effects never matters to the form: every edit
// replaces an effect where it stands, appends a new one, or removes one, and an effect the form does not understand is never touched.
import type { Extra, Item } from './types'

export const MOVE_KINDS = ['warp', 'pull', 'swap'] as const
export type MoveKind = (typeof MOVE_KINDS)[number]
export const STAFF_EFFECT_KINDS = ['heal', 'warp', 'pull', 'swap', 'repair', 'light', 'inflict', 'cure', 'unlock']

/** The one key of an effect (`{ "heal": 5 }` is "heal"); undefined for anything that is not an object with exactly one key. */
export function fxKind(e: unknown): string | undefined {
  if (!e || typeof e !== 'object' || Array.isArray(e)) return undefined
  const k = Object.keys(e)
  return k.length === 1 ? k[0] : undefined
}

export const fxList = (item: Item | undefined): Extra[] => (Array.isArray(item?.effects) ? (item!.effects as Extra[]) : [])

/** Index of the first effect that is one of these kinds, or -1. */
export const fxIndex = (item: Item | undefined, kinds: readonly string[]): number => fxList(item).findIndex((e) => { const k = fxKind(e); return !!k && kinds.includes(k) })

export const hasFx = (item: Item | undefined, kinds: readonly string[]): boolean => fxIndex(item, kinds) >= 0

/** Indexes of the effects the form edits: the first heal, the first move, the first repair, the first light, the first inflict and cure. */
export function handledIndexes(item: Item | undefined): number[] {
  return [fxIndex(item, ['heal']), fxIndex(item, MOVE_KINDS), fxIndex(item, ['repair']), fxIndex(item, ['light']), fxIndex(item, ['inflict']), fxIndex(item, ['cure']), fxIndex(item, ['unlock'])].filter((i) => i >= 0)
}

/** How many effects the form leaves alone and keeps: kinds it does not know, a second heal, a second move, anything malformed. */
export const keptEffectCount = (item: Item | undefined): number => fxList(item).length - handledIndexes(item).length

/** A light staff: one of its effects is a light. */
export const isLightStaff = (item: Item | undefined): boolean => hasFx(item, ['light'])
/** An unlock staff (docs/SRPG.md section 10): one unlock effect; it picks a square of a door or chest. */
export const isUnlockStaff = (item: Item | undefined): boolean => hasFx(item, ['unlock'])
/** A staff that picks a square, not a unit (light or unlock): it is exactly one effect, with no target, area or "only if". */
export const isSquareStaff = (item: Item | undefined): boolean => isLightStaff(item) || isUnlockStaff(item)
