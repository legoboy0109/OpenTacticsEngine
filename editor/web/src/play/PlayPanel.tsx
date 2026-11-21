// The Play overlay: the embedded web player in an iframe, with a map picker, Restart and Close.
import { useCallback, useEffect, useRef, useState } from 'react'
import { difficultyList } from '../model/difficulty'
import { campaignMapHasFog, mapHasFog } from '../model/fog'
import { emitSignal } from '../learn/signals'
import { useDoc } from '../store/context'
import type { DocumentStore } from '../store/documentStore'
import { useStored } from '../ui/hooks'
import { Icon } from '../ui/Icon'
import { Select } from '../ui/Select'
import {
  chapterChoice, DEFAULT_TEST_NUMBER, effectiveMode, parseLevelNumber, playerUrl, PLAY_CAMPAIGN, readTestLevels, TEST_LEVEL_MAX, TEST_LEVEL_MIN, TEST_OFF,
  type PlayMode, type TestLevel,
} from './playOptions'
import { T } from './text'
import { useWholeFit, viewSize } from './wholeFit'

export { PLAY_CAMPAIGN, playerUrl }

interface Props {
  store: DocumentStore
  maps: Array<{ id: string; name: string }>
  initialMap: string
  onClose: () => void
}

type Status = { state: 'loading' } | { state: 'ready' } | { state: 'error'; message: string }

export function PlayPanel({ store, maps, initialMap, onClose }: Props) {
  const dialog = useRef<HTMLDialogElement>(null)
  const frame = useRef<HTMLIFrameElement>(null)
  const stage = useRef<HTMLDivElement>(null)
  const head = useRef<HTMLElement>(null)
  const foot = useRef<HTMLElement>(null)
  const [map, setMap] = useState(initialMap)
  const [status, setStatus] = useState<Status>({ state: 'loading' })
  const [saveFailed, setSaveFailed] = useState(false)
  // The test level the player reports for "Automatic" (it works the number out from the map, so the panel reads it back).
  const [autoLevel, setAutoLevel] = useState<number | null>(null)
  // Every style can be played. With one there is nothing to choose.
  const doc = useDoc()
  const playable = doc.game?.styles ?? []
  // The dialog is sized from the window: the frame is the largest whole multiple of the game's view that fits it, in device pixels (wholeFit.ts).
  const layout = useWholeFit({ dialog, head, stage, foot }, viewSize(doc.game?.view))
  const fitCss = layout.frame
  // The player sizes its canvas (css size x devicePixelRatio, rounded) on the frame's `resize` events, and in a frame whose size is a
  // fraction of a css pixel the backing store was left one pixel short (1919 for 1920 at 1.75): ask it to measure again once the new
  // size is laid out, and when it has started.
  const [readyCount, setReadyCount] = useState(0)
  useEffect(() => {
    const poke = () => frame.current?.contentWindow?.dispatchEvent(new Event('resize'))
    const timers = [0, 120, 400, 1000].map((ms) => window.setTimeout(() => requestAnimationFrame(poke), ms))
    return () => timers.forEach((t) => window.clearTimeout(t))
  }, [fitCss, readyCount])
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

  // Difficulty (docs/DIFFICULTY.md): only for a game that has some. "Ask me" shows the player's choice screen.
  const difficulties = difficultyList(doc)
  const [storedDifficulty, setStoredDifficulty] = useStored<string>(`ote.playDifficulty.${store.id}`, '')
  const difficulty = difficulties.some((d) => d.id === storedDifficulty) ? storedDifficulty : ''
  const switchDifficulty = async (id: string) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setStoredDifficulty(id)
  }

  // "This map alone" / "From this chapter of the campaign" (docs/SAVES.md "Test playthroughs"), remembered per project. The second is
  // only possible on a map that is a chapter's; otherwise the first is used and the choice says why it is greyed.
  const [storedMode, setStoredMode] = useStored<PlayMode>(`ote.playMode.${store.id}`, 'map')
  const chapterOpt = chapterChoice(doc, map)
  const mode = effectiveMode(storedMode, chapterOpt)
  const chapter = mode === 'chapter' && chapterOpt.ok ? chapterOpt.chapter : undefined
  const switchMode = async (v: string) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setStoredMode(v === 'chapter' ? 'chapter' : 'map')
  }

  // The test level (docs/DIFFICULTY.md "Test level"): off / automatic / a number, remembered per map of the project.
  const [storedLevels, setStoredLevels] = useStored<Record<string, TestLevel>>(`ote.playTestLevels.${store.id}`, {})
  const testLevel = readTestLevels(storedLevels)[map] ?? TEST_OFF
  const [draft, setDraft] = useState<string | null>(null)  // the number box while it is being typed
  const setLevel = async (t: TestLevel) => {
    if (!(await saveFirst())) return
    setStatus({ state: 'loading' })
    setAutoLevel(null)
    const next = { ...readTestLevels(storedLevels) }
    if (t.kind === 'off') delete next[map]; else next[map] = t
    setStoredLevels(next)
  }
  const switchLevelKind = (v: string) => {
    setDraft(null)
    void setLevel(v === 'auto' ? { kind: 'auto' } : v === 'number' ? { kind: 'number', n: testLevel.kind === 'number' ? testLevel.n : DEFAULT_TEST_NUMBER } : TEST_OFF)
  }
  const commitNumber = () => {
    if (draft === null) return
    const n = parseLevelNumber(draft)
    setDraft(null)
    if (n !== null && !(testLevel.kind === 'number' && testLevel.n === n)) void setLevel({ kind: 'number', n })
  }
  const numberBad = draft !== null && parseLevelNumber(draft) === null

  useEffect(() => {
    const el = dialog.current!
    if (!el.open) el.showModal()
    return () => { el.close() }
  }, [])

  // The embedded page reports through postMessage; ignore anything that is not from our own frame.
  useEffect(() => {
    const onMessage = (e: MessageEvent) => {
      if (e.source !== frame.current?.contentWindow || !e.data) return
      if (e.data.type === 'ote-player-ready') { setStatus({ state: 'ready' }); setReadyCount((n) => n + 1); frame.current?.focus(); emitSignal('played') }
      else if (e.data.type === 'ote-player-testlevel') setAutoLevel(typeof e.data.level === 'number' && e.data.level > 0 ? e.data.level : null)
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
    <dialog ref={dialog} className="play-dialog" style={layout.dialog} aria-label={T.panelTitle} onCancel={(e) => { e.preventDefault(); onClose() }}>
      <header className="play-head" ref={head}>
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
        <label className="play-map" title={T.modeTitle}>
          <span>{T.modeLabel}</span>
          <Select value={mode} onChange={(v) => void switchMode(v)} aria-label={T.modeLabel}
            options={[{ value: 'map', label: T.modeMap },
              { value: 'chapter', label: chapterOpt.ok ? T.modeChapter(chapterOpt.name) : `${T.modeChapterOff} (${T.modeReason[chapterOpt.reason]})`, disabled: !chapterOpt.ok }]} />
        </label>
        <label className="play-map" title={T.levelTitle}>
          <span>{T.levelLabel}</span>
          <Select value={testLevel.kind} onChange={switchLevelKind} aria-label={T.levelLabel}
            options={[{ value: 'off', label: T.levelOff }, { value: 'auto', label: autoLevel && testLevel.kind === 'auto' ? T.levelAutoWith(autoLevel) : T.levelAuto },
              { value: 'number', label: T.levelNumber }]} />
        </label>
        {testLevel.kind === 'number' && (
          <input type="number" className="input play-level" aria-label={T.levelNumberLabel} aria-invalid={numberBad} title={numberBad ? T.levelBad(TEST_LEVEL_MIN, TEST_LEVEL_MAX) : T.levelNumberLabel}
            min={TEST_LEVEL_MIN} max={TEST_LEVEL_MAX} value={draft ?? String(testLevel.n)}
            onChange={(e) => setDraft(e.target.value)} onBlur={commitNumber} onKeyDown={(e) => { if (e.key === 'Enter') commitNumber() }} />
        )}
        {difficulties.length > 0 && (
          <label className="play-map" title={T.difficultyTitle}>
            <span>{T.difficultyLabel}</span>
            <Select value={difficulty} onChange={(v) => void switchDifficulty(v)} aria-label={T.difficultyLabel}
              options={[{ value: '', label: T.difficultyAsk }, ...difficulties.map((d) => ({ value: d.id, label: d.name || d.id }))]} />
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
      <div className="play-stage" ref={stage} style={layout.stage}>
        <iframe key={`${map}:${style}:${showAll}:${sound}:${difficulty}:${chapter ?? ''}:${testLevel.kind}${testLevel.kind === 'number' ? testLevel.n : ''}`} ref={frame}
          src={playerUrl(store.id, map, { style: style !== playable[0]?.id ? style : undefined, showAll, muted: !sound, difficulty: difficulty || undefined, chapter, testLevel })} title="Game preview" className="play-frame" style={fitCss}
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
      <footer className="play-foot" ref={foot}>
        <p className="play-hint"><strong>{T.controlsTitle}:</strong> {T.controls.map(([what, keys]) => `${what}: ${keys}`).join(' · ')}</p>
      </footer>
    </dialog>
  )
}
