// What the game loader refuses about the dialogue keys of a story command (docs/EVENTS.md "Speech bubbles, notices, names, faces and
// loops"; engine/core/data/loader_dialogue.cpp, loader_story_commands.cpp, loader_campaign_scene.cpp, loader_*_choice.cpp, name_tokens.cpp),
// in plain words. One function per command serves the map events' checks (storyChecks.ts) and the campaign's scenes (campaignChecks.ts).
import { isStoryId } from './storyIds'
import { commandKind, type Command } from './storyTypes'
import type { Extra, ProjectDoc } from './types'

export const MAX_FACE = 15
export const MAX_LOOP = 100
export const DEFAULT_LOOP = 10
export const MAX_NAME_LETTERS = 24
export const DEFAULT_NAME_LETTERS = 12
const MAX_AUTO_MS = 60000

const whole = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)

/** The most `expressions` any portrait of the game has (the loader's `maxExpressions`: a line's `face` may not go past it). */
export function maxExpressions(doc: ProjectDoc): number {
  const g = doc.graphics
  const lists: unknown[] = Object.values(g?.portraits ?? {}).map((p) => p?.expressions)
  for (const st of Object.values(g?.styles ?? {})) {
    const p = (st as Extra).portraits
    if (p && typeof p === 'object') for (const x of Object.values(p as Record<string, unknown>)) lists.push((x as Extra | null)?.expressions)
  }
  return Math.max(0, ...lists.map((l) => (Array.isArray(l) ? l.length : 0)))
}

/** The loader's isStoryId-checked `{name:<id>}` tokens: a "{name:" must be followed by an id and "}". Null when all is well. */
export function nameTokenProblem(text: unknown): string | null {
  if (typeof text !== 'string') return null
  for (let at = text.indexOf('{name:'); at !== -1; at = text.indexOf('{name:', at + 1)) {
    const end = text.indexOf('}', at + 6)
    if (end === -1 || !isStoryId(text.slice(at + 6, end))) return 'a “{name:” must be followed by a character’s story name and “}”, such as {name:hero}.'
  }
  return null
}

/** The commands that sit inside a `loop` (at any depth, through if / answers too): `break` is only allowed for these. */
export function commandsInLoops(list: Command[] | undefined): Set<Command> {
  const out = new Set<Command>()
  const walk = (l: Command[] | undefined, inLoop: boolean): void => {
    for (const c of Array.isArray(l) ? l : []) {
      if (inLoop) out.add(c)
      walk(c.then, inLoop); walk(c.else, inLoop)
      walk(c.loop, true)
      for (const a of c.answers ?? []) walk(a.do, inLoop)
    }
  }
  walk(list, false)
  return out
}

function onlyKeys(c: Command, allowed: string[], what: string, out: string[]): void {
  for (const k of Object.keys(c)) if (!allowed.includes(k)) out.push(`${what} cannot have “${k}” (it takes only ${allowed.map((a) => `“${a}”`).join(', ')}).`)
}

/** A valid typed name: 1 to 24 letters, no control characters, no { or } (the loader's validEnteredName). */
const validName = (s: string): boolean => {
  const letters = [...s]
  return letters.length >= 1 && letters.length <= MAX_NAME_LETTERS && !letters.some((ch) => ch < ' ' || ch === '\x7f' || ch === '{' || ch === '}')
}

/** `face` on a say or a choice. */
function faceProblems(doc: ProjectDoc, c: Command, out: string[]): void {
  if (c.face === undefined) return
  if (!whole(c.face) || c.face < 1 || c.face > MAX_FACE) { out.push(`the face is a whole number from 1 to ${MAX_FACE} (the portrait’s expression number).`); return }
  const most = maxExpressions(doc)
  if (c.face > most)
    out.push(`face ${c.face} needs a portrait with at least ${c.face} expressions (${most === 0 ? 'no portrait has any yet: add them in Art, Portraits' : `the most any portrait has is ${most}`}).`)
}

/**
 * Everything wrong with the dialogue keys of this command (empty = fine).
 * `scene`: a campaign scene (askName is only allowed there); `inLoop`: the command sits inside a loop (`break` needs that).
 */
export function dialogueProblems(doc: ProjectDoc, c: Command, scene: boolean, inLoop: boolean): string[] {
  const out: string[] = []
  const tokens = (text: unknown, what?: string) => { const p = nameTokenProblem(text); if (p) out.push(what ? `${what}: ${p}` : p) }
  switch (commandKind(c)) {
    case 'say':
      faceProblems(doc, c, out)
      tokens(c.say)
      break
    case 'narrate':
      tokens(c.narrate)
      break
    case 'choice': {
      tokens(c.choice, 'the question')
      ;(c.answers ?? []).forEach((a, i) => tokens(a.text, `answer ${i + 1}`))
      if (c.notice !== undefined && typeof c.notice !== 'boolean') out.push('“Show in the notice window” must be on or off.')
      if (c.notice === true) {
        for (const k of ['speaker', 'speakerName', 'portrait', 'face'] as const)
          if (c[k] !== undefined) out.push(`a question in the notice window has no speaker and no portrait, so “${k === 'speakerName' ? 'speaker name' : k}” makes no sense with it.`)
      } else faceProblems(doc, c, out)
      if (c.cursor !== undefined) {
        const most = Math.max(1, Array.isArray(c.answers) ? c.answers.length : 0)
        if (!whole(c.cursor) || c.cursor < 1 || c.cursor > most) out.push(`the starting answer is a whole number from 1 to the number of answers (${most}).`)
      }
      break
    }
    case 'notice':
      if (typeof c.notice !== 'string' || c.notice === '') out.push('the notice text is empty.')
      else tokens(c.notice)
      onlyKeys(c, ['notice', 'auto'], 'a notice', out)
      if (c.auto !== undefined && (!whole(c.auto) || c.auto < 1 || c.auto > MAX_AUTO_MS)) out.push(`the “goes on by itself” time is milliseconds, a whole number from 1 to ${MAX_AUTO_MS}.`)
      break
    case 'loop':
      onlyKeys(c, ['loop', 'max'], 'a loop', out)
      if (!Array.isArray(c.loop) || c.loop.length === 0) out.push('a loop needs at least one command inside it (it runs them again and again until a “Leave the loop”).')
      if (c.max !== undefined && (!whole(c.max) || c.max < 1 || c.max > MAX_LOOP)) out.push(`the most rounds is a whole number from 1 to ${MAX_LOOP}.`)
      break
    case 'break':
      onlyKeys(c, ['break'], 'a leave-the-loop command', out)
      if (c.break !== true) out.push('“Leave the loop” must be written as { "break": true }.')
      if (!inLoop) out.push('“Leave the loop” only works inside a loop.')
      break
    case 'askName': {
      if (!scene) { out.push('asking for a name is for campaign scenes only (a chapter’s Before or After scene): the name is kept by the campaign, not by one battle.'); break }
      onlyKeys(c, ['askName', 'default', 'max', 'prompt'], 'asking for a name', out)
      if (typeof c.askName !== 'string' || !isStoryId(c.askName)) out.push('pick the character whose name is asked (letters, digits, _ and - only).')
      const max = whole(c.max) && c.max >= 1 && c.max <= MAX_NAME_LETTERS ? c.max : DEFAULT_NAME_LETTERS
      if (c.max !== undefined && !(whole(c.max) && c.max >= 1 && c.max <= MAX_NAME_LETTERS)) out.push(`the most letters is a whole number from 1 to ${MAX_NAME_LETTERS}.`)
      if (c.default !== undefined) {
        if (typeof c.default !== 'string' || !validName(c.default)) out.push(`the starting name must be 1 to ${MAX_NAME_LETTERS} letters, with no { or }.`)
        else if ([...c.default].length > max) out.push(`the starting name has ${[...c.default].length} letters, more than the most allowed (${max}).`)
      }
      if (c.prompt !== undefined) {
        if (typeof c.prompt !== 'string' || c.prompt === '') out.push('the words above the name are empty.')
        else tokens(c.prompt, 'the words above the name')
      }
      break
    }
    default: break
  }
  return out
}

/** The checks for a portrait's `expressions` (loader_graphics.cpp): 1 to 15 whole cells; `fits` says whether a cell is inside the sheet. */
export function expressionsProblems(doc: ProjectDoc): string[] {
  const out: string[] = []
  for (const [id, p] of Object.entries(doc.graphics?.portraits ?? {})) {
    const e = (p as Extra | null)?.expressions as unknown
    if (e === undefined) continue
    if (!Array.isArray(e) || e.length < 1 || e.length > MAX_FACE) { out.push(`The portrait “${id}” needs 1 to ${MAX_FACE} expressions (pieces of its image), or none.`); continue }
    if (e.some((cell) => !Array.isArray(cell) || cell.length !== 2 || !whole(cell[0]) || !whole(cell[1]) || cell[0] < 0 || cell[1] < 0))
      out.push(`An expression of the portrait “${id}” is not a piece: write the column and the row, like 2,1.`)
  }
  return out
}
