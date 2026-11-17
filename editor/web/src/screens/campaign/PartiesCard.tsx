// "The parties" (docs/EVENTS.md "Parties"): a campaign can hold several parties, each with its own characters, items, storage and funds, under one
// story. The first in the list is the one the campaign starts with; scenes switch between them and merge them (the "Party" commands).
import { useState } from 'react'
import { campaignUsagesOf } from '../../model/campaignRefs'
import { addParty, deleteParty, moveParty, partiesOf, partyName, renameParty } from '../../model/parties'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { Field } from '../../ui/Field'
import { NameDialog } from '../../ui/NameDialog'
import { RowButtons } from '../../ui/RowButtons'
import { TextInput } from '../../ui/TextInput'
import { C } from './text'

export function PartiesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const parties = partiesOf(doc.campaign)
  const target = parties.find((p) => p.id === deleting)
  return (
    <Card title={C.partiesTitle} help={C.partiesHelp} id="campaign-parties"
      actions={<button type="button" className="btn sm" onClick={() => setNaming(true)}>{C.addParty}</button>}>
      {parties.length === 0 ? <p className="help" data-testid="no-parties">{C.partiesNone}</p> : (
        <ol className="party-list" data-testid="parties">
          {parties.map((p, i) => (
            <li key={p.id} className="party-row" data-testid={`party-${p.id}`}>
              <Field label={C.partyName(i + 1)} help={i === 0 ? C.partyStarting : undefined}>
                <TextInput value={p.name ?? ''} placeholder={p.id} onChange={(v) => edit((d) => renameParty(d, p.id, v), `party:${p.id}:name`)} />
              </Field>
              <RowButtons index={i} count={parties.length} up={C.partyUp(partyName(doc.campaign, p.id))} down={C.partyDown(partyName(doc.campaign, p.id))}
                del={C.partyDelete(partyName(doc.campaign, p.id))}
                onUp={() => edit((d) => moveParty(d, i, i - 1))} onDown={() => edit((d) => moveParty(d, i, i + 1))} onDelete={() => setDeleting(p.id)} />
            </li>
          ))}
        </ol>
      )}
      <NameDialog open={naming} title={C.newPartyTitle} label={C.newPartyLabel} help={C.newPartyHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { edit((d) => addParty(d, name)); setNaming(false) }} />
      <DeleteConfirm open={!!target} what={target ? C.deletePartyWhat(partyName(doc.campaign, target.id)) : ''}
        usages={target ? campaignUsagesOf(doc, [{ kind: 'party', id: target.id }]) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) edit((d) => deleteParty(d, target.id)); setDeleting(null) }} />
    </Card>
  )
}
