import { carriedWeapons, inventorySize } from '../../model/itemRules'
import { featureEnabled } from '../../model/rules'
import { allTags, setUnitItems, setUnitNumber, setUnitTags, setUnitWeapons, setUnitWields, updateUnit, weaponGroups } from '../../model/units'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { ChipsInput } from '../../ui/ChipsInput'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { MultiSelect } from '../../ui/MultiSelect'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { PictureCard } from '../../ui/picture/PictureCard'
import { ItemListEditor } from '../../ui/ItemListEditor'
import { Check } from '../../ui/Toggle'
import { T } from './text'

export function UnitForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const game = doc.game!
  const unit = doc.units!.find((u) => u.id === id)
  if (!unit) return null
  const key = `unit:${id}`
  const growth = featureEnabled(doc, 'progression')
  const production = featureEnabled(doc, 'production') && (game.resources ?? []).length > 0
  const carried = carriedWeapons(doc)
  const items = doc.items ?? []
  const groups = [...new Set([...weaponGroups(doc), ...(unit.wields ?? [])])]
  const statRows = game.stats.map((s) => ({ key: s.id, label: s.name }))

  return (
    <div className="stack">
      <Card>
        <FormGrid>
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={unit.name} onChange={(v) => edit((d) => updateUnit(d, id, { name: v }), `${key}:name`)} />
          </Field>
          <Field label={T.glyph} help={T.glyphHelp}>
            <TextInput value={unit.glyph ?? ''} maxLength={1} className="glyph-input"
              onChange={(v) => edit((d) => updateUnit(d, id, { glyph: Array.from(v)[0] }), `${key}:glyph`)} />
          </Field>
          <Field label={T.moveType} help={T.moveTypeHelp}>
            <Select value={unit.moveType} options={game.moveTypes.map((m) => ({ value: m.id, label: m.name }))}
              onChange={(v) => edit((d) => updateUnit(d, id, { moveType: v }))} />
          </Field>
        </FormGrid>
      </Card>

      <PictureCard kind="units" id={id} name={unit.name} />

      <Card>
        <KeyNumberTable label={T.stats} help={growth ? `${T.statsHelp} ${T.growthHelp}` : T.statsHelp} rows={statRows} emptyText={T.noStats}
          columns={[
            { title: T.statCol, values: unit.stats, min: 0, placeholder: '0', onChange: (k, v) => edit((d) => setUnitNumber(d, id, 'stats', k, v), `${key}:stat:${k}`) },
            ...(growth ? [{ title: T.growthCol, values: unit.growths, min: 0, placeholder: '0', suffix: '%', onChange: (k: string, v: number | undefined) => edit((d) => setUnitNumber(d, id, 'growths', k, v), `${key}:growth:${k}`) }] : []),
          ]} />
      </Card>

      <Card>
        <div className="stack">
          <MultiSelect label={carried ? T.weaponsCarried : T.weapons} help={carried ? T.weaponsCarriedHelp : T.weaponsHelp} emptyText={T.noWeapons} value={unit.weapons ?? []}
            options={(doc.weapons ?? []).map((w) => ({ value: w.id, label: w.name }))}
            onChange={(v) => edit((d) => setUnitWeapons(d, id, v))} />
          {(carried || (unit.wields?.length ?? 0) > 0) && (
            <MultiSelect label={T.wields} help={carried ? T.wieldsHelp : T.wieldsOff} emptyText={T.noGroups} value={unit.wields ?? []}
              options={groups.map((g) => ({ value: g, label: g }))} onChange={(v) => edit((d) => setUnitWields(d, id, v))} />
          )}
        </div>
      </Card>

      {(items.length > 0 || (unit.items?.length ?? 0) > 0) && (
        <Card>
          <ItemListEditor label={T.startItems} help={T.startItemsHelp} entries={unit.items ?? []} items={items} allowDrop max={inventorySize(doc)}
            testId="unit-start-items" onChange={(next, k) => edit((d) => setUnitItems(d, id, next), k && `${key}:${k}`)} />
        </Card>
      )}

      {(production || featureEnabled(doc, 'capture')) && (
        <Card>
          <div className="stack">
            {production && (
              <KeyNumberTable label={T.cost} help={T.costHelp} rows={(game.resources ?? []).map((r) => ({ key: r.id, label: r.name }))}
                columns={[{ title: T.costCol, values: unit.cost, min: 0, placeholder: '0', onChange: (k, v) => edit((d) => setUnitNumber(d, id, 'cost', k, v), `${key}:cost:${k}`) }]} />
            )}
            {featureEnabled(doc, 'capture') && (
              <Check label={T.canCapture} help={T.canCaptureHelp} checked={unit.canCapture === true}
                onChange={(v) => edit((d) => updateUnit(d, id, { canCapture: v ? true : undefined }))} />
            )}
          </div>
        </Card>
      )}

      <Card>
        <Field label={T.tags} help={T.tagsHelp}>
          <ChipsInput value={unit.tags ?? []} suggestions={allTags(doc)} onChange={(v) => edit((d) => setUnitTags(d, id, v))} />
        </Field>
      </Card>

      <Advanced label={T.advanced}>
        <Field label={T.id} help={T.idHelp}><TextInput value={unit.id} readOnly mono /></Field>
      </Advanced>
    </div>
  )
}
