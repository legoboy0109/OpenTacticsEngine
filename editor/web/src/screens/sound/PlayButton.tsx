// A play / stop button for one sound or track. Only one thing plays at a time (audio/preview.ts); pressing the button of
// what is playing stops it.
import { soundPreview, type PlayOptions } from '../../audio/preview'
import { useSoundPreview } from '../../audio/useSoundPreview'
import { Icon } from '../../ui/Icon'
import { T } from './text'

export interface PlaySource { url: string; opts?: PlayOptions }

interface Props {
  /** Which button this is (the preview remembers which one is on). */
  playKey: string
  /** Names the button for assistive technology and tests: "Play battle". */
  what: string
  /** Is there something to play (the button is disabled otherwise, with `why` as its tooltip)? */
  playable: boolean
  /** What to play when pressed (a sound with variants picks one at random here). */
  source: () => PlaySource | null
  why?: string
}

export function PlayButton({ playKey, what, playable, source, why }: Props) {
  const { key } = useSoundPreview()
  const on = key === playKey
  const toggle = () => {
    if (on) { soundPreview.stop(); return }
    const s = source()
    if (s) void soundPreview.play(playKey, s.url, s.opts)
  }
  const blocked = !on && !playable
  return (
    <button type="button" className={`btn sm icon play-button ${on ? 'primary' : ''}`} aria-label={on ? T.stopLabel(what) : T.playLabel(what)} aria-pressed={on}
      title={blocked ? why ?? T.noFileToPlay : on ? T.stop : T.play} disabled={blocked} data-testid={`play-${playKey}`} data-learn="sound-play" onClick={toggle}>
      <Icon name={on ? 'stop' : 'play'} />
    </button>
  )
}
