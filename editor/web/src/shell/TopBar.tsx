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
}

export function TopBar({ problemsOpen, onToggleProblems }: Props) {
  const store = useStore()
  const { doc, save, problems, canUndo, canRedo } = useProject()
  const name = doc.game?.name || store.id
  const n = problems.length
  return (
    <header className="topbar">
      <Link to="/" className="topbar-name" aria-label={`Back to all games (now editing ${name})`}>
        <Icon name="back" />
        <span className="topbar-title">{name}</span>
      </Link>
      <div className="topbar-status" role="status" aria-live="polite" data-state={save.state} title={save.message}>
        {save.state === 'saved' && <Icon name="check" />}
        <span>{SAVE_TEXT[save.state]}</span>
      </div>
      <div className="topbar-tools">
        <button type="button" className="btn ghost icon" onClick={() => store.undo()} disabled={!canUndo} aria-label="Undo" title="Undo (Ctrl+Z)"><Icon name="undo" /></button>
        <button type="button" className="btn ghost icon" onClick={() => store.redo()} disabled={!canRedo} aria-label="Redo" title="Redo (Ctrl+Shift+Z)"><Icon name="redo" /></button>
        <button type="button" className={`btn problems-badge${n ? ' has-problems' : ''}`} onClick={onToggleProblems} aria-expanded={problemsOpen} aria-controls="problems-panel">
          <Icon name={n ? 'warn' : 'check'} />
          {n === 0 ? 'No problems' : n === 1 ? '1 problem' : `${n} problems`}
        </button>
        <span title="Play-testing inside the editor is coming soon" className="play-wrap">
          <button type="button" className="btn primary" disabled aria-label="Play (coming soon)"><Icon name="play" />Play</button>
        </span>
      </div>
    </header>
  )
}
