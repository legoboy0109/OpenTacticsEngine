import { problemTarget } from '../model/problems'
import { SECTIONS, type Section } from '../model/types'
import { Link, projectPath } from '../router'
import { useProject, useStore } from '../store/context'

export const SECTION_LABEL: Record<Section, string> = {
  game: 'Game',
  rules: 'Rules',
  units: 'Units',
  weapons: 'Weapons',
  items: 'Items',
  terrain: 'Terrain',
  structures: 'Structures',
  art: 'Art',
  maps: 'Maps',
}

/** Left sidebar on wide screens; a compact switcher row on phones (same markup, CSS decides). */
export function Sidebar({ current }: { current: Section }) {
  const store = useStore()
  const { problems, doc } = useProject()
  const counts: Partial<Record<Section, number>> = {}
  for (const p of problems) {
    const s = problemTarget(p.file, doc).section
    counts[s] = (counts[s] ?? 0) + 1
  }
  return (
    <nav className="sidebar" aria-label="Sections">
      <ul>
        {SECTIONS.map((s) => (
          <li key={s}>
            <Link to={projectPath(store.id, s)} className="nav-link" aria-current={s === current ? 'page' : undefined}>
              {SECTION_LABEL[s]}
              {counts[s] ? <span className="nav-dot" title={`${counts[s]} to look at`}><span className="sr-only">{counts[s]} problems</span></span> : null}
            </Link>
          </li>
        ))}
      </ul>
    </nav>
  )
}
