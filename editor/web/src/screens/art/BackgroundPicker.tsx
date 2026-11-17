// "Choose picture…" for a story background: pick the image, then either use all of it or click one piece of it (the same piece
// grid the portrait picker uses). Nothing changes in the document until "Use this picture" is pressed.
import { useState } from 'react'
import { useArtState } from '../../art/useArt'
import { artBase } from '../../model/artFiles'
import { findSheet, sheetsOf } from '../../model/graphics'
import { gridInfo } from '../../model/gridGuess'
import type { CellRef, SceneBackground } from '../../model/types'
import { useDoc, useStore } from '../../store/context'
import { Dialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { PieceGrid } from '../../ui/picture/PieceGrid'
import { Select } from '../../ui/Select'
import { BackgroundThumb } from './BackgroundThumb'
import { T } from './text'

interface Props { id: string; current: SceneBackground; onChoose: (sheet: string, cell: CellRef | null) => void; onClose: () => void }

export function BackgroundPicker({ id, current, onChoose, onClose }: Props) {
  const doc = useDoc()
  const store = useStore()
  useArtState()
  const sheets = sheetsOf(doc)
  const [sheetId, setSheetId] = useState(findSheet(doc, current.sheet) ? current.sheet : sheets[0]?.id ?? '')
  const [piece, setPiece] = useState(!!current.cell)
  const [cell, setCell] = useState<CellRef | null>(sheetId === current.sheet ? current.cell ?? null : null)
  const sheet = findSheet(doc, sheetId)
  const img = sheet ? store.art.image(sheet.file) : null
  const info = sheet && img?.status === 'ready' ? gridInfo(img.width, img.height, sheet.cellWidth, sheet.cellHeight) : null
  const chosen: SceneBackground | null = sheet && (!piece || cell) ? { sheet: sheetId, cell: piece ? cell ?? undefined : undefined } : null
  return (
    <Dialog open title={T.backgroundPickTitle(id)} onClose={onClose} wide
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>{T.backgroundCancel}</button>
          <button type="button" className="btn primary" disabled={!chosen} onClick={() => chosen && onChoose(sheetId, piece ? cell : null)}>{T.backgroundUse}</button>
        </>
      )}>
      <div className="picker-layout">
        <div className="picker-main stack tight">
          <Field label={T.portraitImage}>
            <Select value={sheetId} options={sheets.map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))} onChange={(v) => { setSheetId(v); setCell(null) }} />
          </Field>
          <Field label={T.backgroundsTitle}>
            <Select value={piece ? 'piece' : 'whole'} options={[{ value: 'whole', label: T.backgroundWhole }, { value: 'piece', label: T.backgroundPiece }]} onChange={(v) => setPiece(v === 'piece')} />
          </Field>
          <p className="help">{T.backgroundPickHelp}</p>
          {piece && sheet && img?.status === 'ready' && info && (
            <>
              <p className="help">{T.backgroundPiecePick}</p>
              <PieceGrid key={sheet.id + sheet.file} image={img.img} width={img.width} height={img.height} cellWidth={sheet.cellWidth} cellHeight={sheet.cellHeight}
                marks={cell ? [{ col: cell[0], row: cell[1] }] : []} onPick={(col, row) => setCell([col, row])} label={T.portraitGridLabel(sheet.name || artBase(sheet.file))} />
            </>
          )}
          {sheet && img?.status === 'loading' && <p className="help" role="status">{T.portraitLoading}</p>}
        </div>
        <div className="picker-side stack tight">
          <h3>{T.portraitPreview}</h3>
          {chosen ? <BackgroundThumb background={chosen} label={T.backgroundPreviewLabel(id)} height={96} /> : <p className="help">{T.portraitNoneChosen}</p>}
        </div>
      </div>
    </Dialog>
  )
}
