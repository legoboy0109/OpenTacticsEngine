// "List | Timeline": the two ways to look at the campaign. The last one chosen is remembered in this browser.
import { TL } from './text'

export type CampaignView = 'list' | 'timeline'
export const VIEW_KEY = 'ote.campaignView'

export function ViewSwitch({ view, onChange }: { view: CampaignView; onChange: (v: CampaignView) => void }) {
  return (
    <div className="tl-switch" role="tablist" aria-label={TL.switchLabel}>
      {(['list', 'timeline'] as const).map((v) => (
        <button key={v} type="button" role="tab" aria-selected={view === v} className="tab" data-testid={`campaign-view-${v}`} onClick={() => onChange(v)}>
          {v === 'list' ? TL.viewList : TL.viewTimeline}
        </button>
      ))}
    </div>
  )
}
