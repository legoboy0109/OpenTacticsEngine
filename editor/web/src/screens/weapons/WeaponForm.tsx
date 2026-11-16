import { suggestedWeaponProps } from '../../model/formulas'
import { setWeaponProp, setWeaponRange, updateWeapon } from '../../model/weapons'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { Field, FieldGroup } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { NumberInput } from '../../ui/NumberInput'
import { TextInput } from '../../ui/TextInput'
import { Check } from '../../ui/Toggle'
import { AMMO_MAX, AMMO_START, carriedAsItem, setAmmo } from '../../model/supplyUnits'
import { SoundsGroup } from '../sound/SoundsGroup'
import { Effectiveness } from './Effectiveness'
import { WeaponSrpgCards } from './srpg/WeaponSrpgCards'
import { T } from './text'

const titleCase = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)

export function WeaponForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const weapon = doc.weapons!.find((w) => w.id === id)
  if (!weapon) return null
  const key = `weapon:${id}`
  const suggested = suggestedWeaponProps(doc.game!)
  const min = weapon.range?.[0] ?? 1
  const max = weapon.range?.[1] ?? 1
  const carriedItem = carriedAsItem(doc, id)

  // Offer the numbers the chosen combat maths uses, plus whatever the weapon already has.
  const known = new Map<string, string>(suggested.map((s) => [s.id, s.label]))
  const rows = [...new Set([...suggested.map((s) => s.id), ...Object.keys(weapon.props ?? {})])].map((k) => ({ key: k, label: known.get(k) ?? titleCase(k) }))
  const addable = suggested.filter((s) => weapon.props?.[s.id] === undefined).map((s) => ({ key: s.id, label: s.label }))

  return (
    <div className="stack">
      <Card>
        <div className="stack">
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={weapon.name} onChange={(v) => edit((d) => updateWeapon(d, id, { name: v }), `${key}:name`)} />
          </Field>
          <Field label={T.group} help={T.groupHelp}>
            <TextInput value={weapon.group ?? ''} placeholder={T.groupPlaceholder}
              onChange={(v) => edit((d) => updateWeapon(d, id, { group: v.trim() === '' ? undefined : v }), `${key}:group`)} />
          </Field>
        </div>
      </Card>

      <Card>
        <div className="stack">
          <FieldGroup label={T.range} help={T.rangeHelp}>
            <FormGrid>
              <Field label={T.rangeMin}>
                <NumberInput value={min} min={0} max={99} onChange={(v) => { if (v !== undefined) edit((d) => setWeaponRange(d, id, v, max), `${key}:rmin`) }} />
              </Field>
              <Field label={T.rangeMax}>
                <NumberInput value={max} min={Math.max(1, min)} max={99} onChange={(v) => { if (v !== undefined) edit((d) => setWeaponRange(d, id, min, v), `${key}:rmax`) }} />
              </Field>
            </FormGrid>
          </FieldGroup>
          <Check label={T.counter} help={T.counterHelp} checked={weapon.counter !== false}
            onChange={(v) => edit((d) => updateWeapon(d, id, { counter: v ? undefined : false }))} />
          <Check label={T.afterMove} help={T.afterMoveHelp} checked={weapon.afterMove !== false}
            onChange={(v) => edit((d) => updateWeapon(d, id, { afterMove: v ? undefined : false }))} />
        </div>
      </Card>

      <Card>
        <KeyNumberTable label={T.numbers} rows={rows} emptyText={T.numbersEmpty}
          help={suggested.length ? `${T.numbersHelp} ${T.suggested(suggested.map((s) => s.label).join(' and '))}` : T.numbersHelp}
          columns={[{ title: T.numberCol, values: weapon.props, integer: false, onChange: (k, v) => edit((d) => setWeaponProp(d, id, k, v), `${key}:prop:${k}`) }]}
          onRemove={(k) => edit((d) => setWeaponProp(d, id, k, undefined))}
          addOptions={addable} onAdd={(k) => edit((d) => setWeaponProp(d, id, k, suggested.find((s) => s.id === k)?.default ?? 0))} addLabel={T.addNumber} />
      </Card>

      <Card>
        <div className="stack">
          <Check label={T.ammo} help={carriedItem && weapon.ammo === undefined ? T.ammoCarried(carriedItem) : T.ammoHelp} checked={weapon.ammo !== undefined}
            disabled={!!carriedItem && weapon.ammo === undefined} onChange={(on) => edit((d) => setAmmo(d, id, on ? AMMO_START : undefined))} />
          {weapon.ammo !== undefined && (
            <Field label={T.ammoAmount} help={T.ammoAmountHelp}>
              <NumberInput value={weapon.ammo} min={1} max={AMMO_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setAmmo(d, id, v), `${key}:ammo`) }} />
            </Field>
          )}
        </div>
      </Card>

      <Card>
        <Effectiveness id={id} />
      </Card>

      <WeaponSrpgCards id={id} />

      <SoundsGroup owner="weapons" id={id} />

      <Advanced label={T.advanced}>
        <div className="stack">
          <Field label={T.override} help={T.overrideHelp}>
            <TextInput multiline mono rows={2} value={weapon.damage ?? ''}
              onChange={(v) => edit((d) => updateWeapon(d, id, { damage: v.trim() === '' ? undefined : v }), `${key}:damage`)} />
          </Field>
          <Field label={T.id} help={T.idHelp}><TextInput value={weapon.id} readOnly mono /></Field>
        </div>
      </Advanced>
    </div>
  )
}
