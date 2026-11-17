// The Campaign screen's list view: the story's settings and flow, and the chapters as a list with their form (the original view).
import { useState } from 'react'
import { addChapter, deleteChapter, moveChapter, newChapterId, sceneOf } from '../../model/campaign'
import { campaignUsagesOf } from '../../model/campaignRefs'
import { chapterName } from '../../model/campaignTypes'
import { commandsSummary } from '../../model/storySummary'
import { sceneMap } from '../../model/campaignRoster'
import { navigate, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { ChapterForm } from './ChapterForm'
import { FlowCard } from './FlowCard'
import { SettingsCard } from './SettingsCard'
import { C } from './text'

export function CampaignList({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const campaign = doc.campaign!
  const chapters = campaign.chapters
  const [naming, setNaming] = useState(false)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'campaign', id))
  const target = chapters.find((c) => c.id === deleting)
  const ctx = { doc, map: sceneMap(doc) }
  const mapName = (id: string) => doc.game?.maps.find((m) => m.id === id)?.name ?? (id || C.noMapChosen)
  const sub = (id: string) => {
    const ch = chapters.find((c) => c.id === id)!
    const text = commandsSummary(ctx, sceneOf(doc, id, 'before'), '') || commandsSummary(ctx, sceneOf(doc, id, 'after'), '') || C.noScenes
    return C.chapterListSub(mapName(ch.map), text)
  }
  return (
    <>
      <div className="campaign-top">
        <SettingsCard />
        <FlowCard />
      </div>
      <MasterDetail heading="h2" title={C.chaptersTitle} noun={C.chapterNoun} emptyText={C.chaptersEmpty} selectedId={itemId}
        newLabel={C.newChapter}
        items={chapters.map((c) => ({ id: c.id, label: chapterName(c), sub: sub(c.id) }))}
        onSelect={go} onBack={() => go()} onNew={() => setNaming(true)}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = chapters.findIndex((x) => x.id === id); edit((d) => moveChapter(d, i, i + delta)) }}>
        {(id) => <ChapterForm id={id} />}
      </MasterDetail>
      <NameDialog open={naming} title={C.newChapterTitle} label={C.newChapterLabel} help={C.newChapterHelp} onCancel={() => setNaming(false)}
        onSubmit={(name) => { const id = newChapterId(doc, name); edit((d) => addChapter(d, name, id)); setNaming(false); go(id) }} />
      <DeleteConfirm open={!!target} what={target ? C.deleteChapterWhat(chapterName(target)) : ''}
        usages={target ? campaignUsagesOf(doc, [{ kind: 'chapter', id: target.id }]) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deleteChapter(d, target.id)); go() } setDeleting(null) }} />
    </>
  )
}
