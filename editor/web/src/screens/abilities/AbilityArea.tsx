// "Area" card of an ability (heal, cure, heal + cure, or only a condition): aim at a square and act on everyone in the shape around it.
import { AREA_MAX, AREA_SHAPES, VERTICAL_MAX, hasRadius, areaOf, setAreaShape, setAreaSize, setAreaVertical, type AreaShape } from '../../model/abilityArea'
import { gameHasHeight } from '../../model/abilityAreaChecks'
import { setArea, targetOf } from '../../model/abilities'
import { useDoc } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import type { FormProps } from './AbilityEffectFields'
import { T } from './text'

export function AbilityArea({ ability, change }: FormProps) {
  const doc = useDoc()
  const area = areaOf(ability)
  return (
    <Card title={T.areaCard} help={T.areaHelp}>
      <div className="stack" data-testid="ability-area">
        <Check label={T.areaOn} help={T.areaOnHelp} checked={area !== undefined} onChange={(v) => change((a) => setArea(a, v))} />
        {area && (
          <>
            <FormGrid>
              <Field label={T.areaShape}>
                <Select value={area.shape} placeholder="—" options={AREA_SHAPES.map((s) => ({ value: s, label: T.areaShapes[s] }))} onChange={(v) => change((a) => setAreaShape(a, v as AreaShape))} />
              </Field>
              {(hasRadius(area.shape) || area.shape === 'line') && (
                <Field label={area.shape === 'line' ? T.areaLength : T.areaRadius} help={T.areaSizeHelp}>
                  <NumberInput value={area.size} min={1} max={AREA_MAX} onChange={(v) => { if (v !== undefined) change((a) => setAreaSize(a, v), 'areaSize') }} />
                </Field>
              )}
              {area.shape !== 'field' && (gameHasHeight(doc) || area.vertical !== undefined) && (
                <Field label={T.areaVertical} help={T.areaVerticalHelp}>
                  <NumberInput value={area.vertical} min={0} max={VERTICAL_MAX} allowEmpty placeholder="no limit" onChange={(v) => change((a) => setAreaVertical(a, v), 'areaVertical')} />
                </Field>
              )}
            </FormGrid>
            <p className="help" data-testid="ability-area-who">{T.areaWho[targetOf(ability)] ?? ''}</p>
          </>
        )}
      </div>
    </Card>
  )
}
