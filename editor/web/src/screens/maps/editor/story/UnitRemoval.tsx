// Deleting a placed unit. A unit nothing points at goes at once; one that events or objectives point at (by its story
// name) asks first and shows the "used by" list. Either way it is ONE edit: a single Undo brings back the unit and
// everything the clean-up removed.
import { useState, type ReactNode } from 'react'
import type { Cell } from '../../../../model/mapGrid'
import { unitAt } from '../../../../model/mapUnits'
import { unitDisplayName } from '../../../../model/storyIds'
import { removeUnitAndStory, unitRemovalUsages } from '../../../../model/storyUnits'
import { useDoc, useEdit, useStore } from '../../../../store/context'
import { DeleteConfirm } from '../../../../ui/DeleteConfirm'

/** `[remove(cell), dialog]`: call `remove` to delete the unit on that square; render `dialog` somewhere in the screen. */
export function useRemoveUnit(mapId: string): [(c: Cell) => void, ReactNode] {
  const store = useStore()
  const doc = useDoc()
  const edit = useEdit()
  const [target, setTarget] = useState<Cell | null>(null)

  const remove = (c: Cell) => {
    const d = store.getState().doc
    if (unitRemovalUsages(d, mapId, c.x, c.y, c.v).length) setTarget(c)
    else edit((x) => removeUnitAndStory(x, mapId, c.x, c.y, c.v))
  }

  const map = doc.maps[mapId]
  const unit = target && map ? unitAt(map, target.x, target.y, target.v) : undefined
  const dialog = (
    <DeleteConfirm open={!!target && !!unit} what={unit ? `the unit “${unitDisplayName(doc, unit)}”` : ''}
      usages={target ? unitRemovalUsages(doc, mapId, target.x, target.y, target.v) : []}
      onCancel={() => setTarget(null)}
      onConfirm={() => { if (target) edit((x) => removeUnitAndStory(x, mapId, target.x, target.y, target.v)); setTarget(null) }} />
  )
  return [remove, dialog]
}
