// The Skills screen (docs/SRPG.md "Editor", step E2): list on the left, form on the right, like Abilities; plus the installer
// of the starter set and the checks.
import { useMemo, useState } from 'react'
import { skillChecks } from '../../model/skillChecks'
import { addSkill, blocksOf, duplicateSkill, moveSkill } from '../../model/skills'
import { blockSummary } from '../../model/skillText'
import { deleteSkill, newSkillId, skillUsageLines } from '../../model/srpgRefs'
import { navigate, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { SrpgScreen } from '../srpg/SrpgScreen'
import './skills.css'
import { SkillForm } from './SkillForm'
import { StarterDialog } from './StarterDialog'
import { T } from './text'

export function SkillsScreen({ itemId }: { itemId?: string }) {
  return <SrpgScreen feature="skills"><SkillList itemId={itemId} /></SrpgScreen>
}

function SkillList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const list = doc.skills ?? []
  const [naming, setNaming] = useState(false)
  const [installing, setInstalling] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'skills', id))
  const target = list.find((s) => s.id === deleting)
  const checks = useMemo(() => skillChecks(doc), [doc])
  // Unused skills are the common case right after the starter set is added: say so in one line instead of thirty.
  const unused = checks.filter((c) => c.text.includes('nobody has this skill yet'))
  const others = checks.filter((c) => !unused.includes(c))
  const lines = [...others.map((c) => c.text), ...(unused.length > 3 ? [T.manyUnused(unused.length)] : unused.map((c) => c.text))]

  return (
    <>
      {lines.length > 0 && (
        <Banner tone="warn" title={T.checksTitle}>
          <ul className="usage-list" data-testid="skill-checks">{lines.map((t, i) => <li key={i}>{t}</li>)}</ul>
        </Banner>
      )}
      <div className="starter-bar"><button type="button" className="btn" data-testid="starter-open" onClick={() => setInstalling(true)}>{T.installButton}</button></div>
      <MasterDetail title={T.title} noun={T.noun} emptyText={T.empty} heading="h2" selectedId={itemId}
        items={list.map((s) => ({ id: s.id, label: s.name, sub: blocksOf(s)[0] ? blockSummary(doc, blocksOf(s)[0]).replace(/\.$/, '') : '' }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDuplicate={(id) => { const nid = newSkillId(doc, `${list.find((s) => s.id === id)!.name} copy`); edit((d) => duplicateSkill(d, id, nid)); go(nid) }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = list.findIndex((s) => s.id === id); edit((d) => moveSkill(d, i, i + delta)) }}>
        {(id) => <SkillForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={T.newTitle} label={T.newLabel} help={T.newHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newSkillId(doc, name); edit((d) => addSkill(d, name, id)); setNaming(false); go(id) }} />
      <StarterDialog doc={doc} open={installing} onClose={() => setInstalling(false)} onInstall={(r) => edit(() => r.doc)} />
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(target.name) : ''} usages={target ? skillUsageLines(doc, target.id) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteSkill(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
