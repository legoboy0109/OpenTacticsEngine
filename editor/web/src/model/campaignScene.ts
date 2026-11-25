// A scene is the plain list of commands, or (docs/SOUND.md) the object `{ "music": id, "commands": [ ... ] }` which is the
// plain scene with `playMusic` in front. The editor writes the object only while the scene has music (or keys it does
// not know), and the plain list otherwise, so a game without scene music keeps its file as it was.
import type { Command } from './storyTypes'
import type { Extra } from './types'

export interface SceneObject extends Extra { music?: string; commands?: Command[]
  /** A background id (the scene starts on it), or null = the plain colour whatever the campaign's backdrop is (campaignBackdrop.ts). */
  backdrop?: string | null }
export type Scene = Command[] | SceneObject

export const isSceneObject = (s: Scene | undefined): s is SceneObject => !!s && !Array.isArray(s) && typeof s === 'object'

/** The commands of a scene, in whichever form it is written. */
export const sceneCommands = (s: Scene | undefined): Command[] => (Array.isArray(s) ? s : isSceneObject(s) && Array.isArray(s.commands) ? s.commands : [])

/** The music a scene starts, if it has the object form with `music`. */
export const sceneMusic = (s: Scene | undefined): string | undefined => (isSceneObject(s) && typeof s.music === 'string' ? s.music : undefined)

/** The scene with `list` as its commands. An emptied scene is written as no key (undefined); an object that still has music keeps it. */
export function withSceneCommands(s: Scene | undefined, list: Command[]): Scene | undefined {
  if (!isSceneObject(s)) return list.length ? list : undefined
  if (list.length) return { ...s, commands: list }
  const { commands: _gone, ...rest } = s
  return Object.keys(rest).length ? rest : undefined
}

/** The scene with `music` set (the object form) or, for undefined, taken off (back to the plain list when nothing else is left). */
export function withSceneMusic(s: Scene | undefined, music: string | undefined): Scene | undefined {
  if (music !== undefined) {
    if (isSceneObject(s)) return 'music' in s ? { ...s, music } : { music, ...s }
    return s && s.length ? { music, commands: s } : { music }
  }
  if (!isSceneObject(s)) return s
  const { music: _gone, ...rest } = s
  const keys = Object.keys(rest)
  if (keys.length === 0) return undefined
  if (keys.length === 1 && keys[0] === 'commands') return sceneCommands(rest as SceneObject).length ? (rest as SceneObject).commands : undefined
  return rest
}

/** A scene's say about its backdrop: not said (the campaign's), a picture, or "the plain colour" (null). */
export type SceneBackdrop = string | null | undefined

export const sceneBackdrop = (s: Scene | undefined): SceneBackdrop => {
  if (!isSceneObject(s) || !('backdrop' in s)) return undefined
  return typeof s.backdrop === 'string' ? s.backdrop : s.backdrop === null ? null : undefined
}

/** The scene with its `backdrop` set (the object form), or taken away (undefined) and back to the plain list when nothing else is left. */
export function withSceneBackdrop(s: Scene | undefined, backdrop: SceneBackdrop): Scene | undefined {
  if (backdrop !== undefined) {
    if (isSceneObject(s)) return { ...s, backdrop }
    return s && s.length ? { backdrop, commands: s } : { backdrop }
  }
  if (!isSceneObject(s)) return s
  const { backdrop: _gone, ...rest } = s as SceneObject
  const keys = Object.keys(rest)
  if (keys.length === 0) return undefined
  if (keys.length === 1 && keys[0] === 'commands') return Array.isArray(rest.commands) && rest.commands.length ? rest.commands : undefined
  return rest
}

