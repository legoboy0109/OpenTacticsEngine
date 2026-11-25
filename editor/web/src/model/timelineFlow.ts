// What a scene does to the story's way: which chapters its `goto`s name (and what leads to each: the words of an answer, a short
// description of an `if`), and whether the scene can end without any goto having run (then the normal order arrow stays).
// A goto does not stop a scene (docs/EVENTS.md "Campaign": the last one that ran wins), so "falls through" means "some way through
// the scene runs no goto".
import { clip, conditionsText, type SumCtx } from './storySummary'
import type { Command } from './storyTypes'

export interface SceneGoto { to: string; label: string }

const LABEL_MAX = 40
const lowerFirst = (s: string) => s.charAt(0).toLowerCase() + s.slice(1)

function collect(ctx: SumCtx, list: Command[] | undefined, trail: string[], out: SceneGoto[]): void {
  for (const c of list ?? []) {
    if (typeof c.goto === 'string' && c.goto) {
      const label = clip(trail.join(' › '), LABEL_MAX + 14)
      if (!out.some((g) => g.to === c.goto && g.label === label)) out.push({ to: c.goto, label })
    }
    collect(ctx, c.loop, [...trail, 'in a loop'], out)
    ;(c.answers ?? []).forEach((a) => collect(ctx, a.do, [...trail, `“${clip(String(a.text ?? ''), LABEL_MAX)}”`], out))
    if (c.then || c.else) {
      const cond = clip(`if ${lowerFirst(conditionsText(ctx, c.if))}`, LABEL_MAX)
      collect(ctx, c.then, [...trail, cond], out)
      collect(ctx, c.else, [...trail, 'otherwise'], out)
    }
  }
}

/** Every goto of the scene with its label, in the order written. */
export function sceneGotos(ctx: SumCtx, list: Command[] | undefined): SceneGoto[] {
  const out: SceneGoto[] = []
  collect(ctx, list, [], out)
  return out
}

/** True when every way through the list runs a goto: a bare one, a question whose answers all do, an `if` with an `else` whose
 *  two sides both do. Then the scene's own order arrow would never be taken. */
export function alwaysGoes(list: Command[] | undefined): boolean {
  for (const c of list ?? []) {
    if (typeof c.goto === 'string' && c.goto) return true
    if (Array.isArray(c.answers) && c.answers.length > 0 && c.answers.every((a) => alwaysGoes(a.do))) return true
    if (Array.isArray(c.else) && alwaysGoes(c.then) && alwaysGoes(c.else)) return true
  }
  return false
}
