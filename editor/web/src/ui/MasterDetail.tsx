import type { ReactNode } from 'react'
import { useNarrow } from './hooks'
import { Icon } from './Icon'

export interface ListItem {
  id: string
  label: string
  sub?: string
  /** A colour chip shown before the label (terrain). */
  swatch?: string
  /** Something else shown before the label (a thumbnail). */
  thumb?: ReactNode
}

interface Props {
  /** Plural noun for the list ("Units"); used in headings and button names. */
  title: string
  /** Singular noun for buttons ("unit"). */
  noun: string
  items: ListItem[]
  /** From the URL; undefined means "nothing picked". */
  selectedId: string | undefined
  onSelect: (id: string) => void
  onBack: () => void
  onNew: () => void
  onDuplicate?: (id: string) => void
  onDelete?: (id: string) => void
  onMove?: (id: string, delta: -1 | 1) => void
  /** Form for the selected item. */
  children: (id: string) => ReactNode
  emptyText?: string
  /** Wide screens only: hide the list so the form gets the whole width (the map editor). */
  listHidden?: boolean
  /** Extra buttons next to Duplicate / Delete. */
  extraActions?: ReactNode
  /** Wording of the New / Delete buttons when "New {noun}" / "Delete" do not fit (default: New {noun}, Delete). */
  newLabel?: string
  deleteLabel?: string
  /** A short explanation shown under the list heading. */
  note?: ReactNode
  /** The list heading's level when the screen has a heading of its own above (default h1). */
  heading?: 'h1' | 'h2'
}

/** List on the left, form for the selected item on the right. On a phone the
 *  two are separate views: the list first, the form opens with a back button. */
export function MasterDetail({ title, noun, items, selectedId, onSelect, onBack, onNew, onDuplicate, onDelete, onMove, children, emptyText, listHidden, extraActions, newLabel, deleteLabel, note, heading: Heading = 'h1' }: Props) {
  const narrow = useNarrow()
  const known = items.some((i) => i.id === selectedId)
  // Wide screens always show a form; phones show it only once an item is picked.
  const current = known ? selectedId : narrow ? undefined : items[0]?.id
  const item = items.find((i) => i.id === current)
  const index = items.findIndex((i) => i.id === current)
  const showDetail = item !== undefined

  return (
    <div className="master-detail" data-view={showDetail && narrow ? 'detail' : 'list'} data-list={listHidden && !narrow ? 'hidden' : undefined}>
      <div className="md-list">
        <div className="md-list-head">
          <Heading>{title}</Heading>
          <button type="button" className="btn primary sm" data-learn="new-item" onClick={onNew}><Icon name="plus" />{newLabel ?? `New ${noun}`}</button>
        </div>
        {note && <p className="help">{note}</p>}
        {items.length === 0 && <p className="help">{emptyText ?? `No ${title.toLowerCase()} yet.`}</p>}
        <ul className="md-items" aria-label={title}>
          {items.map((i) => (
            <li key={i.id}>
              <button type="button" className="md-item" data-learn="list-item" data-learn-item={i.id} aria-current={i.id === current ? 'true' : undefined} onClick={() => onSelect(i.id)}>
                {i.swatch && <span className="swatch" style={{ background: i.swatch }} aria-hidden="true" />}
                {i.thumb}
                <span className="md-item-text">
                  <span className="md-item-label">{i.label || 'Unnamed'}</span>
                  {i.sub && <span className="md-item-sub">{i.sub}</span>}
                </span>
              </button>
            </li>
          ))}
        </ul>
      </div>
      <div className="md-detail">
        {item && (
          <>
            <div className="md-detail-head">
              {narrow && <button type="button" className="btn ghost sm" onClick={onBack}><Icon name="back" />{title}</button>}
              <h2 className="md-detail-title">{item.label || 'Unnamed'}</h2>
              <div className="md-detail-actions">
                {!narrow && extraActions}
                {onMove && (
                  <>
                    <button type="button" className="btn ghost sm icon" aria-label={`Move ${noun} up`} disabled={index <= 0} onClick={() => onMove(item.id, -1)}><Icon name="up" /></button>
                    <button type="button" className="btn ghost sm icon" aria-label={`Move ${noun} down`} disabled={index >= items.length - 1} onClick={() => onMove(item.id, 1)}><Icon name="down" /></button>
                  </>
                )}
                {onDuplicate && <button type="button" className="btn sm" onClick={() => onDuplicate(item.id)}><Icon name="copy" />Duplicate</button>}
                {onDelete && <button type="button" className="btn sm danger" onClick={() => onDelete(item.id)}><Icon name="trash" />{deleteLabel ?? 'Delete'}</button>}
              </div>
            </div>
            <div className="md-detail-body" key={item.id}>{children(item.id)}</div>
          </>
        )}
        {!item && !narrow && <p className="help">{emptyText ?? `Create a ${noun} to get started.`}</p>}
      </div>
    </div>
  )
}
