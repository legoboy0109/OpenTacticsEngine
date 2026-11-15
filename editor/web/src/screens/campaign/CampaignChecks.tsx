// The amber banners of campaign mistakes (same look as the item and story checks): what the loader would refuse, and
// hints about things it accepts. The Rules screen shows the part about switches, variables and resources (`topics`).
import { useMemo } from 'react'
import { campaignChecks, type CampaignCheck } from '../../model/campaignChecks'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { C } from './text'

export function CampaignChecks({ topics, chapterId }: { topics?: CampaignCheck['topic'][]; chapterId?: string }) {
  const doc = useDoc()
  const all = useMemo(() => campaignChecks(doc), [doc])
  const mine = all.filter((c) => (!topics || topics.includes(c.topic)) && (chapterId === undefined || c.chapterId === chapterId))
  const errors = mine.filter((c) => c.level === 'error')
  const hints = mine.filter((c) => c.level === 'hint')
  return (
    <>
      {errors.length > 0 && (
        <Banner tone="warn" title={C.checksTitle}>
          <ul className="usage-list" data-testid="campaign-checks">{errors.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
        </Banner>
      )}
      {hints.length > 0 && (
        <Banner tone="warn" title={C.hintsTitle}>
          <ul className="usage-list" data-testid="campaign-hints">{hints.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
        </Banner>
      )}
    </>
  )
}
