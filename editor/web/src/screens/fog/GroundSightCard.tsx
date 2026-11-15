// "Extra sight" and "hides" on a terrain or structure kind (docs/ECONOMY.md "Fog of war"), shown always. The extra sight is
// for every unit (a plain number in the file) or only for some unit types and labels (an object, the same picking as a
// structure's Repairs).
import { BONUS_MAX, bonusAmount, bonusUnits, setHides, setVisionBonus, setVisionUnits, type Ground } from '../../model/fog'
import { unitKeyOptions } from '../../model/unitKeys'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export function GroundSightCard({ kind, id }: { kind: Ground; id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const g = (kind === 'terrain' ? doc.terrain : doc.structures)?.find((x) => x.id === id)
  if (!g) return null
  const only = bonusUnits(g)
  const list = only ?? []
  return (
    <Card title={T.groundTitle} help={T.groundHelp}>
      <div className="stack">
        <Field label={T.bonus} help={T.bonusHelp}>
          <NumberInput value={bonusAmount(g)} min={0} max={BONUS_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setVisionBonus(d, kind, id, v), `${kind}:${id}:visionBonus`) }} />
        </Field>
        <Field label={T.bonusFor} help={T.bonusForHelp}>
          <Select value={only === undefined ? 'all' : 'only'} options={[{ value: 'all', label: T.forAll }, { value: 'only', label: T.forOnly }]}
            onChange={(v) => edit((d) => setVisionUnits(d, kind, id, v === 'all' ? undefined : []))} />
        </Field>
        {only !== undefined && (
          <>
            <MultiSelect label={T.bonusPick} value={list} options={unitKeyOptions(doc, list, T.tagLabel, T.missing)} onChange={(v) => edit((d) => setVisionUnits(d, kind, id, v))} />
            {list.length === 0 && <p className="help" data-testid="sight-nobody">{T.bonusNobody}</p>}
          </>
        )}
        <Check label={T.hides} help={T.hidesHelp} checked={g.hides === true} onChange={(on) => edit((d) => setHides(d, kind, id, on))} />
      </div>
    </Card>
  )
}
