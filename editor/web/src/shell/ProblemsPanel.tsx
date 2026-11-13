import { useEffect } from 'react'
import { problemTarget } from '../model/problems'
import { navigate, projectPath } from '../router'
import { useProject, useStore } from '../store/context'
import { Icon } from '../ui/Icon'
import { SECTION_LABEL } from './Sidebar'

/** The helper's problem list. The message text is shown exactly as the helper wrote it. */
export function ProblemsPanel({ onClose, fixFirst }: { onClose: () => void; fixFirst?: boolean }) {
  const store = useStore()
  const { problems, doc, save } = useProject()
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose() }
    document.addEventListener('keydown', onKey)
    return () => document.removeEventListener('keydown', onKey)
  }, [onClose])
  const pending = save.state !== 'saved'
  return (
    <section id="problems-panel" className="problems-panel" aria-label="Problems">
      <header>
        <h2>Problems</h2>
        <button type="button" className="btn ghost sm icon" aria-label="Close problems" onClick={onClose}><Icon name="close" /></button>
      </header>
      {fixFirst && problems.length > 0 && <p className="attention" role="alert">Fix these before playing.</p>}
      {problems.length === 0 && <p className="help">The game loads and starts without problems.</p>}
      {pending && problems.length > 0 && <p className="help">This list is from the last save; it updates after the next one.</p>}
      <ul>
        {problems.map((p, i) => {
          const t = problemTarget(p.file, doc)
          return (
            <li key={i}>
              <button type="button" className="problem" onClick={() => { navigate(projectPath(store.id, t.section, t.itemId)); onClose() }}>
                <span className="problem-where">{SECTION_LABEL[t.section]}{p.file ? ` (${p.file})` : ''}</span>
                <span className="problem-msg">{p.message}</span>
              </button>
            </li>
          )
        })}
      </ul>
    </section>
  )
}
