import type { Usage } from '../model/references'
import { ConfirmDialog } from './Dialog'

interface Props {
  open: boolean
  /** What is being deleted, e.g. `the weapon "Iron Sword"`. */
  what: string
  usages: Usage[]
  /** Extra consequences that are not plain usages. */
  notes?: string[]
  /** Set when the delete is not allowed; shown instead of the question. */
  blockedReason?: string | null
  onConfirm: () => void
  onCancel: () => void
}

const SHOWN = 12

/** "Delete X?" with the list of places that use it. Deleting removes those
 *  references in the same step, so one Undo brings everything back. */
export function DeleteConfirm({ open, what, usages, notes = [], blockedReason, onConfirm, onCancel }: Props) {
  const lines = [...usages.map((u) => u.text), ...notes]
  return (
    <ConfirmDialog open={open} title={blockedReason ? `Can't delete ${what}` : `Delete ${what}?`} confirmLabel="Delete" danger
      blocked={!!blockedReason} onConfirm={onConfirm} onCancel={onCancel}>
      {blockedReason ? <p>{blockedReason}</p> : (
        <>
          {lines.length === 0
            ? <p>Nothing else uses it.</p>
            : (
              <>
                <p>It is used in these places. Deleting it takes it out of all of them:</p>
                <ul className="usage-list">
                  {lines.slice(0, SHOWN).map((l, i) => <li key={i}>{l}</li>)}
                  {lines.length > SHOWN && <li>and {lines.length - SHOWN} more</li>}
                </ul>
              </>
            )}
          <p className="help">You can undo this with the Undo button.</p>
        </>
      )}
    </ConfirmDialog>
  )
}
