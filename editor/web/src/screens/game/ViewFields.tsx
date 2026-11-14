// "Kind of view" of one visual style (docs/VIEWS.md) and, for Isometric, its sizes in a collapsed Advanced area.
import { updateStyleWith } from '../../model/game'
import { isIsometric, isoDefaults, ISO_RANGE, viewKind, withIsoSize, withView, type EditableKind, type IsoKey } from '../../model/styleView'
import type { Style } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { T } from './text'

const SIZE_FIELDS: Array<{ key: IsoKey; label: string; help: string }> = [
  { key: 'tileWidth', label: T.isoTileWidth, help: T.isoTileWidthHelp },
  { key: 'tileHeight', label: T.isoTileHeight, help: T.isoTileHeightHelp },
  { key: 'heightStep', label: T.isoHeightStep, help: T.isoHeightStepHelp },
  { key: 'unitWidth', label: T.isoUnitWidth, help: T.isoUnitWidthHelp },
]

export function ViewFields({ style }: { style: Style }) {
  const doc = useDoc()
  const edit = useEdit()
  const kind = viewKind(style)
  const key = `style:${style.id}`
  // "3d" is kept exactly as the file has it: no control, so nothing can change it here.
  if (kind === 'future') return <p className="help" data-testid="view-future">{T.viewFuture}</p>
  const defaults = isoDefaults(doc.game, style.isometric)
  return (
    <>
      <Field label={T.view} help={T.viewHelp}>
        <Select value={kind} aria-label={`${style.name}: ${T.view}`}
          options={[{ value: 'top-down', label: T.viewTopDown }, { value: 'isometric', label: T.viewIsometric }, { value: 'isometric-3d', label: T.viewIsometric3d }]}
          onChange={(v) => edit((d) => updateStyleWith(d, style.id, (s) => withView(s, v as EditableKind)))} />
      </Field>
      {isIsometric(kind) && (
        <>
          <p className="help">{kind === 'isometric-3d' ? T.viewOtherPictures3d : T.viewOtherPictures}</p>
          <Advanced label={T.advanced}>
            <div className="stack">
              <p className="help">{T.isoSizesHelp}</p>
              <FormGrid cols={2}>
                {SIZE_FIELDS.map((f) => (
                  <Field key={f.key} label={f.label} help={f.help}>
                    <NumberInput value={style.isometric?.[f.key]} min={ISO_RANGE[f.key].min} max={ISO_RANGE[f.key].max} even={ISO_RANGE[f.key].even}
                      allowEmpty placeholder={String(defaults[f.key])} aria-label={`${style.name}: ${f.label}`}
                      onChange={(v) => edit((d) => updateStyleWith(d, style.id, (s) => withIsoSize(s, f.key, v)), `${key}:iso:${f.key}`)} />
                  </Field>
                ))}
              </FormGrid>
            </div>
          </Advanced>
        </>
      )}
    </>
  )
}
