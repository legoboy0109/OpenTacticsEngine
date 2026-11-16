// "Add starter skills…": the thirty skills with checkboxes (all on), four pickers for the stats they point at, and Add.
// Skills whose id already exists, or whose stat is not chosen, are skipped and listed afterwards.
import { useState } from 'react'
import { PICKED_ROLES, STARTER_SKILLS, guessStats, installStarter, rolesNeeded, type InstallResult, type StatMap } from '../../model/starterSkills'
import type { ProjectDoc } from '../../model/types'
import { Check } from '../../ui/Toggle'
import { Dialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { T } from './text'

const LABEL = { atk: T.installAtk, skl: T.installSkl, spd: T.installSpd, def: T.installDef } as const

interface Props { doc: ProjectDoc; open: boolean; onClose: () => void; onInstall: (r: InstallResult) => void }

export function StarterDialog({ doc, open, onClose, onInstall }: Props) {
  if (!open) return null
  return <Form doc={doc} onClose={onClose} onInstall={onInstall} />
}

function Form({ doc, onClose, onInstall }: Omit<Props, 'open'>) {
  const [chosen, setChosen] = useState<string[]>(() => STARTER_SKILLS.map((s) => s.id))
  const [map, setMap] = useState<StatMap>(() => guessStats(doc))
  const [result, setResult] = useState<InstallResult | null>(null)
  const stats = (doc.game?.stats ?? []).filter((s) => s.id !== doc.game?.roles?.hp && s.id !== doc.game?.roles?.move)
  const options = [{ value: '', label: T.installNone }, ...stats.map((s) => ({ value: s.id, label: s.name }))]
  const toggle = (id: string, on: boolean) => setChosen((c) => (on ? [...c, id] : c.filter((x) => x !== id)))
  const have = new Set((doc.skills ?? []).map((s) => s.id))
  // Which of the checked skills cannot be added right now (named in the list, so the maker knows before pressing Add).
  const note = (id: string): string | null => {
    const sk = STARTER_SKILLS.find((s) => s.id === id)!
    if (have.has(id)) return null
    const missing = rolesNeeded(sk).filter((r) => !map[r])
    return missing.length ? T.installNeeds(missing.map((r) => T.installRole[r]).join(', ')) : null
  }

  if (result) {
    return (
      <Dialog open title={T.installDoneTitle} onClose={onClose} footer={<button type="button" className="btn primary" onClick={onClose}>{T.installClose}</button>}>
        <div className="stack" data-testid="starter-result">
          <p>{T.installAdded(result.added.length)}</p>
          {result.existing.length > 0 && <p>{T.installSkippedExisting(result.existing.join(', '))}</p>}
          {result.missingStat.length > 0 && <p>{T.installSkippedStat(result.missingStat.join(', '))}</p>}
        </div>
      </Dialog>
    )
  }
  const addable = chosen.filter((id) => !have.has(id) && note(id) === null).length
  const add = () => { const r = installStarter(doc, chosen, map); onInstall(r); setResult(r) }
  return (
    <Dialog open wide title={T.installTitle} onClose={onClose} onSubmit={add}
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>{T.installCancel}</button>
          <button type="submit" className="btn primary" disabled={chosen.length === 0}>{T.installAdd(addable)}</button>
        </>
      )}>
      <div className="stack">
        <p className="help">{T.installIntro}</p>
        <h3>{T.installStats}</h3>
        <div className="starter-stats">
          {PICKED_ROLES.map((r) => (
            <Field key={r} label={LABEL[r]}><Select value={map[r] ?? ''} options={options} onChange={(v) => setMap((m) => ({ ...m, [r]: v || undefined }))} /></Field>
          ))}
        </div>
        <div className="starter-actions">
          <button type="button" className="btn sm" onClick={() => setChosen(STARTER_SKILLS.map((s) => s.id))}>{T.installAll}</button>
          <button type="button" className="btn sm" onClick={() => setChosen([])}>{T.installNoneChecked}</button>
        </div>
        <div className="starter-list" data-testid="starter-list">
          {STARTER_SKILLS.map((s) => {
            const why = note(s.id)
            return <Check key={s.id} label={s.name} checked={chosen.includes(s.id)} help={have.has(s.id) ? 'Already in your game' : why ?? s.description} onChange={(v) => toggle(s.id, v)} />
          })}
        </div>
      </div>
    </Dialog>
  )
}
