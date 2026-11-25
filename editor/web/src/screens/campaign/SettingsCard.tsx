// The campaign's name and rules: what happens to defeated units and lost chapters, and which maps can be played alone.
import { playableMaps } from '../../model/cutScene'
import { useState } from 'react'
import {
  defeatRule, fallenRule, freeMaps, freeMode, freeNote, freeWhen, removeCampaign, setCampaignName, setDefeatRule, setFallenRule, setFreeMap,
  setFreeMode, setFreeNote, setFreeWhen, type FreeMode,
} from '../../model/campaign'
import type { DefeatRule, FallenRule, FreeWhen } from '../../model/campaignTypes'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { Check } from '../../ui/Toggle'
import { Field, FieldGroup } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { CampaignBackdropField } from './BackdropFields'
import { C } from './text'

export function SettingsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const [removing, setRemoving] = useState(false)
  const c = doc.campaign!
  const mode = freeMode(c)
  const chosen = new Set(freeMaps(c))
  return (
    <Card title={C.settingsTitle}
      actions={<button type="button" className="btn sm danger" onClick={() => setRemoving(true)}><Icon name="trash" />{C.remove}</button>}>
      <div className="stack">
        <Field label={C.name} help={C.nameHelp}>
          <TextInput value={c.name ?? ''} onChange={(v) => edit((d) => setCampaignName(d, v), 'campaign:name')} />
        </Field>
        <Field label={C.fallen} help={C.fallenHelp}>
          <Select value={fallenRule(c)} onChange={(v) => edit((d) => setFallenRule(d, v as FallenRule))}
            options={(['gone', 'return'] as FallenRule[]).map((v) => ({ value: v, label: C.fallenChoices[v] }))} />
        </Field>
        <Field label={C.onDefeat}>
          <Select value={defeatRule(c)} onChange={(v) => edit((d) => setDefeatRule(d, v as DefeatRule))}
            options={(['retry', 'end'] as DefeatRule[]).map((v) => ({ value: v, label: C.defeatChoices[v] }))} />
        </Field>
        <CampaignBackdropField />
        <Field label={C.freeTitle} help={C.freeHelp}>
          <Select value={mode} onChange={(v) => edit((d) => setFreeMode(d, v as FreeMode))}
            options={(['none', 'all', 'chosen'] as FreeMode[]).map((v) => ({ value: v, label: C.freeChoices[v] }))} />
        </Field>
        {mode === 'chosen' && (
          <FieldGroup label={C.freePick}>
            <div className="stack tight" data-testid="free-maps">
              {playableMaps(doc.game?.maps ?? []).map((m) => (
                <Check key={m.id} label={m.name || m.id} checked={chosen.has(m.id)} onChange={(on) => edit((d) => setFreeMap(d, m.id, on))} />
              ))}
            </div>
          </FieldGroup>
        )}
        {mode !== 'none' && (
          <Field label={C.freeWhen} help={C.freeWhenHelp}>
            <Select value={freeWhen(c)} onChange={(v) => edit((d) => setFreeWhen(d, v as FreeWhen))}
              options={(['always', 'storyComplete'] as FreeWhen[]).map((v) => ({ value: v, label: C.freeWhenChoices[v] }))} />
          </Field>
        )}
        {mode !== 'none' && freeWhen(c) === 'storyComplete' && (
          <Field label={C.freeNote} help={C.freeNoteHelp}>
            <TextInput value={freeNote(c)} onChange={(v) => edit((d) => setFreeNote(d, v), 'campaign:freeNote')} />
          </Field>
        )}
      </div>
      <ConfirmDialog open={removing} title={C.removeTitle} confirmLabel={C.removeConfirm} danger
        onCancel={() => setRemoving(false)} onConfirm={() => { edit(removeCampaign); setRemoving(false) }}>
        <p>{C.removeBody}</p>
        <p className="help">{C.removeUndo}</p>
      </ConfirmDialog>
    </Card>
  )
}
