import { useEffect, useState } from 'react'
import { api, type TemplateInfo } from '../../api/client'
import { projectSlug } from '../../model/ids'
import { navigate, projectPath } from '../../router'
import { Advanced } from '../../ui/Card'
import { Dialog } from '../../ui/Dialog'
import { Field, FieldGroup } from '../../ui/Field'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

interface Props { open: boolean; existing: string[]; onClose: () => void }

export function NewGameDialog({ open, ...rest }: Props) {
  if (!open) return null
  return <Form {...rest} />
}

/** "My Game" -> "My Game 2" (or bump a trailing number) until its folder name is free. */
function suggestName(name: string, taken: string[]): string {
  const m = name.match(/^(.*?)(?:\s+(\d+))?$/)
  const base = (m?.[1] ?? name).trim() || 'Game'
  for (let n = Number(m?.[2] ?? 1) + 1; ; n++) {
    const cand = `${base} ${n}`
    if (!taken.includes(projectSlug(cand))) return cand
  }
}

function Form({ existing, onClose }: Omit<Props, 'open'>) {
  const [name, setName] = useState('')
  const [templates, setTemplates] = useState<TemplateInfo[]>([])
  const [template, setTemplate] = useState('')
  const [idOverride, setIdOverride] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [suggestion, setSuggestion] = useState<string | null>(null)
  const [taken, setTaken] = useState(existing)

  useEffect(() => {
    void api.listTemplates().then((r) => {
      if (!r.ok) { setError(r.error); return }
      setTemplates(r.value)
      setTemplate((cur) => cur || (r.value.find((t) => t.id === 'strategy-rpg') ?? r.value[0])?.id || '')
    })
  }, [])

  const id = idOverride ? idOverride : projectSlug(name)
  const idTaken = taken.includes(id)
  const canCreate = name.trim() !== '' && template !== '' && !busy

  const create = async () => {
    if (!canCreate) return
    if (idTaken) { setError(T.taken(name.trim())); setSuggestion(suggestName(name.trim(), taken)); return }
    setBusy(true); setError(null); setSuggestion(null)
    const res = await api.createProject({ id, name: name.trim(), template })
    setBusy(false)
    if (res.ok) { onClose(); navigate(projectPath(res.value.id, 'game')); return }
    if (res.status === 409) {
      const next = [...taken, id]
      setTaken(next)
      setError(T.taken(name.trim()))
      setSuggestion(idOverride === null ? suggestName(name.trim(), next) : null)
      return
    }
    setError(res.error)
  }

  return (
    <Dialog open title={T.dialogTitle} onClose={onClose} onSubmit={() => void create()}
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>{T.cancel}</button>
          <button type="submit" className="btn primary" disabled={!canCreate}>{busy ? T.creating : T.create}</button>
        </>
      )}>
      <div className="stack">
        <Field label={T.nameLabel} help={T.nameHelp} error={error}>
          <TextInput value={name} onChange={(v) => { setName(v); setError(null); setSuggestion(null) }} />
        </Field>
        {suggestion && (
          <div><button type="button" className="btn sm" onClick={() => { setName(suggestion); setError(null); setSuggestion(null) }}>{T.useSuggestion(suggestion)}</button></div>
        )}
        <FieldGroup label={T.templateLabel} help={T.templateHelp}>
          <div className="template-grid" role="radiogroup" aria-label={T.templateLabel}>
            {templates.map((t) => (
              <label key={t.id} className="template-card" data-selected={t.id === template}>
                <input type="radio" name="template" value={t.id} checked={t.id === template} onChange={() => setTemplate(t.id)} />
                <span className="template-name">{t.name}</span>
                <span className="template-desc">{t.description}</span>
              </label>
            ))}
          </div>
        </FieldGroup>
        <Advanced label={T.advanced}>
          <Field label={T.folderLabel} help={T.folderHelp}>
            <TextInput value={id} onChange={(v) => setIdOverride(v.toLowerCase().replace(/[^a-z0-9-]/g, '').replace(/^-+/, '').slice(0, 40))} mono />
          </Field>
        </Advanced>
      </div>
    </Dialog>
  )
}
