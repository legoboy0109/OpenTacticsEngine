// "Kind of view" of one visual style (docs/VIEWS.md) and, for Isometric, its sizes in a collapsed Advanced area.
import { updateStyleWith } from '../../model/game'
import { gridOfGame, isHex } from '../../model/gridSpec'
import { useHexViews } from '../../api/hexViews'
import { allowedKinds, isIsometric, isoDefaults, isoKeysFor, ISO_RANGE, viewKind, withIsoSize, withView, type EditableKind, type IsoKey, type ViewKind } from '../../model/styleView'
import type { Style } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { T } from './text'

const SIZE_FIELDS: Array<{ key: IsoKey; label: string; help: string; hexLabel?: string; hexHelp?: string }> = [
  { key: 'tileWidth', label: T.isoTileWidth, help: T.isoTileWidthHelp, hexLabel: T.isoTileWidthHex, hexHelp: T.isoTileWidthHexHelp },
  { key: 'tileHeight', label: T.isoTileHeight, help: T.isoTileHeightHelp },
  { key: 'heightStep', label: T.isoHeightStep, help: T.isoHeightStepHelp, hexHelp: T.isoHeightStepHexHelp },
  { key: 'unitWidth', label: T.isoUnitWidth, help: T.isoUnitWidthHelp },
]

const KIND_LABEL: Record<ViewKind, string> = { 'top-down': T.viewTopDown, isometric: T.viewIsometric, 'isometric-3d': T.viewIsometric3d, '3d': T.viewFull3d }

export function ViewFields({ style }: { style: Style }) {
  const doc = useDoc()
  const edit = useEdit()
  const kind = viewKind(style)
  const key = `style:${style.id}`
  const hex = isHex(gridOfGame(doc.game))
  const defaults = isoDefaults(doc.game, style.isometric, hex)
  // A hexagon game is offered only the kinds of view the engine draws for hexagons (the helper says which); a style
  // that somehow has another kind keeps it in the list, greyed, with the reason.
  const allowed = allowedKinds(hex, useHexViews())
  const offered = (['top-down', 'isometric', 'isometric-3d', '3d'] as ViewKind[]).filter((k) => allowed.includes(k) || k === kind)
  const choice = (k: ViewKind) => ({ value: k, label: allowed.includes(k) ? KIND_LABEL[k] : `${KIND_LABEL[k]} (${T.viewNotHex})`, disabled: !allowed.includes(k) })
  const hexHelp = !hex || allowed.length === 4 ? null : allowed.includes('isometric') ? T.viewHexIsoHelp : T.viewHexHelp
  const sizes = isoKeysFor(hex)
  return (
    <>
      <Field label={T.view} help={T.viewHelp}>
        <Select value={kind} aria-label={`${style.name}: ${T.view}`} options={offered.map(choice)}
          onChange={(v) => edit((d) => updateStyleWith(d, style.id, (s) => withView(s, v as EditableKind)))} />
      </Field>
      {hexHelp && <p className="help" data-testid="view-hex-help">{hexHelp}</p>}
      {kind === '3d' && <p className="help" data-testid="view-full3d-help">{T.viewFull3dHelp}</p>}
      {isIsometric(kind) && (
        <>
          <p className="help">{kind === 'isometric-3d' ? T.viewOtherPictures3d : T.viewOtherPictures}</p>
          <Advanced label={T.advanced}>
            <div className="stack">
              <p className="help">{T.isoSizesHelp}</p>
              {hex && <p className="help" data-testid="view-hex-sizes-help">{T.viewHexIsoSizesHelp}</p>}
              <FormGrid cols={2}>
                {SIZE_FIELDS.filter((f) => sizes.includes(f.key)).map((f) => (
                  <Field key={f.key} label={hex && f.hexLabel ? f.hexLabel : f.label} help={hex && f.hexHelp ? f.hexHelp : f.help}>
                    <NumberInput value={style.isometric?.[f.key]} min={ISO_RANGE[f.key].min} max={ISO_RANGE[f.key].max} even={ISO_RANGE[f.key].even}
                      allowEmpty placeholder={String(defaults[f.key])} aria-label={`${style.name}: ${hex && f.hexLabel ? f.hexLabel : f.label}`}
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
