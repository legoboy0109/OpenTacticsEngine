// "Choose picture…": pick an image, then click its pieces. The choice becomes a look (model/graphicsLooks.ts) only when
// "Use this picture" is pressed; until then nothing in the document changes.
import { useMemo, useState } from 'react'
import { useArtState } from '../../art/useArt'
import { artBase } from '../../model/artFiles'
import { findSheet, sheetsOf } from '../../model/graphics'
import { emptyPick, lookFromPick, MAX_PIECES, pickFromLook, SPEEDS, type Pick, type PickKind } from '../../model/graphicsLooks'
import { gridInfo } from '../../model/gridGuess'
import type { CellRef, Look, LookKind, ProjectDoc } from '../../model/types'
import { useDoc, useStore } from '../../store/context'
import { Advanced } from '../Card'
import { Dialog } from '../Dialog'
import { Field } from '../Field'
import { MultiSelect } from '../MultiSelect'
import { NumberInput } from '../NumberInput'
import { Select } from '../Select'
import { Check } from '../Toggle'
import { ArtCanvas } from './ArtCanvas'
import { JoinGuide } from './JoinGuide'
import { PieceGrid, type PieceMark } from './PieceGrid'
import { P } from './text'
import { useStructureSample, useTerrainSample, useUnitSample, type LookChoice } from './usePreview'

interface Props {
  kind: LookKind
  thingId: string
  /** Shown in the title, e.g. "Forest" or "Knight (red side)". */
  what: string
  current: Look | null | undefined
  /** The document with `look` put in the place being edited (for the live preview). */
  apply: (doc: ProjectDoc, look: Look) => ProjectDoc
  look: LookChoice
  /** For a picture of one side of a unit: that side's id (the preview shows its colour). */
  side?: string
  onChoose: (look: Look) => void
  onClose: () => void
}

const KINDS: Record<LookKind, PickKind[]> = { terrain: ['one', 'mix', 'animation', 'join'], units: ['one', 'animation'], structures: ['one', 'animation'] }
const same = (a: CellRef, b: CellRef) => a[0] === b[0] && a[1] === b[1]

export function PickerDialog({ kind, thingId, what, current, apply, look, side, onChoose, onClose }: Props) {
  const doc = useDoc()
  const store = useStore()
  useArtState() // re-render when an image finishes loading
  const sheets = sheetsOf(doc)
  const [pick, setPick] = useState<Pick>(() => {
    const p = pickFromLook(current, sheets[0]?.id ?? '')
    return findSheet(doc, p.sheet) ? p : { ...emptyPick(sheets[0]?.id ?? ''), kind: p.kind }
  })
  const [warn, setWarn] = useState(false)
  const sheet = findSheet(doc, pick.sheet)
  const img = sheet ? store.art.image(sheet.file) : null
  const info = sheet && img?.status === 'ready' ? gridInfo(img.width, img.height, sheet.cellWidth, sheet.cellHeight) : null
  const chosen = useMemo(() => lookFromPick(pick), [pick])
  const draftDoc = useMemo(() => (chosen ? apply(doc, chosen) : doc), [doc, chosen]) // eslint-disable-line react-hooks/exhaustive-deps -- `apply` is rebuilt by the parent on every render

  const set = (p: Partial<Pick>) => setPick((cur) => ({ ...cur, ...p }))
  const changeKind = (k: PickKind) => { setWarn(false); set({ kind: k, cells: k === 'one' || k === 'join' ? pick.cells.slice(0, 1) : pick.cells }) }
  const clickPiece = (col: number, row: number) => {
    const c: CellRef = [col, row]
    setWarn(false)
    switch (pick.kind) {
      case 'one': set({ cells: [c] }); break
      case 'mix': {
        const has = pick.cells.some((x) => same(x, c))
        if (has) set({ cells: pick.cells.filter((x) => !same(x, c)) })
        else if (pick.cells.length < MAX_PIECES) set({ cells: [...pick.cells, c] })
        break
      }
      case 'animation': if (pick.cells.length < MAX_PIECES) set({ cells: [...pick.cells, c] }); break
      case 'join':
        if (info && col + 4 <= info.cols && row + 4 <= info.rows) set({ cells: [c] }); else setWarn(true)
        break
    }
  }

  const marks = useMemo<PieceMark[]>(() => {
    if (pick.kind === 'join') return []
    if (pick.kind === 'one') return pick.cells.map(([col, row]) => ({ col, row }))
    const byCell = new Map<string, PieceMark>()
    pick.cells.forEach(([col, row], i) => {
      const k = `${col},${row}`
      const m = byCell.get(k)
      if (m) m.label = `${m.label},${i + 1}`; else byCell.set(k, { col, row, label: String(i + 1) })
    })
    return [...byCell.values()]
  }, [pick.kind, pick.cells])
  const block = pick.kind === 'join' && pick.cells[0] ? { col: pick.cells[0][0], row: pick.cells[0][1], size: 4 } : null

  const hint = pick.kind === 'one' ? (kind === 'terrain' ? P.pickHintOne : kind === 'structures' ? P.pickHintStructureOne : P.pickHintUnitOne) : pick.kind === 'mix' ? P.pickHintMix : pick.kind === 'animation' ? P.pickHintAnimation : P.pickHintJoin
  const speedKey = (Object.entries(SPEEDS).find(([, ms]) => ms === pick.frameMs)?.[0] ?? 'custom') as keyof typeof SPEEDS | 'custom'
  const others = (doc.terrain ?? []).filter((t) => t.id !== thingId)

  return (
    <Dialog open title={P.pickerTitle(what)} onClose={onClose} wide
      footer={(
        <>
          <button type="button" className="btn" onClick={onClose}>{P.cancel}</button>
          <button type="button" className="btn primary" disabled={!chosen} onClick={() => chosen && onChoose(chosen)}>{P.use}</button>
        </>
      )}>
      <div className="picker-layout">
        <div className="picker-main stack tight">
          <Field label={P.image} help={P.imageHelp}>
            <Select value={pick.sheet} options={sheets.map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))}
              onChange={(v) => { setWarn(false); set({ sheet: v, cells: [] }) }} />
          </Field>
          <fieldset className="fieldset">
            <legend>{P.kind}</legend>
            <div className="kind-row" role="radiogroup" aria-label={P.kind}>
              {KINDS[kind].map((k) => (
                <button key={k} type="button" role="radio" aria-checked={pick.kind === k} className="pick pick-inline" onClick={() => changeKind(k)}>
                  <span className="pick-name">{P.kindNames[k]}</span>
                </button>
              ))}
            </div>
            <p className="help">{(kind === 'terrain' ? P.kindHelp : kind === 'structures' ? P.structureKindHelp : P.unitKindHelp)[pick.kind as 'one']}</p>
          </fieldset>
          <p className="help">{hint}</p>
          {warn && <p className="error-text" role="alert">{P.blockOutside}</p>}
          {sheet && img?.status === 'ready' && info && (
            <PieceGrid key={sheet.id + sheet.file} image={img.img} width={img.width} height={img.height} cellWidth={sheet.cellWidth} cellHeight={sheet.cellHeight}
              marks={marks} block={block} hoverBlock={pick.kind === 'join' ? 4 : undefined} onPick={clickPiece} label={P.gridLabel(sheet.name || artBase(sheet.file))} />
          )}
          {sheet && img?.status === 'loading' && <p className="help" role="status">Loading the image…</p>}
          {sheet && img?.status === 'error' && <p className="error-text" role="alert">The image file could not be opened.</p>}
        </div>

        <div className="picker-side stack tight">
          <section aria-label={P.chosen}>
            <h3>{P.chosen}</h3>
            <p className="help" data-testid="chosen-summary">
              {pick.cells.length === 0 ? P.chosenNone : pick.kind === 'join' ? P.pieceAt(pick.cells[0][0], pick.cells[0][1]) : P.chosenPieces(pick.cells.length)}
            </p>
            {pick.cells.length > 0 && (
              <div className="row">
                {pick.kind === 'animation' && pick.cells.length > 1 && <button type="button" className="btn sm" onClick={() => set({ cells: pick.cells.slice(0, -1) })}>{P.removeLast}</button>}
                <button type="button" className="btn sm" onClick={() => set({ cells: [] })}>{P.clearChoice}</button>
              </div>
            )}
          </section>

          {pick.kind === 'animation' && (
            <>
              <Field label={P.speed}>
                <Select value={speedKey} aria-label={P.speed}
                  options={[...(Object.keys(SPEEDS) as Array<keyof typeof SPEEDS>).map((k) => ({ value: k, label: P.speeds[k] })), ...(speedKey === 'custom' ? [{ value: 'custom', label: `${P.custom} (${pick.frameMs} ms)` }] : [])]}
                  onChange={(v) => { if (v in SPEEDS) set({ frameMs: SPEEDS[v as keyof typeof SPEEDS] }) }} />
              </Field>
              <Advanced label={P.advanced}>
                <Field label={P.frameMs} help={P.frameMsHelp}>
                  <NumberInput value={pick.frameMs} min={20} max={60000} onChange={(v) => { if (v !== undefined) set({ frameMs: v }) }} />
                </Field>
              </Advanced>
            </>
          )}

          {pick.kind === 'join' && (
            <>
              {others.length > 0 && (
                <MultiSelect label={P.alsoJoins} help={P.alsoJoinsHelp} value={pick.joinWith} options={others.map((t) => ({ value: t.id, label: t.name }))}
                  onChange={(v) => set({ joinWith: v })} />
              )}
              <Check label={P.joinEdge} help={P.joinEdgeHelp} checked={pick.joinEdge} onChange={(v) => set({ joinEdge: v })} />
              {sheet && <JoinGuide pieceW={sheet.cellWidth} pieceH={sheet.cellHeight} />}
            </>
          )}

          <section aria-label={P.preview}>
            <h3>{P.preview}</h3>
            <p className="help">{P.previewHelp}</p>
            {kind === 'terrain'
              ? <TerrainDraft id={thingId} look={look} doc={draftDoc} what={what} />
              : kind === 'structures'
                ? <StructureDraft id={thingId} look={look} doc={draftDoc} what={what} side={side} />
                : <UnitDraft id={thingId} look={look} doc={draftDoc} what={what} side={side} />}
          </section>
        </div>
      </div>
    </Dialog>
  )
}

function TerrainDraft({ id, look, doc, what }: { id: string; look: LookChoice; doc: ProjectDoc; what: string }) {
  return <ArtCanvas spec={useTerrainSample(id, look, doc)} label={P.previewTerrain(what)} />
}
function UnitDraft({ id, look, doc, what, side }: { id: string; look: LookChoice; doc: ProjectDoc; what: string; side?: string }) {
  return <ArtCanvas spec={useUnitSample(id, look, doc, side ? [side] : undefined)} label={P.previewUnit(what)} />
}
function StructureDraft({ id, look, doc, what, side }: { id: string; look: LookChoice; doc: ProjectDoc; what: string; side?: string }) {
  return <ArtCanvas spec={useStructureSample(id, look, doc, side ? [side] : undefined)} label={P.previewStructure(what)} />
}
