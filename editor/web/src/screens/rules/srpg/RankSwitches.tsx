// The two switches "This game has weapon ranks" / "This game has stat caps" of the Strategy-RPG features card (docs/SRPG.md section 7).
// Like the other features, a switch that is on stays on after a reload even while empty (the empty file is written). Switching off
// asks first when something would be removed; ranks can only be switched on once weapons are carried (the loader refuses a ladder otherwise).
import { useState } from 'react'
import { canSwitchOnRanks, capsCount, ranksCount, ranksOn, switchOffCaps, switchOffRanks, switchOnCaps, switchOnRanks } from '../../../model/ranks'
import { useDoc, useEdit } from '../../../store/context'
import { ConfirmDialog } from '../../../ui/Dialog'
import { Toggle } from '../../../ui/Toggle'
import { R } from '../../ranks/text'

export function RankSwitches() {
  const doc = useDoc()
  const edit = useEdit()
  const [asking, setAsking] = useState<'ranks' | 'caps' | null>(null)
  const ranks = ranksOn(doc)
  const caps = doc.caps !== null
  const blocked = !ranks && !canSwitchOnRanks(doc)
  return (
    <>
      <div className="feature">
        <Toggle learn="srpg-ranks" label={R.ranks} help={blocked ? R.ranksNeedCarried : R.ranksHelp} checked={ranks} disabled={blocked}
          onChange={(on) => { if (on) edit(switchOnRanks); else if (ranksCount(doc) > 0) setAsking('ranks'); else edit(switchOffRanks) }} />
      </div>
      <div className="feature">
        <Toggle learn="srpg-caps" label={R.caps} help={R.capsHelp} checked={caps}
          onChange={(on) => { if (on) edit(switchOnCaps); else if (capsCount(doc) > 0) setAsking('caps'); else edit(switchOffCaps) }} />
      </div>
      <ConfirmDialog open={asking !== null} title={asking === 'ranks' ? R.ranksOffTitle : R.capsOffTitle} confirmLabel={R.offConfirm} danger
        onCancel={() => setAsking(null)} onConfirm={() => { edit(asking === 'ranks' ? switchOffRanks : switchOffCaps); setAsking(null) }}>
        <p>{asking === 'ranks' ? R.ranksOffBody(ranksCount(doc)) : R.capsOffBody(capsCount(doc))}</p>
      </ConfirmDialog>
    </>
  )
}
