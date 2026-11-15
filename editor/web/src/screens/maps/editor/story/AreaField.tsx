// A square or an area, as numbers AND picked on the map: "Pick on the map" waits for a click (or a dragged rectangle)
// on the canvas. The numbers count from 1 like the map's status line; the file stores them from 0.
import { useEffect, useRef } from 'react'
import { areaText, squareText } from '../../../../model/storySummary'
import type { Rect } from '../../../../model/storyTypes'
import { useCellWords } from '../../../../ui/cellWords'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { useViewSlice } from '../viewState'
import { stopPicking } from './pickStroke'
import { useStory } from './StoryContext'
import { S } from './text'

let counter = 0

interface Props {
  /** Names the group ("When: area"); the number boxes are named from it. */
  label: string
  value: Rect
  onChange: (r: Rect) => void
  mode: 'area' | 'square'
  help?: string
}

export function AreaField({ label, value, onChange, mode, help }: Props) {
  const { vs, map } = useStory()
  const words = useCellWords()
  const id = useRef(0)
  if (!id.current) id.current = ++counter
  const picking = useViewSlice(vs, (s) => s.picking?.id === id.current)
  const latest = useRef(onChange)
  latest.current = onChange
  const W = map.grid.width, H = map.grid.height

  // Leaving the form (another event, another tab) cancels a pick that was waiting.
  useEffect(() => () => { if (vs.picking?.id === id.current) stopPicking(vs) }, [vs])

  const start = () => {
    stopPicking(vs)
    vs.set({ picking: { id: id.current, mode, done: (r) => { if (r) latest.current(mode === 'square' ? { x: r.x, y: r.y, w: 1, h: 1 } : r) } }, pickRect: null })
  }

  /** Keeps the rectangle inside the map whatever number was typed. */
  const set = (p: Partial<Rect>) => {
    const x = Math.min(W - 1, Math.max(0, p.x ?? value.x))
    const y = Math.min(H - 1, Math.max(0, p.y ?? value.y))
    onChange({ x, y, w: Math.min(W - x, Math.max(1, p.w ?? value.w)), h: Math.min(H - y, Math.max(1, p.h ?? value.h)) })
  }
  const num = (v: number | undefined, fn: (n: number) => void) => { if (v !== undefined) fn(v) }
  const square = mode === 'square'
  return (
    <div className="area-field" role="group" aria-label={label}>
      <div className="form-grid area-numbers">
        <Field label={square ? S.squareColumn : S.areaLeft}>
          <NumberInput aria-label={`${label}: ${square ? S.squareColumn : S.areaLeft}`} value={value.x + 1} min={1} max={W} onChange={(v) => num(v, (n) => set({ x: n - 1 }))} />
        </Field>
        <Field label={square ? S.squareRow : S.areaTop}>
          <NumberInput aria-label={`${label}: ${square ? S.squareRow : S.areaTop}`} value={value.y + 1} min={1} max={H} onChange={(v) => num(v, (n) => set({ y: n - 1 }))} />
        </Field>
        {!square && (
          <>
            <Field label={S.areaWidth}>
              <NumberInput aria-label={`${label}: ${S.areaWidth}`} value={value.w} min={1} max={W} onChange={(v) => num(v, (n) => set({ w: n }))} />
            </Field>
            <Field label={S.areaHeight}>
              <NumberInput aria-label={`${label}: ${S.areaHeight}`} value={value.h} min={1} max={H} onChange={(v) => num(v, (n) => set({ h: n }))} />
            </Field>
          </>
        )}
      </div>
      <div className="row area-actions">
        {picking
          ? (
            <>
              <span className="help picking-note" role="status">{words(square ? S.pickingSquare : S.pickingArea)}</span>
              <button type="button" className="btn sm" onClick={() => stopPicking(vs)}>{S.cancelPick}</button>
            </>
          )
          : <button type="button" className="btn sm" data-learn="pick-area" aria-label={`${label}: ${S.pickOnMap}`} onClick={start}>{S.pickOnMap}</button>}
        <span className="help area-readout">{words(square ? `Square ${squareText(value.x, value.y)}` : areaText(value))}</span>
      </div>
      <p className="help">{words(help ?? (square ? S.squareHelp : S.areaHelp))}</p>
    </div>
  )
}
