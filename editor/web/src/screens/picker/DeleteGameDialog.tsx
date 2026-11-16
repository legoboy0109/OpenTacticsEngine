import { useState } from 'react'
import { api, type ProjectInfo } from '../../api/client'
import { Dialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

interface Props { game: ProjectInfo | null; onClose: () => void; onDeleted: () => void }

/** Deleting a game removes its folder for good, so the maker types the game's name first. */
export function DeleteGameDialog({ game, ...rest }: Props) {
  if (!game) return null
  return <Form key={game.id} game={game} {...rest} />
}

/** What must be typed: the name as the card shows it. Spaces at the ends and capitals are forgiven. */
export const confirmMatches = (typed: string, shown: string) => typed.trim().toLowerCase() === shown.trim().toLowerCase()

function Form({ game, onClose, onDeleted }: { game: ProjectInfo; onClose: () => void; onDeleted: () => void }) {
  const shown = game.name || game.id
  const [typed, setTyped] = useState('')
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const ready = confirmMatches(typed, shown) && !busy

  const remove = async () => {
    if (!ready) return
    setBusy(true); setError(null)
    const res = await api.deleteProject(game.id)
    setBusy(false)
    // 404: it is already gone (deleted by hand, or in another tab), which is what was asked for.
    if (res.ok || res.status === 404) { onDeleted(); onClose(); return }
    setError(res.error)
  }

  return (
    <Dialog open title={T.deleteTitle(shown)} onClose={onClose} onSubmit={() => void remove()}
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>{T.cancel}</button>
          <button type="submit" className="btn danger" disabled={!ready}>{busy ? T.deleting : T.deleteNow}</button>
        </>
      )}>
      <div className="stack">
        <p>{T.deleteBody(game.id)}</p>
        <Field label={T.deleteConfirmLabel} help={T.deleteConfirmHelp(shown)} error={error}>
          <TextInput value={typed} onChange={(v) => { setTyped(v); setError(null) }} />
        </Field>
      </div>
    </Dialog>
  )
}
