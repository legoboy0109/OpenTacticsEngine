import { suggestedTerrainProps } from '../../model/formulas'
import { setTerrainNumber, setTerrainTags, updateTerrain } from '../../model/terrain'
import { Link, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { ChipsInput } from '../../ui/ChipsInput'
import { ColorInput } from '../../ui/ColorInput'
import { Field } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { TextInput } from '../../ui/TextInput'
import { PictureCard } from '../../ui/picture/PictureCard'
import { MoveCosts } from './MoveCosts'
import { T } from './text'

const titleCase = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)

export function TerrainForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const game = doc.game!
  const t = doc.terrain!.find((x) => x.id === id)
  if (!t) return null
  const key = `terrain:${id}`
  const suggested = suggestedTerrainProps(game)
  const labelOf = new Map(suggested.map((s) => [s.id, s.label]))
  const propRows = [...new Set([...suggested.map((s) => s.id), ...Object.keys(t.props ?? {})])].map((k) => ({ key: k, label: labelOf.get(k) ?? titleCase(k) }))
  const addable = suggested.filter((s) => t.props?.[s.id] === undefined)

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

      <PictureCard kind="terrain" id={id} name={t.name} />

      <Card><MoveCosts id={id} /></Card>

      <Card>
        <KeyNumberTable label={T.numbers} help={T.numbersHelp} rows={propRows} emptyText={T.numbersEmpty}
          columns={[{ title: T.numberCol, values: t.props, integer: false, onChange: (k, v) => edit((d) => setTerrainNumber(d, id, k, v), `${key}:prop:${k}`) }]}
          onRemove={(k) => edit((d) => setTerrainNumber(d, id, k, undefined))}
          addOptions={addable.map((s) => ({ key: s.id, label: s.label }))} onAdd={(k) => edit((d) => setTerrainNumber(d, id, k, 0))} addLabel={T.addNumber} />
      </Card>

      <p className="help" data-testid="terrain-structures-note">{T.structuresNote} <Link to={projectPath(store.id, 'structures')}>{T.structuresLink}</Link></p>

      <Advanced label={T.advanced}>
        <div className="stack">
          <Field label={T.tags} help={T.tagsHelp}>
            <ChipsInput value={t.tags ?? []} onChange={(v) => edit((d) => setTerrainTags(d, id, v))} />
          </Field>
          <Field label={T.id} help={T.idHelp}><TextInput value={t.id} readOnly mono /></Field>
        </div>
      </Advanced>
    </div>
  )
}
