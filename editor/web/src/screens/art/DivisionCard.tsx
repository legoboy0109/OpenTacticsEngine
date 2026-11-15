// "How is this image divided?": one picture or a grid of pieces with a width and height.
import { setSheetGrid } from '../../model/graphics'
import { gridSizeFor, hexPieceBox, isOnePicture, QUICK_SIZES } from '../../model/gridGuess'
import type { Sheet } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { T } from './text'

export function DivisionCard({ sheet, width, height }: { sheet: Sheet; width: number; height: number }) {
  const edit = useEdit()
  const game = useDoc().game
  const tile = game?.view?.tileSize ?? 24
  // A hexagon game's pieces are the size of a hexagon's box.
  const box = hexPieceBox(game)
  const one = isOnePicture(width, height, sheet.cellWidth, sheet.cellHeight)
  const key = `art:${sheet.id}:grid`
  const setGrid = (w: number, h: number, k?: string) => edit((d) => setSheetGrid(d, sheet.id, w, h), k)
  return (
    <Card title={T.divide}>
      <div className="stack tight">
        <div className="kind-row" role="radiogroup" aria-label={T.divide}>
          <button type="button" role="radio" aria-checked={one} className="pick pick-inline" onClick={() => { if (!one) setGrid(width, height) }}>
            <span className="pick-name">{T.divideOne}</span>
          </button>
          <button type="button" role="radio" aria-checked={!one} className="pick pick-inline" onClick={() => { if (one) { if (box) setGrid(box.w, box.h); else { const n = gridSizeFor(width, height, tile); setGrid(n, n) } } }}>
            <span className="pick-name">{T.divideGrid}</span>
          </button>
        </div>
        <p className="help">{one ? T.divideOneHelp : T.divideGridHelp}</p>
        {!one && (
          <>
            <FormGrid>
              <Field label={T.pieceWidth} help={T.pixels}>
                <NumberInput value={sheet.cellWidth} min={1} max={width} onChange={(v) => { if (v !== undefined) setGrid(v, sheet.cellHeight, key) }} />
              </Field>
              <Field label={T.pieceHeight} help={T.pixels}>
                <NumberInput value={sheet.cellHeight} min={1} max={height} onChange={(v) => { if (v !== undefined) setGrid(sheet.cellWidth, v, key) }} />
              </Field>
            </FormGrid>
            <div className="row" role="group" aria-label={T.quick}>
              {box && <button type="button" className="btn sm" aria-pressed={sheet.cellWidth === box.w && sheet.cellHeight === box.h} onClick={() => setGrid(box.w, box.h)}>{T.hexButton(box.w, box.h)}</button>}
              {QUICK_SIZES.map((n) => (
                <button key={n} type="button" className="btn sm" aria-pressed={sheet.cellWidth === n && sheet.cellHeight === n} onClick={() => setGrid(n, n)}>{T.quickButton(n)}</button>
              ))}
            </div>
          </>
        )}
      </div>
    </Card>
  )
}
