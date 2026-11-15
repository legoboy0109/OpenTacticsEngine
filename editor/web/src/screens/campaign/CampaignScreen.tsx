// The Campaign section: no campaign yet (what it is, and a button), or the story's settings, its flow and its chapters.
import { useState } from 'react'
import { addChapter, deleteChapter, moveChapter, newChapterId, createCampaign, sceneOf } from '../../model/campaign'
import { campaignUsagesOf } from '../../model/campaignRefs'
import { chapterName } from '../../model/campaignTypes'
import { commandsSummary } from '../../model/storySummary'
import { sceneMap } from '../../model/campaignRoster'
import { navigate, projectPath } from '../../router'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useProject, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, PageHeader } from '../../ui/Card'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { NameDialog } from '../../ui/NameDialog'
import { CampaignChecks } from './CampaignChecks'
import { ChapterForm } from './ChapterForm'
import { FlowCard } from './FlowCard'
import { SettingsCard } from './SettingsCard'
import { C } from './text'

export function CampaignScreen({ itemId }: { itemId?: string }) {
  return (
    <div className="screen wide">
      <Needs parts={['game']}><CampaignGate itemId={itemId} /></Needs>
    </div>
  )
}

/** A damaged data/campaign.json loads as "none"; creating or editing would overwrite it, so refuse until it is fixed. */
function CampaignGate({ itemId }: { itemId?: string }) {
  const { doc, problems } = useProject()
  const bad = problems.filter((p) => p.file === 'data/campaign.json')
  if (doc.campaign === null && bad.length > 0) {
    return (
      <Banner tone="error" title={C.brokenTitle}>
        {bad.map((p, i) => <p key={i}>{p.message}</p>)}
        <p>{C.brokenBody}</p>
      </Banner>
    )
  }
  return doc.campaign ? <CampaignEditor itemId={itemId} /> : <NoCampaign />
}

function NoCampaign() {
  const doc = useDoc()
  const edit = useEdit()
  const none = (doc.game?.maps.length ?? 0) === 0
  return (
    <>
      <PageHeader title={C.title} help={C.intro} />
      <Card title={C.emptyTitle}>
        <div className="stack tight" data-testid="no-campaign">
          <p>{C.emptyBody}</p>
          {none && <p className="help">{C.noMaps}</p>}
          <div><button type="button" className="btn primary" disabled={none} onClick={() => edit(createCampaign)}>{C.create}</button></div>
        </div>
      </Card>
    </>
  )
}

function CampaignEditor({ itemId }: { itemId?: string }) {
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
      <PageHeader title={C.title} help={C.intro} />
      <CampaignChecks />
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
