// game.json "dialogue": how story lines are shown (docs/EVENTS.md "Speech bubbles"). Presentation only. The defaults (box; bubble
// portrait beside; names shown) are written as NO key, and a game that never touches the setting has no "dialogue" at all. The loader
// refuses "portrait" and "names" unless the style is "bubble", so choosing the box takes them away. Unknown keys stay.
import { withGame } from './doc'
import { patchObj } from './util'
import type { DialogueLook, ProjectDoc } from './types'

export type DialogueStyle = 'box' | 'bubble'
export type BubblePortrait = 'beside' | 'inside' | 'none'
export const DIALOGUE_STYLES: DialogueStyle[] = ['box', 'bubble']
export const BUBBLE_PORTRAITS: BubblePortrait[] = ['beside', 'inside', 'none']
const KNOWN = ['style', 'portrait', 'names']

const look = (doc: ProjectDoc): DialogueLook | undefined => {
  const d = doc.game?.dialogue as unknown
  return d !== null && typeof d === 'object' && !Array.isArray(d) ? (d as DialogueLook) : undefined
}

/** A word the loader would refuse reads as the default here and is reported by dialogueProblems. */
export const dialogueStyle = (doc: ProjectDoc): DialogueStyle => (look(doc)?.style === 'bubble' ? 'bubble' : 'box')
export const bubblePortrait = (doc: ProjectDoc): BubblePortrait => {
  const p = look(doc)?.portrait
  return p === 'inside' || p === 'none' ? p : 'beside'
}
export const bubbleNames = (doc: ProjectDoc): boolean => look(doc)?.names !== false

/** Writes the `dialogue` object, or no key when nothing but defaults is left (unknown keys keep it alive). */
function write(doc: ProjectDoc, patch: Record<string, unknown>): ProjectDoc {
  const next = patchObj(look(doc) ?? {}, patch)
  return withGame(doc, (g) => patchObj(g, { dialogue: Object.keys(next).length ? next : undefined }))
}

export function setDialogueStyle(doc: ProjectDoc, style: DialogueStyle): ProjectDoc {
  if (!doc.game || dialogueStyle(doc) === style) return doc
  return style === 'bubble' ? write(doc, { style: 'bubble' }) : write(doc, { style: undefined, portrait: undefined, names: undefined })
}

export function setBubblePortrait(doc: ProjectDoc, p: BubblePortrait): ProjectDoc {
  if (!doc.game || dialogueStyle(doc) !== 'bubble' || bubblePortrait(doc) === p) return doc
  return write(doc, { portrait: p === 'beside' ? undefined : p })
}

export function setBubbleNames(doc: ProjectDoc, on: boolean): ProjectDoc {
  if (!doc.game || dialogueStyle(doc) !== 'bubble' || bubbleNames(doc) === on) return doc
  return write(doc, { names: on ? undefined : false })
}

/** What the loader refuses about "dialogue" (engine/core/data/loader_dialogue.cpp readDialogueLook), in plain words. */
export function dialogueProblems(doc: ProjectDoc): string[] {
  const raw = doc.game?.dialogue as unknown
  if (raw === undefined) return []
  if (raw === null || typeof raw !== 'object' || Array.isArray(raw)) return ['The dialogue setting must be an object like { "style": "bubble" }.']
  const d = raw as DialogueLook
  const out: string[] = []
  for (const k of Object.keys(d)) if (!KNOWN.includes(k)) out.push(`The dialogue setting has “${k}”, which the game does not know (it knows style, portrait and names).`)
  if (d.style !== undefined && !DIALOGUE_STYLES.includes(d.style)) out.push('The dialogue style must be “box” (the box at the bottom of the screen) or “bubble” (a speech bubble pointing at the speaker).')
  if (d.portrait !== undefined && !BUBBLE_PORTRAITS.includes(d.portrait)) out.push('The bubble’s portrait must be “beside”, “inside” or “none”.')
  if (d.names !== undefined && typeof d.names !== 'boolean') out.push('“Show the speaker’s name” must be on or off.')
  if (d.style !== 'bubble' && (d.portrait !== undefined || d.names !== undefined)) out.push('The portrait place and the speaker’s name are for speech bubbles: they need the style “bubble”.')
  return out
}
