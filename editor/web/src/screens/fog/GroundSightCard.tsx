// "Extra sight" and "hides" on a terrain or structure kind (docs/ECONOMY.md "Fog of war"), shown always.
import { BONUS_MAX, setHides, setVisionBonus, type Ground } from '../../model/fog'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export function GroundSightCard({ kind, id }: { kind: Ground; id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const g = (kind === 'terrain' ? doc.terrain : doc.structures)?.find((x) => x.id === id)
  if (!g) return null
  return (
    <Card title={T.groundTitle} help={T.groundHelp}>
      <div className="stack">
        <Field label={T.bonus} help={T.bonusHelp}>
          <NumberInput value={g.visionBonus ?? 0} min={0} max={BONUS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setVisionBonus(d, kind, id, v), `${kind}:${id}:visionBonus`) }} />
        </Field>
        <Check label={T.hides} help={T.hidesHelp} checked={g.hides === true} onChange={(on) => edit((d) => setHides(d, kind, id, on))} />
      </div>
    </Card>
  )
}
