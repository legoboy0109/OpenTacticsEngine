// Listening to a sound in the editor: ONE shared <audio> element, so a second play button stops the first. Music that
// repeats starts again from its "Repeat from" time when it ends, like the game does (a simple `ended` loop is enough
// for a preview). The element sits in the page (hidden) so tests and assistive tools can find it.
import { emitSignal } from '../learn/signals'

export interface PreviewState {
  /** Which play button is on (a key the caller chose), or null. */
  key: string | null
  /** The browser could not play the file. */
  error: string | null
}

export interface PlayOptions {
  /** 0 to 100 (percent of full volume). */
  volume?: number
  loop?: boolean
  /** Seconds to jump back to when a repeating track ends. */
  loopStart?: number
}

class SoundPreview {
  private el: HTMLAudioElement | null = null
  private opts: PlayOptions = {}
  private state: PreviewState = { key: null, error: null }
  private listeners = new Set<() => void>()

  subscribe = (l: () => void): (() => void) => { this.listeners.add(l); return () => this.listeners.delete(l) }
  getState = (): PreviewState => this.state
  private set(p: Partial<PreviewState>) { this.state = { ...this.state, ...p }; this.listeners.forEach((l) => l()) }

  private element(): HTMLAudioElement {
    if (this.el) return this.el
    const el = document.createElement('audio')
    el.hidden = true
    el.preload = 'auto'
    el.setAttribute('data-testid', 'sound-preview')
    el.addEventListener('ended', () => {
      if (this.opts.loop && this.state.key !== null) {
        el.currentTime = Math.max(0, this.opts.loopStart ?? 0)
        void el.play().catch(() => this.stop())
      } else this.set({ key: null })
    })
    el.addEventListener('error', () => this.set({ key: null, error: 'This file could not be played here.' }))
    document.body.appendChild(el)
    this.el = el
    return el
  }

  async play(key: string, url: string, opts: PlayOptions = {}): Promise<void> {
    emitSignal('soundPreviewed')
    const el = this.element()
    this.opts = opts
    el.pause()
    this.set({ key, error: null })
    el.src = url
    el.volume = Math.min(1, Math.max(0, (opts.volume ?? 100) / 100))
    el.currentTime = 0
    try { await el.play() } catch (e) {
      // A newer play() replaced this one: nothing to say. Otherwise the browser refused (or the file is bad).
      if (this.state.key === key) this.set({ key: null, error: e instanceof Error && e.name === 'NotAllowedError' ? 'The browser did not allow sound yet; click the button again.' : 'This file could not be played here.' })
    }
  }

  stop(): void {
    this.el?.pause()
    this.set({ key: null })
  }

  /** Where the playing sound is, in seconds (0 when nothing plays). */
  position(): number { return this.state.key !== null && this.el ? this.el.currentTime : 0 }
}

export const soundPreview = new SoundPreview()
