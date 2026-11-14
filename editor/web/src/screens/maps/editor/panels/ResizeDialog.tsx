// Resize with an anchor, and an honest list of what would be lost.
import { useState } from 'react'
import { editMap } from '../../../../model/mapCtx'
import { MAP_MAX } from '../../../../model/mapGrid'
import { ANCHORS, resizeLoss, resizeMap, type Anchor } from '../../../../model/mapResize'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Dialog } from '../../../../ui/Dialog'
import { Field, FieldGroup } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { T } from '../text'

interface Props { doc: ProjectDoc; map: MapFile; mapId: string; onClose: () => void }

const MAX_LISTED = 8

export function ResizeDialog({ doc, map, mapId, onClose }: Props) {
  const edit = useEdit()
  const [w, setW] = useState(map.grid.width)
  const [h, setH] = useState(map.grid.height)
  const [anchor, setAnchor] = useState<Anchor>('nw')
  const structureTypes = doc.structures ?? []
  const loss = resizeLoss(map, structureTypes, w, h, anchor)
  const lossCount = loss.units.length + loss.structures.length
  const changed = w !== map.grid.width || h !== map.grid.height
  const unitName = (u: MapFile['units'] extends (infer U)[] | undefined ? U : never) =>
    `${u.name ? `${u.name} (${typeName(u.type)})` : typeName(u.type)} at ${u.x + 1}, ${u.y + 1}`
  const typeName = (id: string) => doc.units?.find((x) => x.id === id)?.name ?? id
  const listed = loss.units.slice(0, MAX_LISTED).map(unitName)
  const apply = () => {
    if (changed) edit((d) => editMap(d, mapId, (m, c) => resizeMap(m, c.terrain, c.structureTypes, w, h, anchor)))
    onClose()
  }
  return (
    <Dialog open title={T.resizeTitle} onClose={onClose} onSubmit={apply}
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>Cancel</button>
          <button type="submit" className={`btn ${lossCount ? 'danger solid' : 'primary'}`} disabled={!changed}>{T.resizeApply}</button>
        </>
      )}>
      <div className="form-grid">
        <Field label={T.widthLabel}><NumberInput value={w} min={1} max={MAX_SIZE} onChange={(v) => v !== undefined && setW(v)} /></Field>
        <Field label={T.heightLabel}><NumberInput value={h} min={1} max={MAX_SIZE} onChange={(v) => v !== undefined && setH(v)} /></Field>
      </div>
      <p className="help">{T.sizeRange}</p>
      {(w > 64 || h > 64) && <p className="attention">{T.big(w, h)}</p>}
      <FieldGroup label={T.anchorLabel} help={T.anchorHelp}>
        <div className="anchor-grid" role="radiogroup" aria-label={T.anchorLabel}>
          {ANCHORS.map((a) => (
            <button key={a} type="button" role="radio" aria-checked={a === anchor} aria-label={T.anchorNames[a]} title={T.anchorNames[a]}
              className="anchor-cell" onClick={() => setAnchor(a)}>
              <span aria-hidden="true">{a === anchor ? '■' : '□'}</span>
            </button>
          ))}
        </div>
      </FieldGroup>
      {changed && (lossCount > 0
        ? (
          <div className="attention" role="alert">
            <strong>{T.cropTitle}</strong>
            <ul className="usage-list">
              {listed.map((t, i) => <li key={i}>{t}</li>)}
              {loss.units.length > MAX_LISTED && <li>and {loss.units.length - MAX_LISTED} more units</li>}
              {loss.structures.map((s, i) => <li key={`s${i}`}>{`${doc.structures?.find((d) => d.id === s.type)?.name ?? s.type} at ${s.x + 1}, ${s.y + 1}`}</li>)}
            </ul>
            <p className="help">{T.resizeUndo}</p>
          </div>
        )
        : <p className="help">{T.cropNothing}</p>)}
    </Dialog>
  )
}

const MAX_SIZE = MAP_MAX
