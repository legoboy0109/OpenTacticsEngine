// The Abilities screen (docs/SRPG.md "Editor", step E3): list on the left, form on the right, like Items.
import { useMemo, useState } from 'react'
import { addAbility, duplicateAbility, moveAbility } from '../../model/abilities'
import { abilityChecks } from '../../model/abilityChecks'
import { abilitySummary } from '../../model/abilityText'
import { newAbilityId, abilityUsageLines, deleteAbility } from '../../model/srpgRefs'
import { navigate, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { SrpgScreen } from '../srpg/SrpgScreen'
import { AbilityForm } from './AbilityForm'
import { T } from './text'

export function AbilitiesScreen({ itemId }: { itemId?: string }) {
  return <SrpgScreen feature="abilities"><AbilityList itemId={itemId} /></SrpgScreen>
}

function AbilityList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const list = doc.abilities ?? []
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'abilities', id))
  const target = list.find((a) => a.id === deleting)
  const checks = useMemo(() => abilityChecks(doc), [doc])

  return (
    <>
      {checks.length > 0 && (
        <Banner tone="warn" title={T.checksTitle}>
          <ul className="usage-list" data-testid="ability-checks">{checks.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
        </Banner>
      )}
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} heading="h2" selectedId={itemId}
        items={list.map((a) => ({ id: a.id, label: a.name, sub: abilitySummary(doc, a).split(';')[0].replace(/\.$/, '') }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => { const nid = newAbilityId(doc, `${list.find((a) => a.id === id)!.name} copy`); edit((d) => duplicateAbility(d, id, nid)); go(nid) }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = list.findIndex((a) => a.id === id); edit((d) => moveAbility(d, i, i + delta)) }}>
        {(id) => <AbilityForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newAbilityId(doc, name); edit((d) => addAbility(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? abilityUsageLines(doc, target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteAbility(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
