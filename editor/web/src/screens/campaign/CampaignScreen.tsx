// The Campaign section: no campaign yet (what it is, and a button), or the story's settings, its flow and its chapters.
import { createCampaign } from '../../model/campaign'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useProject } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, PageHeader } from '../../ui/Card'
import { useStored } from '../../ui/hooks'
import { CampaignChecks } from './CampaignChecks'
import { CampaignList } from './CampaignList'
import { TimelineChapter } from './timeline/TimelineChapter'
import { TimelineView } from './timeline/TimelineView'
import { ViewSwitch, VIEW_KEY, type CampaignView } from './timeline/ViewSwitch'
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
  const [view, setView] = useStored<CampaignView>(VIEW_KEY, 'list')
  return (
    <>
      <PageHeader title={C.title} help={C.intro} />
      <CampaignChecks />
      <ViewSwitch view={view} onChange={setView} />
      {view === 'timeline' ? (itemId ? <TimelineChapter id={itemId} /> : <TimelineView />) : <CampaignList itemId={itemId} />}
    </>
  )
}
