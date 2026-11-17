// "Walking" on a unit's Picture card: the pictures shown while the unit moves (an image, how big a piece is, a row for each direction, how
// many frames, how long each lasts) with a live preview. For the unit's own picture or a side's own picture. Checks mirror the game's loader.
import { useState } from 'react'
import { useArtState } from '../../../art/useArt'
import { artBase } from '../../../model/artFiles'
import { findSheet, setSheetGrid, sheetsOf } from '../../../model/graphics'
import { type Slot } from '../../../model/graphicsLooks'
import { gameSides } from '../../../model/graphicsUsage'
import { DEFAULT_WALK_MS, WALK_MAX_FRAMES, addWalk, frameCount, handWritten, hasOwnSides, lookOf, removeWalk, rowOfRun, setOwnSides, setWalkCount, setWalkMs, setWalkRow, setWalkSheet, walkProblems, type WalkDir, type WalkTarget } from '../../../model/walking'
import type { Look } from '../../../model/types'
import { useDoc, useEdit, useStore } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { Toggle } from '../../../ui/Toggle'
import { WalkPreview } from './WalkPreview'
import { W } from './text'

interface Props { slot: Slot; entry: Look }

export function WalkingCard({ slot, entry }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  useArtState()
  const sideIds = Object.keys(entry.sides ?? {})
  const [picked, setPicked] = useState('')
  const side = sideIds.includes(picked) ? picked : ''
  const target: WalkTarget = { slot, side: side || undefined }
  const look = lookOf(doc, target)
  if (!look) return null
  const walk = look.walk
  const sides = gameSides(doc)
  const key = `walk:${slot.kind}:${slot.id}:${slot.style ?? ''}:${side}`
  const sheet = findSheet(doc, walk?.sheet ?? look.sheet)
  const own = walk ? hasOwnSides(walk) : false
  const dirs: WalkDir[] = own ? ['down', 'up', 'left', 'right'] : ['down', 'up', 'side']
  const problems = walk ? walkProblems(doc, look, (f) => store.art.fileInfo(f)) : []
  const hand = walk ? handWritten(walk) : []
  const info = sheet ? store.art.fileInfo(sheet.file) : undefined
  const cols = sheet && info ? Math.floor(info.width / sheet.cellWidth) : undefined
  return (
    <section className="walking-card stack tight" aria-label={W.title} data-testid="walking-card">
      <h3>{W.title}</h3>
      <p className="help">{W.help}</p>
      {sideIds.length > 0 && (
        <Field label={W.forWho}>
          <Select aria-label={W.forWho} value={side} options={[{ value: '', label: W.forAll }, ...sideIds.map((s) => ({ value: s, label: W.forSide(sides.find((x) => x.id === s)?.name ?? s) }))]} onChange={setPicked} />
        </Field>
      )}
      {!walk ? (
        <div><button type="button" className="btn" onClick={() => edit((d) => addWalk(d, target, cols))}>{W.add}</button></div>
      ) : (
        <>
          <WalkPreview look={look} />
          <div className="battle-fields">
            <Field label={W.image} help={W.imageHelp}>
              <Select aria-label={`${W.title}: ${W.image}`} value={walk.sheet ?? ''}
                options={[{ value: '', label: W.sameImage }, ...sheetsOf(doc).filter((s) => s.id !== look.sheet).map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))]}
                onChange={(v) => edit((d) => setWalkSheet(d, target, v))} />
            </Field>
            <Field label={W.frameWidth} help={sheet?.id === look.sheet ? W.frameSizeSame : W.frameSizeHelp}>
              <NumberInput aria-label={`${W.title}: ${W.frameWidth}`} min={1} max={2048} value={sheet?.cellWidth} disabled={sheet?.id === look.sheet}
                onChange={(v) => sheet && v !== undefined && edit((d) => setSheetGrid(d, sheet.id, v, sheet.cellHeight), `${key}:w`)} />
            </Field>
            <Field label={W.frameHeight}>
              <NumberInput aria-label={`${W.title}: ${W.frameHeight}`} min={1} max={2048} value={sheet?.cellHeight} disabled={sheet?.id === look.sheet}
                onChange={(v) => sheet && v !== undefined && edit((d) => setSheetGrid(d, sheet.id, sheet.cellWidth, v), `${key}:h`)} />
            </Field>
            <Field label={W.frames} help={W.framesHelp}>
              <NumberInput aria-label={`${W.title}: ${W.frames}`} min={1} max={WALK_MAX_FRAMES} value={frameCount(walk)}
                onChange={(v) => v !== undefined && edit((d) => setWalkCount(d, target, v), `${key}:count`)} />
            </Field>
            <Field label={W.ms}>
              <NumberInput aria-label={`${W.title}: ${W.ms}`} min={20} max={60000} value={walk.frameMs ?? DEFAULT_WALK_MS}
                onChange={(v) => v !== undefined && edit((d) => setWalkMs(d, target, v), `${key}:ms`)} />
            </Field>
          </div>
          <h4>{W.rows}</h4>
          <p className="help">{own ? W.rowsHelp : `${W.rowsHelp} ${W.sideHelp}`}</p>
          <div className="battle-fields">
            {dirs.map((dir) => (
              <Field key={dir} label={W.rowLabel(W.row[dir])}>
                <NumberInput aria-label={`${W.title}: ${W.rowLabel(W.row[dir])}`} allowEmpty min={1} max={500}
                  value={rowOfRun(walk[dir]) === undefined ? undefined : rowOfRun(walk[dir])! + 1}
                  placeholder={walk[dir] && rowOfRun(walk[dir]) === undefined ? 'by hand' : undefined}
                  onChange={(v) => edit((d) => setWalkRow(d, target, dir, v === undefined ? undefined : v - 1), `${key}:row:${dir}`)} />
              </Field>
            ))}
          </div>
          <Toggle label={W.ownSides} help={W.ownSidesHelp} checked={own} onChange={(v) => edit((d) => setOwnSides(d, target, v))} />
          {hand.length > 0 && <p className="help">{W.handWritten(hand.map((h) => W.row[h].toLowerCase()).join(', '))}</p>}
          {problems.map((p, i) => <p key={i} className="error-text" role="alert">{p}</p>)}
          <div><button type="button" className="btn" onClick={() => edit((d) => removeWalk(d, target))}>{W.remove}</button></div>
        </>
      )}
    </section>
  )
}
