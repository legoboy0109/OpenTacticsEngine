// The map settings' exits part: a list of squares (column and row, counted from 1), the same look as a patrol's route.
import { exitsOf, freeExitSquare, MAX_EXITS, setExits, type ExitSquare } from '../../../../model/exits'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { X } from './exitsText'

export function ExitsSettings({ map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const list = exitsOf(map)
  const set = (next: ExitSquare[], key?: string) => edit((d) => setExits(d, mapId, next), key)
  const change = (i: number, p: Partial<ExitSquare>) => set(list.map((s, j) => (j === i ? { ...s, ...p } : s)), `exits:${mapId}:${i}`)
  const free = freeExitSquare(map)
  return (
    <fieldset className="fieldset" data-testid="map-exits">
      <legend>{X.title}</legend>
      <p className="help">{X.help}</p>
      {list.map((s, i) => (
        <div className="form-grid" key={i}>
          <Field label={X.square(i + 1)}>
            <div className="form-grid">
              <NumberInput aria-label={X.column(i + 1)} value={s.x + 1} min={1} max={map.grid.width} onChange={(v) => v !== undefined && change(i, { x: v - 1 })} />
              <NumberInput aria-label={X.row(i + 1)} value={s.y + 1} min={1} max={map.grid.height} onChange={(v) => v !== undefined && change(i, { y: v - 1 })} />
            </div>
          </Field>
          <button type="button" className="btn sm" aria-label={X.remove(i + 1)} onClick={() => set(list.filter((_, j) => j !== i))}>×</button>
        </div>
      ))}
      <button type="button" className="btn sm" disabled={list.length >= MAX_EXITS || free === null} onClick={() => free && set([...list, free])}>{X.add}</button>
    </fieldset>
  )
}
