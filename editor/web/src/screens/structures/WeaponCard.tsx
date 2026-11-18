// "A weapon to use here" on a structure kind (docs/SRPG.md "Map weapons"): which weapon, how many uses, who may use it, what it becomes when spent.
// What the loader refuses is shown under the card's title (model/mapWeapon.ts).
import { useMemo } from 'react'
import {
  byTypeIds, hasWeapon, mapWeaponChecks, setWeapon, setWeaponBecomes, setWeaponBy, setWeaponIgnoreRank, setWeaponUses, starterWeapon, unitTags, weaponItems, weaponTwins,
} from '../../model/mapWeapon'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { W } from './weaponText'

export function WeaponCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const s = doc.structures?.find((x) => x.id === id)
  const checks = useMemo(() => mapWeaponChecks(doc).filter((c) => c.section === 'structures' && c.kind === id), [doc, id])
  if (!s) return null
  const key = `structure:${id}`
  const items = weaponItems(doc)
  const on = hasWeapon(s)
  const first = starterWeapon(doc)
  const listed = on && !items.some((i) => i.id === s.weapon) ? [{ value: String(s.weapon), label: `${s.weapon} (not a weapon item: pick another)` }] : []
  const options = [...listed, ...items.map((i) => ({ value: i.id, label: i.name }))]
  const unitOptions = (doc.units ?? []).map((u) => ({ value: u.id, label: u.name }))
  const tagOptions = unitTags(doc).map((t) => ({ value: t, label: t }))
  const by = s.by ?? {}
  const twins = weaponTwins(doc, s)
  const becomes = [{ value: '', label: W.becomesStays }, ...twins.map((t) => ({ value: t.id, label: W.becomesTo(t.name) }))]
  if (s.becomes && !twins.some((t) => t.id === s.becomes)) becomes.push({ value: s.becomes, label: `${s.becomes} (not usable: pick another)` })
  const stands = byTypeIds(doc, s)
  return (
    <Card title={W.title} help={W.help} id={`${key}:weapon`}>
      <div className="stack" data-testid="weapon-card">
        {checks.length > 0 && (
          <Banner tone="warn" title={W.checksTitle}>
            <ul className="usage-list" data-testid="weapon-checks">{checks.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
          </Banner>
        )}
        <Check label={W.on} help={items.length === 0 && !on ? W.noWeaponItems : W.onHelp} checked={on} disabled={!on && !first}
          onChange={(v) => edit((d) => setWeapon(d, id, v ? first : undefined))} />
        {on && (
          <div className="indent stack">
            <Field label={W.weapon} help={W.weaponHelp}>
              <Select value={String(s.weapon)} options={options} onChange={(v) => edit((d) => setWeapon(d, id, v))} />
            </Field>
            <Field label={W.usesLabel} help={W.usesHelp}>
              <NumberInput value={s.uses} min={1} max={9999} allowEmpty placeholder={W.usesPlaceholder}
                onChange={(v) => edit((d) => setWeaponUses(d, id, v), `${key}:uses`)} />
            </Field>
            <MultiSelect label={W.byTypes} help={W.byHelp} emptyText={W.byNoTypes} options={unitOptions} value={by.types ?? []}
              onChange={(v) => edit((d) => setWeaponBy(d, id, v, by.tags ?? []))} />
            {tagOptions.length > 0 && (
              <MultiSelect label={W.byTags} help={W.byTagsHelp} emptyText={W.byNoTags} options={tagOptions} value={by.tags ?? []}
                onChange={(v) => edit((d) => setWeaponBy(d, id, by.types ?? [], v))} />
            )}
            {stands && stands.length > 0 && <p className="help" data-testid="weapon-who">{stands.length === 1 ? '1 unit type may use it.' : `${stands.length} unit types may use it.`}</p>}
            <Check label={W.ignoreRank} help={W.ignoreRankHelp} checked={s.ignoreRank === true} onChange={(v) => edit((d) => setWeaponIgnoreRank(d, id, v))} />
            <Field label={W.becomes} help={W.becomesHelp}>
              <Select value={s.becomes ?? ''} options={becomes} onChange={(v) => edit((d) => setWeaponBecomes(d, id, v))} />
            </Field>
          </div>
        )}
      </div>
    </Card>
  )
}
