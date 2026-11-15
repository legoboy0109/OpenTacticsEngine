import type { RefObject } from 'react'
import { LearnButton } from '../learn/LearnButton'
import { Link } from '../router'
import { useProject, useStore } from '../store/context'
import type { SaveStatus } from '../store/autosave'
import { Icon } from '../ui/Icon'

const SAVE_TEXT: Record<SaveStatus['state'], string> = {
  saved: 'Saved',
  dirty: 'Unsaved changes',
  saving: 'Saving…',
  error: 'Could not save',
  conflict: 'Not saved: changed elsewhere',
}

interface Props {
  problemsOpen: boolean
  onToggleProblems: () => void
  onPlay: () => void
  playBusy: boolean
  playDisabled: boolean
  playRef: RefObject<HTMLButtonElement>
}

export function TopBar({ problemsOpen, onToggleProblems, onPlay, playBusy, playDisabled, playRef }: Props) {
  const store = useStore()
  const { doc, save, problems, canUndo, canRedo } = useProject()
  const name = doc.game?.name || store.id
  const n = problems.length
  const label = n === 0 ? 'No problems' : n === 1 ? '1 problem' : `${n} problems`
  return (
    <header className="topbar">
      <Link to="/" className="topbar-name" aria-label={`Back to all games (now editing ${name})`}>
        <Icon name="back" />
        <span className="topbar-title">{name}</span>
      </Link>
      <div className="topbar-status" role="status" aria-live="polite" data-state={save.state} title={save.message}>
        {save.state === 'saved' && <Icon name="check" />}
        <span className="status-text">{SAVE_TEXT[save.state]}</span>
      </div>
      <div className="topbar-tools">
        <button type="button" className="btn ghost icon" onClick={() => store.undo()} disabled={!canUndo} aria-label="Undo" title="Undo (Ctrl+Z)"><Icon name="undo" /></button>
        <button type="button" className="btn ghost icon" onClick={() => store.redo()} disabled={!canRedo} aria-label="Redo" title="Redo (Ctrl+Shift+Z)"><Icon name="redo" /></button>
        <LearnButton />
        <button type="button" data-learn="problems" className={`btn problems-badge${n ? ' has-problems' : ''}`} onClick={onToggleProblems} aria-expanded={problemsOpen} aria-controls="problems-panel" aria-label={label}>
          <Icon name={n ? 'warn' : 'check'} />
          <span className="badge-text">{label}</span>
          {n > 0 && <span className="badge-count" aria-hidden="true">{n}</span>}
        </button>
        <button type="button" ref={playRef} data-learn="play" className="btn primary play-btn" onClick={() => { if (!playBusy) onPlay() }} disabled={playDisabled} aria-busy={playBusy || undefined} aria-label="Play" title="Play this game on the map you are editing">
          <Icon name="play" /><span className="btn-text">Play</span>
        </button>
      </div>
    </header>
  )
}
