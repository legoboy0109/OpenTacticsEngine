import { useEffect, useRef, useState } from 'react'
import { problemTarget } from '../model/problems'
import { srpgSectionShown } from '../model/srpgFeatures'
import { SECTIONS, type Section } from '../model/types'
import { Link, projectPath } from '../router'
import { useProject, useStore } from '../store/context'
import { useNarrow } from '../ui/hooks'
import { Icon } from '../ui/Icon'

export const SECTION_LABEL: Record<Section, string> = {
  game: 'Game',
  rules: 'Rules',
  units: 'Units',
  characters: 'Characters',
  skills: 'Skills',
  abilities: 'Abilities',
  conditions: 'Conditions',
  supports: 'Supports',
  weapons: 'Weapons',
  items: 'Items',
  terrain: 'Terrain',
  structures: 'Structures',
  research: 'Research',
  art: 'Art',
  sound: 'Sound',
  maps: 'Maps',
  campaign: 'Campaign',
}

// Placeholder wording (owner to choose): the phone bar's button label.
const MENU_LABEL = 'Menu'

/** Left sidebar on wide screens (a scrolling list). On phones: one row with the current section's name and a menu button; the
 *  full list opens as a panel under that row. The `nav` named "Sections" is the same element in both layouts, so links are found
 *  by the same name; closed on a phone it is `hidden` (out of the tab order and the accessibility tree). */
export function Sidebar({ current }: { current: Section }) {
  const store = useStore()
  const { problems, doc } = useProject()
  const phone = useNarrow()
  const [openState, setOpen] = useState(false)
  const open = phone && openState
  const wrap = useRef<HTMLDivElement>(null)
  const button = useRef<HTMLButtonElement>(null)
  const nav = useRef<HTMLElement>(null)
  const counts: Partial<Record<Section, number>> = {}
  for (const p of problems) {
    const s = problemTarget(p.file, doc).section
    counts[s] = (counts[s] ?? 0) + 1
  }

  // Focus goes into the panel when it opens (on the current section's link); closing it with the keyboard or a choice puts it
  // back on the button. Escape, a tap outside and tabbing out all close it.
  useEffect(() => {
    if (!open) return
    nav.current?.querySelector<HTMLElement>('a[aria-current="page"], a')?.focus()
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') { setOpen(false); button.current?.focus() } }
    const onDown = (e: Event) => {
      if (wrap.current?.contains(e.target as Node)) return
      setOpen(false)
      // Focus lands wherever the tap went; if that was nothing focusable, hand it back to the button.
      requestAnimationFrame(() => { if (!document.activeElement || document.activeElement === document.body) button.current?.focus() })
    }
    document.addEventListener('keydown', onKey)
    document.addEventListener('pointerdown', onDown)
    return () => { document.removeEventListener('keydown', onKey); document.removeEventListener('pointerdown', onDown) }
  }, [open])

  const total = Object.values(counts).reduce((a, b) => a + b, 0)
  return (
    <div className="section-bar" ref={wrap}
      onBlur={(e) => { if (open && e.relatedTarget && !wrap.current?.contains(e.relatedTarget as Node)) setOpen(false) }}>
      <button type="button" ref={button} className="section-bar-button" aria-expanded={open} aria-controls="section-panel"
        data-learn="nav-menu" onClick={() => setOpen(!open)}>
        <span className="section-bar-name">{SECTION_LABEL[current]}</span>
        {total ? <span className="nav-dot" title={`${total} to look at`}><span className="sr-only">{total} problems</span></span> : null}
        <span className="section-bar-menu"><Icon name="list" />{MENU_LABEL}</span>
      </button>
      <nav className="sidebar" id="section-panel" ref={nav} aria-label="Sections" hidden={phone && !open}
        onClick={(e) => { if (phone && (e.target as HTMLElement).closest('a')) { setOpen(false); button.current?.focus() } }}>
        <ul>
          {SECTIONS.filter((s) => srpgSectionShown(doc, s, current)).map((s) => (
            <li key={s}>
              <Link to={projectPath(store.id, s)} className="nav-link" data-learn={`nav-${s}`} aria-current={s === current ? 'page' : undefined}>
                {SECTION_LABEL[s]}
                {counts[s] ? <span className="nav-dot" title={`${counts[s]} to look at`}><span className="sr-only">{counts[s]} problems</span></span> : null}
              </Link>
            </li>
          ))}
        </ul>
      </nav>
    </div>
  )
}
