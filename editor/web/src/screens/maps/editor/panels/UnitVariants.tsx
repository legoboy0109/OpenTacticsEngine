// The variants of a unit that share its square (docs/DIFFICULTY.md "One unit, different numbers per difficulty"): pick which one the
// panel shows, and copy a unit for the difficulties it lacks. Shown only when the square holds variants or the unit is limited.
import { difficultyList, difficultyName, hasDifficulties } from '../../../../model/difficulty'
import type { Cell } from '../../../../model/mapGrid'
import { editMap } from '../../../../model/mapCtx'
import { addUnitVariant, unitIndexesAt } from '../../../../model/mapUnits'
import { useDoc, useEdit } from '../../../../store/context'
import { D } from '../../../rules/difficultyText'

export function UnitVariants({ mapId, cell, onSelect, say }: { mapId: string; cell: Cell; onSelect?: (c: Cell) => void; say: (m: string | null) => void }) {
  const doc = useDoc()
  const edit = useEdit()
  const map = doc.maps[mapId]
  if (!map || !hasDifficulties(doc)) return null
  const stack = unitIndexesAt(map, cell.x, cell.y)
  const v = cell.v ?? 0
  const unit = map.units?.[stack[v % Math.max(1, stack.length)]]
  if (!unit || (stack.length < 2 && unit.difficulty === undefined)) return null
  const all = difficultyList(doc).map((d) => d.id)
  const label = (i: number) => {
    const d = map.units![stack[i]].difficulty
    return d === undefined ? D.variantAll : d.map((id) => difficultyName(doc, id)).join(', ')
  }
  const add = () => {
    const r = addUnitVariant(map, cell.x, cell.y, v, all)
    if (r.error) { say(r.error); return }
    const at = (v % stack.length) + 1  // the copy sits right after the unit
    edit((d) => editMap(d, mapId, (m) => addUnitVariant(m, cell.x, cell.y, v, all).map))
    onSelect?.({ x: cell.x, y: cell.y, v: at })
  }
  return (
    <div className="stack tight" data-testid="unit-variants">
      <h4>{D.variantsTitle}</h4>
      {stack.length > 1 && (
        <>
          <p className="help">{D.variantsHelp}</p>
          <div className="check-list">
            {stack.map((_, i) => (
              <button key={i} type="button" className={`btn sm${i === v % stack.length ? ' primary' : ''}`} aria-pressed={i === v % stack.length}
                onClick={() => onSelect?.({ x: cell.x, y: cell.y, v: i })}>{D.variantPick(i + 1, stack.length, label(i))}</button>
            ))}
          </div>
        </>
      )}
      <button type="button" className="btn sm" title={D.variantAddHelp} onClick={add}>{D.variantAdd}</button>
    </div>
  )
}
