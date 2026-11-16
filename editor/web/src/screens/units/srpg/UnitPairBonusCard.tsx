// The "Gives when behind" card of a unit type, for pair-up games (E3 of docs/SRPG.md). Edits `pairBonus` of the unit type, which replaces
// the game-wide numbers of the Rules screen as a whole. Shown when the game is set to Pair up, or when the unit type already has numbers
// (so they can be seen and cleared even if the mode was changed in the file).
import { changeableStats, rescueMode } from '../../../model/rescue'
import { updateUnit } from '../../../model/units'
import { dropKey } from '../../../model/util'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { KeyNumberTable } from '../../../ui/KeyNumberTable'
import { P } from './promotionText'

export function UnitPairBonusCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.units?.find((u) => u.id === id)
  const pairUp = rescueMode(doc) === 'pairUp'
  if (!unit || (!pairUp && !unit.pairBonus)) return null
  const bonus = unit.pairBonus ?? {}
  const change = (stat: string, v: number | undefined) =>
    edit((d) => updateUnit(d, id, { pairBonus: v === undefined ? dropKey(bonus, stat) : { ...bonus, [stat]: v } }), `unit:${id}:pairBonus:${stat}`)
  return (
    <Card title={P.pairTitle} help={P.pairHelp} id="unit-pair-bonus">
      <div className="stack" data-testid="unit-pair-bonus">
        {!pairUp && <Banner tone="warn">{P.pairNeedsPairUp}</Banner>}
        <KeyNumberTable label={P.pairTitle} rows={changeableStats(doc).map((s) => ({ key: s.id, label: s.name }))}
          columns={[{ title: P.pairCol, values: bonus, placeholder: '0', onChange: change }]} />
      </div>
    </Card>
  )
}
