// The Slots tool's panel (docs/EVENTS.md "Preparation"): what clicking does, how many slots there are, a way to switch the
// preparation on when the map has none.
import { editMap } from '../../../../model/mapCtx'
import { enablePrep, removeSlot } from '../../../../model/prep'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Icon } from '../../../../ui/Icon'
import { P } from './prepText'

export function SlotsTool({ doc: _doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const prep = map.prep
  return (
    <div className="stack tight" data-testid="slots-tool">
      <h3>{P.toolTitle}</h3>
      <p className="help">{P.toolHelp}</p>
      {!prep && (
        <>
          <p className="help">{P.toolOff}</p>
          <button type="button" className="btn primary sm" onClick={() => edit((d) => editMap(d, mapId, (m, c) => enablePrep(m, c)))}>{P.toolOn}</button>
        </>
      )}
      {prep && (
        <>
          <p data-testid="slots-count"><strong>{P.toolCount(prep.slots.length, prep.limit ?? prep.slots.length)}</strong></p>
          <ol className="usage-list" aria-label={P.toolTitle}>
            {prep.slots.map((s, i) => (
              <li key={`${s.x},${s.y}`}>
                {`${i + 1}: ${s.x + 1}, ${s.y + 1}`}{' '}
                <button type="button" className="btn ghost sm icon" aria-label={P.remove(`slot ${i + 1}`)} onClick={() => edit((d) => editMap(d, mapId, (m) => removeSlot(m, i)))}><Icon name="trash" /></button>
              </li>
            ))}
          </ol>
        </>
      )}
    </div>
  )
}
