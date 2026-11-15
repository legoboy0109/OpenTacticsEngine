// "Kind of view" of one visual style (docs/VIEWS.md) and, for Isometric, its sizes in a collapsed Advanced area.
import { updateStyleWith } from '../../model/game'
import { gridOfGame, isHex } from '../../model/gridSpec'
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
  const defaults = isoDefaults(doc.game, style.isometric)
  // On a hexagon game only the top-down kind of view can be chosen; the others stay in the list, greyed, with the reason.
  const hex = isHex(gridOfGame(doc.game))
  const choice = (value: string, label: string) => ({ value, label: hex && value !== 'top-down' ? `${label} (${T.viewNotHex})` : label, disabled: hex && value !== 'top-down' && value !== kind })
  return (
    <>
      <Field label={T.view} help={T.viewHelp}>
        <Select value={kind} aria-label={`${style.name}: ${T.view}`}
          options={[choice('top-down', T.viewTopDown), choice('isometric', T.viewIsometric), choice('isometric-3d', T.viewIsometric3d), choice('3d', T.viewFull3d)]}
          onChange={(v) => edit((d) => updateStyleWith(d, style.id, (s) => withView(s, v as EditableKind)))} />
      </Field>
      {hex && <p className="help" data-testid="view-hex-help">{T.viewHexHelp}</p>}
      {kind === '3d' && <p className="help" data-testid="view-full3d-help">{T.viewFull3dHelp}</p>}
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
