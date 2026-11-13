import { suggestedTerrainProps } from '../../model/formulas'
import { featureEnabled } from '../../model/rules'
import { setCapturePoints, setHeadquarters, setProduces, setTerrainNumber, setTerrainTags, updateTerrain } from '../../model/terrain'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { ChipsInput } from '../../ui/ChipsInput'
import { ColorInput } from '../../ui/ColorInput'
import { Field } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { TextInput } from '../../ui/TextInput'
import { Check } from '../../ui/Toggle'
import { MoveCosts } from './MoveCosts'
import { T } from './text'

const titleCase = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)

export function TerrainForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const game = doc.game!
  const t = doc.terrain!.find((x) => x.id === id)
  if (!t) return null
  const key = `terrain:${id}`
  const suggested = suggestedTerrainProps(game)
  const labelOf = new Map(suggested.map((s) => [s.id, s.label]))
  const propRows = [...new Set([...suggested.map((s) => s.id), ...Object.keys(t.props ?? {})])].map((k) => ({ key: k, label: labelOf.get(k) ?? titleCase(k) }))
  const addable = suggested.filter((s) => t.props?.[s.id] === undefined)
  const resources = game.resources ?? []
  const capturable = (t.capture ?? 0) > 0
  const isHq = t.tags?.includes('hq') === true
  const otherTags = (t.tags ?? []).filter((x) => x !== 'hq')

  return (
    <div className="stack">
      <Card>
        <FormGrid>
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={t.name} onChange={(v) => edit((d) => updateTerrain(d, id, { name: v }), `${key}:name`)} />
          </Field>
          <Field label={T.glyph} help={T.glyphHelp}>
            <TextInput value={t.glyph ?? ''} maxLength={1} className="glyph-input"
              onChange={(v) => edit((d) => updateTerrain(d, id, { glyph: Array.from(v)[0] }), `${key}:glyph`)} />
          </Field>
          <Field label={T.color} help={T.colorHelp}>
            <ColorInput value={t.color} aria-label={`${t.name} ${T.color.toLowerCase()}`} onChange={(v) => edit((d) => updateTerrain(d, id, { color: v }), `${key}:color`)} />
          </Field>
        </FormGrid>
      </Card>

      <Card><MoveCosts id={id} /></Card>

      <Card>
        <KeyNumberTable label={T.numbers} help={T.numbersHelp} rows={propRows} emptyText={T.numbersEmpty}
          columns={[{ title: T.numberCol, values: t.props, integer: false, onChange: (k, v) => edit((d) => setTerrainNumber(d, id, 'props', k, v), `${key}:prop:${k}`) }]}
          onRemove={(k) => edit((d) => setTerrainNumber(d, id, 'props', k, undefined))}
          addOptions={addable.map((s) => ({ key: s.id, label: s.label }))} onAdd={(k) => edit((d) => setTerrainNumber(d, id, 'props', k, 0))} addLabel={T.addNumber} />
      </Card>

      <Card>
        <div className="stack">
          <Check label={T.capture} help={T.captureHelp} checked={capturable} onChange={(on) => edit((d) => setCapturePoints(d, id, on ? 20 : undefined))} />
          {capturable && (
            <Field label={T.capturePoints} help={T.capturePointsHelp}>
              <NumberInput value={t.capture} min={1} onChange={(v) => { if (v !== undefined) edit((d) => setCapturePoints(d, id, v), `${key}:capture`) }} />
            </Field>
          )}
          {(capturable || isHq) && !featureEnabled(doc, 'capture') && <p className="help">{T.captureOff}</p>}
          <Check label={T.hq} help={T.hqHelp} checked={isHq} onChange={(on) => edit((d) => setHeadquarters(d, id, on))} />
        </div>
      </Card>

      <Card>
        <div className="stack">
          {resources.length === 0
            ? <p className="help">{T.incomeNone}</p>
            : (
              <KeyNumberTable label={T.income} help={T.incomeHelp} rows={resources.map((r) => ({ key: r.id, label: r.name }))}
                columns={[{ title: T.incomeCol, values: t.income, min: 0, placeholder: '0', onChange: (k, v) => edit((d) => setTerrainNumber(d, id, 'income', k, v), `${key}:income:${k}`) }]} />
            )}
          {resources.length > 0 && Object.keys(t.income ?? {}).length > 0 && !featureEnabled(doc, 'economy') && <p className="help">{T.incomeOff}</p>}
        </div>
      </Card>

      <Card>
        <MultiSelect label={T.produces} help={T.producesHelp} value={t.produces ?? []} options={(doc.units ?? []).map((u) => ({ value: u.id, label: u.name }))}
          onChange={(v) => edit((d) => setProduces(d, id, v))} />
        {(t.produces?.length ?? 0) > 0 && !featureEnabled(doc, 'production') && <p className="help">{T.producesOff}</p>}
      </Card>

      <Advanced label={T.advanced}>
        <div className="stack">
          <Field label={T.tags} help={T.tagsHelp}>
            <ChipsInput value={otherTags} onChange={(v) => edit((d) => setTerrainTags(d, id, [...(isHq ? ['hq'] : []), ...v]))} />
          </Field>
          <Field label={T.id} help={T.idHelp}><TextInput value={t.id} readOnly mono /></Field>
        </div>
      </Advanced>
    </div>
  )
}
