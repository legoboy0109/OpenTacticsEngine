// A unit type's sight (docs/ECONOMY.md "Fog of war"): shown always, since fog is switched on per map.
import { setVision, VISION_DEFAULT, VISION_MAX, visionOf } from '../../model/fog'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { T } from './text'

export function SightCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.units?.find((u) => u.id === id)
  if (!unit) return null
  return (
    <Card title={T.unitTitle} help={T.unitHelp}>
      <Field label={T.unitSight} help={T.unitSightHelp(VISION_DEFAULT)}>
        <NumberInput value={visionOf(unit)} min={0} max={VISION_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setVision(d, id, v), `unit:${id}:vision`) }} />
      </Field>
    </Card>
  )
}
