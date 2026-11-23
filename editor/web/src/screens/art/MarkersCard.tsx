// "Map markers": a small picture an event puts on the squares of its reach area while it can still run (docs/GRAPHICS.md "Map markers"). Each has a name
// (the event's "Marker" picker names it), an image (whole or one piece), the point of the square it sits on and an offset.
import { BADGE_ANCHORS, BADGE_OFFSET_MAX, type BadgeAnchor } from '../../model/badges'
import { sheetsOf } from '../../model/graphics'
import { addMarker, markerList, markerProblems, patchMarker, removeMarker } from '../../model/markers'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Toggle } from '../../ui/Toggle'
import { T } from './text'

export function MarkersCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = markerList(doc)
  const sheets = sheetsOf(doc)
  return (
    <Card title={T.markersTitle} help={T.markersHelp} id="markers">
      <div className="stack tight" data-testid="markers">
        {sheets.length === 0 && <p className="help">{T.badgesNeedImage}</p>}
        {sheets.length > 0 && list.length === 0 && <p className="help">{T.markersNone}</p>}
        {list.map((m, i) => (
          <div key={i} className="battle-fields" data-testid={`marker-${i + 1}`}>
            <Field label={T.badgeName}><TextInput aria-label={T.markerNameN(i + 1)} value={m.id} onChange={(v) => edit((d) => patchMarker(d, i, { id: v }), `marker:${i}:id`)} /></Field>
            <Field label={T.badgeImage}>
              <Select aria-label={T.markerImageN(i + 1)} value={m.sheet} options={sheets.map((s) => ({ value: s.id, label: s.name || s.id }))} onChange={(v) => edit((d) => patchMarker(d, i, { sheet: v }))} />
            </Field>
            <Toggle label={T.badgeOneCell} checked={m.cell !== undefined} onChange={(on) => edit((d) => patchMarker(d, i, { cell: on ? [0, 0] : undefined }))} />
            {m.cell !== undefined && (
              <>
                <Field label={T.badgeCol}><NumberInput aria-label={T.markerColN(i + 1)} min={0} max={30000} value={m.cell[0]} onChange={(v) => v !== undefined && edit((d) => patchMarker(d, i, { cell: [v, m.cell![1]] }), `marker:${i}:col`)} /></Field>
                <Field label={T.badgeRow}><NumberInput aria-label={T.markerRowN(i + 1)} min={0} max={30000} value={m.cell[1]} onChange={(v) => v !== undefined && edit((d) => patchMarker(d, i, { cell: [m.cell![0], v] }), `marker:${i}:row`)} /></Field>
              </>
            )}
            <Field label={T.badgeAnchor}>
              <Select aria-label={T.markerAnchorN(i + 1)} value={m.anchor ?? 'center'} options={BADGE_ANCHORS.map((a) => ({ value: a, label: T.badgeAnchors[a] }))} onChange={(v) => edit((d) => patchMarker(d, i, { anchor: v as BadgeAnchor }))} />
            </Field>
            <Field label={T.badgeOffsetX}><NumberInput aria-label={T.markerOffsetXN(i + 1)} min={-BADGE_OFFSET_MAX} max={BADGE_OFFSET_MAX} value={m.offset?.[0] ?? 0} onChange={(v) => v !== undefined && edit((d) => patchMarker(d, i, { offset: [v, m.offset?.[1] ?? 0] }), `marker:${i}:ox`)} /></Field>
            <Field label={T.badgeOffsetY}><NumberInput aria-label={T.markerOffsetYN(i + 1)} min={-BADGE_OFFSET_MAX} max={BADGE_OFFSET_MAX} value={m.offset?.[1] ?? 0} onChange={(v) => v !== undefined && edit((d) => patchMarker(d, i, { offset: [m.offset?.[0] ?? 0, v] }), `marker:${i}:oy`)} /></Field>
            <button type="button" className="btn sm" aria-label={T.markerDelete(i + 1)} onClick={() => edit((d) => removeMarker(d, i))}>{T.markerDeleteWord}</button>
          </div>
        ))}
        {sheets.length > 0 && <div className="add-row"><button type="button" className="btn sm" onClick={() => edit((d) => addMarker(d))}><Icon name="plus" />{T.markerAdd}</button></div>}
        {markerProblems(doc).map((p) => <p key={p} className="error-text" role="alert">{p}</p>)}
      </div>
    </Card>
  )
}
