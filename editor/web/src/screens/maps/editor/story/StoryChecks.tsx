// The amber banner of story mistakes (the same look as the item checks): what the game loader would refuse.
import type { StoryCheck } from '../../../../model/storyChecks'
import { Banner } from '../../../../ui/Banner'
import { S } from './text'

export function StoryChecks({ checks, testId = 'story-checks', compact }: { checks: StoryCheck[]; testId?: string; compact?: boolean }) {
  if (checks.length === 0) return null
  return (
    <Banner tone="warn" title={compact ? undefined : S.checksTitle}>
      <ul className="usage-list" data-testid={testId}>{checks.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
      {!compact && <p className="help">{S.checksHelp}</p>}
    </Banner>
  )
}
