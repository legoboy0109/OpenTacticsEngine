// The Play overlay: the embedded web player in an iframe, with a map picker, Restart and Close.
import { useCallback, useEffect, useRef, useState } from 'react'
import { viewKind } from '../model/styleView'
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

export const playerUrl = (project: string, map: string, style?: string): string =>
  `/player/embed.html?project=${encodeURIComponent(project)}&map=${encodeURIComponent(map)}${style ? `&style=${encodeURIComponent(style)}` : ''}`

export function PlayPanel({ store, maps, initialMap, onClose }: Props) {
  const dialog = useRef<HTMLDialogElement>(null)
  const frame = useRef<HTMLIFrameElement>(null)
  const [map, setMap] = useState(initialMap)
  const [status, setStatus] = useState<Status>({ state: 'loading' })
  const [saveFailed, setSaveFailed] = useState(false)
  // Styles the player can draw (it leaves out "isometric-3d" and "3d" for now). With one there is nothing to choose.
  const playable = (useDoc().game?.styles ?? []).filter((s) => viewKind(s) !== 'future')
  const [storedStyle, setStoredStyle] = useStored<string>(`ote.playStyle.${store.id}`, '')
  const style = playable.some((s) => s.id === storedStyle) ? storedStyle : playable[0]?.id ?? ''
  const switchStyle = async (id: string) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setStoredStyle(id)
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
          <Select value={map} onChange={(v) => void switchMap(v)} options={maps.map((m) => ({ value: m.id, label: m.name }))} aria-label={T.mapLabel} />
        </label>
        {playable.length > 1 && (
          <label className="play-map">
            <span>{T.styleLabel}</span>
            <Select value={style} onChange={(v) => void switchStyle(v)} options={playable.map((s) => ({ value: s.id, label: s.name || s.id }))} aria-label={T.styleLabel} />
          </label>
        )}
        <div className="play-actions">
          <button type="button" className="btn" onClick={() => void restart()} title={T.restartTitle}><Icon name="restart" />{T.restart}</button>
          <button type="button" className="btn" onClick={onClose}><Icon name="close" />{T.close}</button>
        </div>
      </header>
      <div className="play-stage">
        <iframe key={`${map}:${style}`} ref={frame} src={playerUrl(store.id, map, style !== playable[0]?.id ? style : undefined)} title="Game preview" className="play-frame"
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
