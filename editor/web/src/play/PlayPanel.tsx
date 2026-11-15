// The Play overlay: the embedded web player in an iframe, with a map picker, Restart and Close.
import { useCallback, useEffect, useRef, useState } from 'react'
import { campaignMapHasFog, mapHasFog } from '../model/fog'
import { useDoc } from '../store/context'
import type { DocumentStore } from '../store/documentStore'
import { useStored } from '../ui/hooks'
import { Icon } from '../ui/Icon'
import { Select } from '../ui/Select'
import { T } from './text'

interface Props {
  store: DocumentStore
  maps: Array<{ id: string; name: string }>
  initialMap: string
  onClose: () => void
}

type Status = { state: 'loading' } | { state: 'ready' } | { state: 'error'; message: string }

/** Picker value for "the campaign from chapter 1" (no map id can be this: ids have no '*'). */
export const PLAY_CAMPAIGN = '*campaign'

/** `showAll` adds `showall=1`: the player draws without fog of war (the maker's view; docs/ECONOMY.md "Fog of war").
 *  `muted` adds `noaudio=1`: the player starts without a sound device. */
export const playerUrl = (project: string, map: string, style?: string, showAll?: boolean, muted?: boolean): string =>
  `/player/embed.html?project=${encodeURIComponent(project)}&${map === PLAY_CAMPAIGN ? 'campaign=1' : `map=${encodeURIComponent(map)}`}${style ? `&style=${encodeURIComponent(style)}` : ''}${showAll ? '&showall=1' : ''}${muted ? '&noaudio=1' : ''}`

export function PlayPanel({ store, maps, initialMap, onClose }: Props) {
  const dialog = useRef<HTMLDialogElement>(null)
  const frame = useRef<HTMLIFrameElement>(null)
  const [map, setMap] = useState(initialMap)
  const [status, setStatus] = useState<Status>({ state: 'loading' })
  const [saveFailed, setSaveFailed] = useState(false)
  // Every style can be played. With one there is nothing to choose.
  const doc = useDoc()
  const playable = doc.game?.styles ?? []
  // A game with a campaign can also be played as its story, from the first chapter.
  const choices = [...(doc.campaign ? [{ value: PLAY_CAMPAIGN, label: T.campaignChoice }] : []), ...maps.map((m) => ({ value: m.id, label: m.name }))]
  const [storedStyle, setStoredStyle] = useStored<string>(`ote.playStyle.${store.id}`, '')
  const style = playable.some((s) => s.id === storedStyle) ? storedStyle : playable[0]?.id ?? ''
  const switchStyle = async (id: string) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setStoredStyle(id)
  }

  // "Show everything" only means something when the played map has fog (for the story: when any chapter's map has).
  const fogHere = map === PLAY_CAMPAIGN ? campaignMapHasFog(doc) : mapHasFog(doc, map)
  const [storedAll, setStoredAll] = useStored<boolean>(`ote.playShowAll.${store.id}`, false)
  const showAll = fogHere && storedAll === true
  const switchShowAll = async (on: boolean) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setStoredAll(on)
  }

  // Sound: on unless the maker turned it off (remembered like the panel's other choices).
  const [storedSound, setStoredSound] = useStored<boolean>(`ote.playSound.${store.id}`, true)
  const sound = storedSound !== false
  const switchSound = async (on: boolean) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setStoredSound(on)
  }

  useEffect(() => {
    const el = dialog.current!
    if (!el.open) el.showModal()
    return () => { el.close() }
  }, [])

  // The embedded page reports through postMessage; ignore anything that is not from our own frame.
  useEffect(() => {
    const onMessage = (e: MessageEvent) => {
      if (e.source !== frame.current?.contentWindow || !e.data) return
      if (e.data.type === 'ote-player-ready') { setStatus({ state: 'ready' }); frame.current?.focus() }
      else if (e.data.type === 'ote-player-error') setStatus({ state: 'error', message: String(e.data.message ?? 'Unknown error') })
    }
    window.addEventListener('message', onMessage)
    return () => window.removeEventListener('message', onMessage)
  }, [])

  const saveFirst = useCallback(async () => {
    const ok = await store.flush()
    setSaveFailed(!ok)
    return ok
  }, [store])

  const restart = async () => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    frame.current?.contentWindow?.postMessage({ type: 'ote-player-restart' }, location.origin)
  }
  const switchMap = async (id: string) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setMap(id)
  }

  return (
    <dialog ref={dialog} className="play-dialog" aria-label={T.panelTitle} onCancel={(e) => { e.preventDefault(); onClose() }}>
      <header className="play-head">
        <h2>{T.panelTitle}</h2>
        <label className="play-map">
          <span>{T.mapLabel}</span>
          <Select value={map} onChange={(v) => void switchMap(v)} options={choices} aria-label={T.mapLabel} />
        </label>
        {playable.length > 1 && (
          <label className="play-map">
            <span>{T.styleLabel}</span>
            <Select value={style} onChange={(v) => void switchStyle(v)} options={playable.map((s) => ({ value: s.id, label: s.name || s.id }))} aria-label={T.styleLabel} />
          </label>
        )}
        {fogHere && (
          <label className="play-all" title={T.showAllTitle}>
            <input type="checkbox" checked={showAll} onChange={(e) => void switchShowAll(e.target.checked)} />
            <span>{T.showAll}</span>
          </label>
        )}
        <label className="play-all" title={T.soundTitle}>
          <input type="checkbox" checked={sound} onChange={(e) => void switchSound(e.target.checked)} />
          <span>{T.sound}</span>
        </label>
        <div className="play-actions">
          <button type="button" className="btn" onClick={() => void restart()} title={T.restartTitle}><Icon name="restart" />{T.restart}</button>
          <button type="button" className="btn" onClick={onClose}><Icon name="close" />{T.close}</button>
        </div>
      </header>
      <div className="play-stage">
        <iframe key={`${map}:${style}:${showAll}:${sound}`} ref={frame} src={playerUrl(store.id, map, style !== playable[0]?.id ? style : undefined, showAll, !sound)} title="Game preview" className="play-frame"
          allow="fullscreen" onLoad={() => frame.current?.focus()} />
        {status.state === 'loading' && <div className="play-overlay" role="status" data-testid="play-loading"><span className="spinner" aria-hidden="true" />{T.loading}</div>}
        {status.state === 'error' && (
          <div className="play-overlay error" role="alert" data-testid="play-error">
            <h3>{T.errorTitle}</h3>
            <pre className="play-error-text">{status.message}</pre>
            <button type="button" className="btn primary" onClick={onClose}>{T.back}</button>
          </div>
        )}
        {saveFailed && <div className="play-banner" role="alert">{T.unsavedTitle}</div>}
      </div>
      <footer className="play-foot">
        <p className="play-hint"><strong>{T.controlsTitle}:</strong> {T.controls.map(([what, keys]) => `${what}: ${keys}`).join(' · ')}</p>
      </footer>
    </dialog>
  )
}
