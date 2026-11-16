// The "Stat caps of this class" card (docs/SRPG.md section 7): a cap per stat that replaces the game's for this class. Empty boxes
// use the game's cap, shown as the placeholder. Shown once the game has stat caps (or the class already has some).
import { gameCap, setCapIn } from '../../../model/ranks'
import { updateUnit } from '../../../model/units'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { CapRows } from '../../ranks/CapRows'
import { R } from '../../ranks/text'

export function UnitCapsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.units?.find((u) => u.id === id)
  if (!unit || (doc.caps === null && !unit.caps)) return null
  return (
    <Card title={R.unitCapsTitle} help={R.unitCapsHelp} id="unit-caps">
      <CapRows testId="unit-caps" mode="cap" values={unit.caps} fallback={(s) => gameCap(doc, s)}
        onChange={(s, v) => edit((d) => updateUnit(d, id, { caps: setCapIn(d.units?.find((u) => u.id === id)?.caps, s, v) }), `unit:${id}:cap:${s}`)} />
    </Card>
  )
}
