// The map settings' backdrop part (docs/VIEWS.md "Backdrop"): the image, where square 0,0 sits in it, the over image, how many pieces
// are cut from the over image (read only: converters and tools write them) and which 2D isometric styles draw it. Only for square games
// with such a style; a map that already has a backdrop always shows it so it can be fixed or removed. Checks: model/backdropChecks.ts.
import { artBase } from '../../../../model/artFiles'
import { addBackdrop, backdropAllowed, isometricStyles, pieceCount, removeBackdrop, setBackdropOrigin, setBackdropOver, setBackdropSheet, setBackdropStyles } from '../../../../model/backdrop'
import { sheetsOf } from '../../../../model/graphics'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { Check } from '../../../../ui/Toggle'
import { BD } from './backdropText'

export function BackdropSettings({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const b = map.backdrop
  const sheets = sheetsOf(doc)
  if (!b && !backdropAllowed(doc)) return null
  const sheetOptions = sheets.map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))
  const iso = isometricStyles(doc)
  const chosen = Array.isArray(b?.styles) ? b!.styles! : []
  const origin = Array.isArray(b?.origin) ? b!.origin! : [0, 0]
  const pieces = pieceCount(b)
  return (
    <fieldset className="fieldset" data-testid="map-backdrop">
      <legend>{BD.title}</legend>
      <p className="help">{BD.help}</p>
      {!b ? (
        sheets.length === 0
          ? <p className="help">{BD.needImage}</p>
          : <div><button type="button" className="btn sm" onClick={() => edit((d) => addBackdrop(d, mapId, sheets[0].id))}>{BD.add}</button></div>
      ) : (
        <div className="stack tight">
          <Field label={BD.image} help={BD.imageHelp}>
            <Select aria-label={BD.image} value={typeof b.sheet === 'string' ? b.sheet : ''} placeholder="Choose an image" options={sheetOptions} onChange={(v) => edit((d) => setBackdropSheet(d, mapId, v))} />
          </Field>
          <div className="form-grid">
            <Field label={BD.originX} help={BD.originHelp}>
              <NumberInput aria-label={BD.originX} min={-8192} max={8192} value={origin[0]} onChange={(v) => v !== undefined && edit((d) => setBackdropOrigin(d, mapId, v, origin[1]), `backdrop:${mapId}:ox`)} />
            </Field>
            <Field label={BD.originY}>
              <NumberInput aria-label={BD.originY} min={-8192} max={8192} value={origin[1]} onChange={(v) => v !== undefined && edit((d) => setBackdropOrigin(d, mapId, origin[0], v), `backdrop:${mapId}:oy`)} />
            </Field>
          </div>
          <Field label={BD.over} help={BD.overHelp}>
            <Select aria-label={BD.over} value={typeof b.over === 'string' ? b.over : ''} options={[{ value: '', label: BD.overNone }, ...sheetOptions]} onChange={(v) => edit((d) => setBackdropOver(d, mapId, v))} />
          </Field>
          <p className="help" data-testid="backdrop-pieces">{BD.pieces(pieces)} {BD.piecesNote}</p>
          {iso.length > 0 && (
            <fieldset className="fieldset">
              <legend>{BD.styles}</legend>
              <p className="help">{chosen.length === 0 ? BD.allStyles : BD.stylesHelp}</p>
              {iso.map((s) => (
                <Check key={s.id} label={s.name || s.id} checked={chosen.includes(s.id)}
                  onChange={(on) => edit((d) => setBackdropStyles(d, mapId, on ? [...chosen, s.id] : chosen.filter((x) => x !== s.id)))} />
              ))}
            </fieldset>
          )}
          <div><button type="button" className="btn sm" onClick={() => edit((d) => removeBackdrop(d, mapId))}>{BD.remove}</button></div>
        </div>
      )}
    </fieldset>
  )
}
