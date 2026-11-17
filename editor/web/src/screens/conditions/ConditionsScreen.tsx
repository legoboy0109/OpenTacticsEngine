// The Conditions screen (docs/SRPG.md section 9, docs/EDITOR_UI.md "Conditions"): list on the left, form on the right, like Skills and Abilities; a bar with
// "Add the starter set" (always there: on an empty screen it is the way in, later it fills in what was deleted); and what is worth a look.
import { useMemo, useState } from 'react'
import { conditionChecks } from '../../model/conditionChecks'
import { conditionListLine } from '../../model/conditionText'
import { deleteCondition, conditionUsageLines } from '../../model/conditionRefs'
import { addCondition, duplicateCondition, moveCondition, newConditionId } from '../../model/conditions'
import { installStarterConditions, type StarterConditionsResult } from '../../model/starterConditions'
import { navigate, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { SrpgScreen } from '../srpg/SrpgScreen'
import '../skills/skills.css'
import { ConditionForm } from './ConditionForm'
import { T } from './text'

export function ConditionsScreen({ itemId }: { itemId?: string }) {
  return <SrpgScreen feature="conditions"><ConditionList itemId={itemId} /></SrpgScreen>
}

function ConditionList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const list = doc.conditions ?? []
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const [result, setResult] = useState<StarterConditionsResult | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'conditions', id))
  const target = list.find((c) => c.id === deleting)
  const notes = useMemo(() => conditionChecks(doc).filter((c) => !c.blocks), [doc])

  // One Undo step: the whole set goes in with one edit.
  const addStarter = () => {
    const r = installStarterConditions(doc)
    if (r.added.length > 0) edit(() => r.doc)
    setResult(r)
  }

  return (
    <>
      <div className="starter-bar">
        <button type="button" className="btn" data-testid="starter-conditions" data-learn="starter-conditions" onClick={addStarter}>{T.starterButton}</button>
        <span className="help"> {T.starterHelp}</span>
      </div>
      {result && (
        <Banner tone={result.added.length > 0 ? 'info' : 'warn'} title={T.starterButton}>
          <div data-testid="starter-conditions-result">
            {result.added.length > 0 && <p>{T.starterAdded(result.added.length)} {result.added.join(', ')}.</p>}
            {result.added.length === 0 && result.existing.length > 0 && result.missingStat.length === 0 && <p>{T.starterNothing}</p>}
            {result.added.length > 0 && result.existing.length > 0 && <p>{T.starterSkippedExisting(result.existing.join(', '))}</p>}
            {result.missingStat.length > 0 && <p>{T.starterSkippedStat(result.missingStat.join(', '))}</p>}
          </div>
        </Banner>
      )}
      {notes.length > 0 && (
        <Banner tone="warn" title={T.checksTitle}>
          <ul className="usage-list" data-testid="condition-checks">{notes.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
        </Banner>
      )}
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} heading="h2" selectedId={itemId}
        items={list.map((c) => ({ id: c.id, label: c.name, sub: conditionListLine(c) }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => { const nid = newConditionId(doc, `${list.find((c) => c.id === id)!.name} copy`); edit((d) => duplicateCondition(d, id, nid)); go(nid) }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = list.findIndex((c) => c.id === id); edit((d) => moveCondition(d, i, i + delta)) }}>
        {(id) => <ConditionForm id={id} />}
      </MasterDetail>
      {list.length >= 100 && <p className="help">{T.limit}</p>}
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newConditionId(doc, name); edit((d) => addCondition(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? conditionUsageLines(doc, target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteCondition(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
