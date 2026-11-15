// "Choose picture…" for a portrait: pick the image, then click the piece (the same piece grid the terrain and unit
// picker uses). Nothing changes in the document until "Use this picture" is pressed.
import { useState } from 'react'
import { useArtState } from '../../art/useArt'
import { artBase } from '../../model/artFiles'
import { findSheet, sheetsOf } from '../../model/graphics'
import { gridInfo } from '../../model/gridGuess'
import type { CellRef, Portrait } from '../../model/types'
import { useDoc, useStore } from '../../store/context'
import { Dialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { PieceGrid } from '../../ui/picture/PieceGrid'
import { Select } from '../../ui/Select'
import { PortraitThumb } from './PortraitThumb'
import { T } from './text'

interface Props { id: string; current: Portrait; onChoose: (sheet: string, cell: CellRef) => void; onClose: () => void }

export function PortraitPicker({ id, current, onChoose, onClose }: Props) {
  const doc = useDoc()
  const store = useStore()
  useArtState()
  const sheets = sheetsOf(doc)
  const [sheetId, setSheetId] = useState(findSheet(doc, current.sheet) ? current.sheet : sheets[0]?.id ?? '')
  const [cell, setCell] = useState<CellRef | null>(sheetId === current.sheet ? current.cell : null)
  const sheet = findSheet(doc, sheetId)
  const img = sheet ? store.art.image(sheet.file) : null
  const info = sheet && img?.status === 'ready' ? gridInfo(img.width, img.height, sheet.cellWidth, sheet.cellHeight) : null
  return (
    <Dialog open title={T.portraitPickTitle(id)} onClose={onClose} wide
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>{T.portraitCancel}</button>
          <button type="button" className="btn primary" disabled={!cell || !sheet} onClick={() => cell && onChoose(sheetId, cell)}>{T.portraitUse}</button>
        </>
      )}>
      <div className="picker-layout">
        <div className="picker-main stack tight">
          <Field label={T.portraitImage}>
            <Select value={sheetId} options={sheets.map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))} onChange={(v) => { setSheetId(v); setCell(null) }} />
          </Field>
          <p className="help">{T.portraitPickHelp}</p>
          {sheet && img?.status === 'ready' && info && (
            <PieceGrid key={sheet.id + sheet.file} image={img.img} width={img.width} height={img.height} cellWidth={sheet.cellWidth} cellHeight={sheet.cellHeight}
              marks={cell ? [{ col: cell[0], row: cell[1] }] : []} onPick={(col, row) => setCell([col, row])} label={T.portraitGridLabel(sheet.name || artBase(sheet.file))} />
          )}
          {sheet && img?.status === 'loading' && <p className="help" role="status">{T.portraitLoading}</p>}
        </div>
        <div className="picker-side stack tight">
          <h3>{T.portraitPreview}</h3>
          {cell && sheet ? <PortraitThumb sheet={sheetId} cell={cell} label={T.portraitPreviewLabel(id)} height={96} /> : <p className="help">{T.portraitNoneChosen}</p>}
        </div>
      </div>
    </Dialog>
  )
}
