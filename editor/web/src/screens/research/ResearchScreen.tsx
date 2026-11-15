// The Research section: technologies a side can research (list + form, like Items).
import { useState } from 'react'
import { addTechnology, deleteTechnology, duplicateTechnology, moveTechnology, newTechId, turnsOf } from '../../model/research'
import { researchNeedsStructure } from '../../model/optionRules'
import { technologyUsageLines } from '../../model/researchRefs'
import { navigate, Link, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useProject, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { EconomyChecks } from './EconomyChecks'
import { ResearchForm } from './ResearchForm'
import { T } from './text'

export function ResearchScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen">
      <Needs parts={['game', 'units']}><ResearchGate itemId={itemId} /></Needs>
    </div>
  )
}

/** A damaged data/research.json loads as "none"; editing would overwrite it, so refuse until it is fixed. */
function ResearchGate({ itemId }: { itemId?: string }) {
  const { doc, problems } = useProject()
  const bad = problems.filter((p) => p.file === 'data/research.json')
  if (doc.research === null && bad.length > 0) {
    return (
      <Banner tone="error" title={T.blockedTitle}>
        <p>{T.blockedBody}</p>
        {bad.map((p, i) => <p key={i}>{p.message}</p>)}
      </Banner>
    )
  }
  return <ResearchList itemId={itemId} />
}

function ResearchList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const list = doc.research ?? []
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'research', id))
  const target = list.find((t) => t.id === deleting)
  const resources = doc.game?.resources ?? []
  const costText = (cost: Record<string, number> | undefined) => {
    const parts = resources.filter((r) => cost?.[r.id]).map((r) => `${cost![r.id]} ${r.name}`)
    return parts.length ? parts.join(', ') : T.free
  }
  // Only when the game's rule says research needs a structure does the list matter.
  const noPlace = researchNeedsStructure(doc) && !(doc.structures ?? []).some((s) => s.research === true)
  const structuresLink = <Link to={projectPath(store.id, 'structures')}>{T.emptyStructures}</Link>

  return (
    <>
      <EconomyChecks section="research" />
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId}
        note={<>{T.intro}{list.length === 0 ? <> {T.emptyExplain}{researchNeedsStructure(doc) ? <> {T.emptyExplainStructure} {structuresLink}.</> : null}</> : null}{list.length > 0 && noPlace
          ? <span data-testid="no-research-structure"> {T.noResearchStructure} {structuresLink}.</span> : null}</>}
        items={list.map((t) => ({ id: t.id, label: t.name, sub: T.subtitle(costText(t.cost), turnsOf(t)) }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => {
          const src = list.find((t) => t.id === id)!
          const nid = newTechId(doc, `${src.name} copy`)
          edit((d) => duplicateTechnology(d, id, nid)); go(nid)
        }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = list.findIndex((t) => t.id === id); edit((d) => moveTechnology(d, i, i + delta)) }}>
        {(id) => <ResearchForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newTechId(doc, name); edit((d) => addTechnology(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? technologyUsageLines(doc, target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteTechnology(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
