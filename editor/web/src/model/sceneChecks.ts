// Checks for the scene presentation commands (docs/EVENTS.md "Scene presentation"), mirroring what the game's loader refuses, so the
// maker hears about it before Play. One list of sentences per command, used by the map-event checks (storyChecks.ts) and by the
// campaign's scene checks (campaignChecks.ts). A scene left faded out is fine: it fades in by itself, so nothing says anything.
import { backgroundIds } from './backgrounds'
import { commandKind, STAGE_POSITIONS, type Command } from './storyTypes'
import type { ProjectDoc } from './types'

export const MAX_SCENE_MS = 10000
export const MAX_WAIT_MS = 60000
export const MAX_AUTO_MS = 60000
export const MAX_SHAKE_STRENGTH = 32

const whole = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v)
const ms = (what: string, v: unknown, hi: number, lo = 0): string | null =>
  v === undefined || (whole(v) && v >= lo && v <= hi) ? null : `${what} is milliseconds, a whole number from ${lo} to ${hi}.`

/** Everything wrong with the scene keys of this command (empty = fine). `scene`: a campaign scene (no map: no weather). */
export function sceneProblems(doc: ProjectDoc, c: Command, scene: boolean): string[] {
  const out: string[] = []
  const add = (t: string | null) => { if (t) out.push(t) }
  switch (commandKind(c)) {
    case 'background': {
      if (typeof c.background === 'string') {
        if (c.background === '') out.push('pick a background.')
        else if (!backgroundIds(doc).includes(c.background)) {
          out.push(`the background “${c.background}” is not one of this game’s (Art, Backgrounds)${backgroundIds(doc).length ? '' : ': there are none yet'}.`)
        }
      }
      if (c.transition !== undefined && c.transition !== 'cut' && c.transition !== 'fade') out.push('the transition is “cut” or “fade”.')
      add(ms('the transition time', c.ms, MAX_SCENE_MS))
      break
    }
    case 'fade':
      if (c.fade !== 'out' && c.fade !== 'in') out.push('a fade goes “out” or “in”.')
      add(ms('the fade time', c.ms, MAX_SCENE_MS))
      if (c.color !== undefined && !(typeof c.color === 'string' && /^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$/.test(c.color))) out.push('the colour is written like #000000.')
      break
    case 'wait':
      if (!whole(c.wait) || c.wait < 0 || c.wait > MAX_WAIT_MS) out.push(`the wait is milliseconds, a whole number from 0 to ${MAX_WAIT_MS}.`)
      break
    case 'shake':
      if (typeof c.shake !== 'object' || c.shake === null) out.push('a shake needs its length and strength.')
      else {
        add(ms('the shake time', c.shake.ms, MAX_SCENE_MS))
        const s = c.shake.strength
        if (s !== undefined && (!whole(s) || s < 1 || s > MAX_SHAKE_STRENGTH)) out.push(`the strength is pixels, a whole number from 1 to ${MAX_SHAKE_STRENGTH}.`)
      }
      break
    case 'leave':
      if (typeof c.leave !== 'string' || c.leave.trim() === '') out.push('name the speaker who leaves (the unit or name a line used).')
      if (c.transition !== undefined && c.transition !== 'cut' && c.transition !== 'fade') out.push('the transition is “cut” or “fade”.')
      add(ms('the fade time', c.ms, MAX_SCENE_MS))
      break
    case 'enter': case 'move': {
      const who = c.enter ?? c.move
      if (typeof who !== 'string' || who.trim() === '') out.push(`name the speaker who ${commandKind(c) === 'enter' ? 'comes on stage' : 'moves'} (the unit or name a line used).`)
      if (c.position === undefined) out.push('pick the place on the stage.')
      else if (!STAGE_POSITIONS.includes(c.position)) out.push('the place is farLeft, left, midLeft, center, midRight, right or farRight.')
      if (c.transition !== undefined && c.transition !== 'cut' && c.transition !== 'fade') out.push('the transition is “cut” or “fade”.')
      add(ms('the transition time', c.ms, MAX_SCENE_MS))
      break
    }
    case 'weather':
      if (scene) out.push('weather is for map events only: a scene has no map to put weather over.')
      else if (c.weather !== 'rain' && c.weather !== 'snow' && c.weather !== 'none') out.push('the weather is rain, snow or none.')
      break
    case 'narrate':
      if (!String(c.narrate ?? '').trim()) out.push('the text to say is empty.')
      add(ms('the “goes on by itself” time', c.auto, MAX_AUTO_MS, 1))
      break
    case 'say':
      if (c.position !== undefined && !STAGE_POSITIONS.includes(c.position)) out.push('the place is left, right or center (or farLeft, midLeft, midRight, farRight).')
      add(ms('the “goes on by itself” time', c.auto, MAX_AUTO_MS, 1))
      break
    case 'focus':
      add(ms('the scroll time', c.ms, MAX_SCENE_MS))
      if (c.speed !== undefined && (typeof c.speed !== 'number' || c.speed < 0.1 || c.speed > 100)) out.push('the scroll speed is squares per second, from 0.1 to 100.')
      if (c.speed !== undefined && c.ms !== undefined) out.push('give the scroll time or its speed, not both.')
      if (c.wait !== undefined && typeof c.wait !== 'boolean') out.push('“wait” is yes or no.')
      break
    case 'moveUnit':
      if (c.walk !== undefined && typeof c.walk !== 'boolean') out.push('“walk” is yes or no.')
      if (c.wait !== undefined && c.walk !== true) out.push('“wait” only means something while the unit walks.')
      break
    default: break
  }
  return out
}
