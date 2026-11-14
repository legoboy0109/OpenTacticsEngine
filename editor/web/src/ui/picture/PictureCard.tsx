// The "Picture" card on every Terrain and Unit: live preview, Choose picture… / Remove picture, a picture per visual
// style ("For style:") and, for units, a different picture per side.
import { useMemo, useState } from 'react'
import { styleView } from '../../art/scene'
import { sheetsOf } from '../../model/graphics'
import { getLook, setLook, setSideLook, type Slot } from '../../model/graphicsLooks'
import { gameSides, type GameSide } from '../../model/graphicsUsage'
import { isModelEntry } from '../../model/models3d'
import { inheritsGeneralArt, is3dKind, viewKind } from '../../model/styleView'
import type { Look, LookKind, ProjectDoc } from '../../model/types'
import { Link, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Card } from '../Card'
import { Field } from '../Field'
import { Select } from '../Select'
import { ArtCanvas } from './ArtCanvas'
import { ModelEditor } from './ModelEditor'
import { M } from './modelText'
import { PickerDialog } from './PickerDialog'
import { P } from './text'
import { useStructureSample, useTerrainSample, useUnitSample, type LookChoice } from './usePreview'

interface Props { kind: LookKind; id: string; name: string }
type Open = { side?: string } | null

export function PictureCard({ kind, id, name }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const styles = doc.game?.styles ?? []
  const [styleSel, setStyleSel] = useState('')
  const style = styles.some((s) => s.id === styleSel) ? styleSel : ''
  const [open, setOpen] = useState<Open>(null)
  const slot: Slot = { kind, id, style: style || undefined }
  const entry = getLook(doc, slot)
  const hasImages = sheetsOf(doc).length > 0
  const choice = useMemo<LookChoice>(() => ({ id: style || undefined, view: styleView(doc.game, style || undefined) }), [doc.game, style])
  const otherView = !inheritsGeneralArt(doc.game, style || undefined) // a style of another kind of view starts with no pictures
  const key = `picture:${kind}:${id}`
  // A 3D isometric or full 3D style can show a thing as a 3D model instead of a picture (units and structures only).
  const can3d = kind !== 'terrain' && !!style && is3dKind(viewKind(styles.find((s) => s.id === style)))
  const [modelPicked, setModelPicked] = useState<string | null>(null) // "Shown as 3D model" chosen, no file yet
  const modelMode = can3d && (isModelEntry(entry) || modelPicked === `${id}:${style}`)

  return (
    <Card title={P.cardTitle} help={kind === 'terrain' ? P.cardHelpTerrain : kind === 'structures' ? P.cardHelpStructure : P.cardHelpUnit} id={key}>
      <div className="stack tight">
        {styles.length > 1 && (
          <Field label={P.forLook} help={P.lookHelp}>
            <Select value={style} options={[{ value: '', label: P.allLooks }, ...styles.map((s) => ({ value: s.id, label: s.name || s.id }))]} onChange={setStyleSel} />
          </Field>
        )}
        {can3d && (
          <fieldset className="fieldset">
            <legend>{M.group}</legend>
            <div className="kind-row" role="radiogroup" aria-label={M.group}>
              <Radio on={!modelMode} label={M.picture} onPick={() => { setModelPicked(null); if (isModelEntry(entry)) edit((d) => setLook(d, slot, undefined)) }} />
              <Radio on={modelMode} label={M.model} onPick={() => setModelPicked(`${id}:${style}`)} />
            </div>
            {modelMode && entry && !isModelEntry(entry) && <p className="help">{M.pictureGone}</p>}
          </fieldset>
        )}
        {modelMode
          ? <ModelEditor slot={slot} entry={entry} structure={kind === 'structures'} />
          : !hasImages
          ? <p className="help">{P.noImages} <Link to={projectPath(store.id, 'art')}>{P.goToArt}</Link></p>
          : (
            <div className="picture-row">
              <div className="picture-preview">
                {kind === 'terrain' ? <TerrainSample id={id} name={name} look={choice} /> : kind === 'structures' ? <StructureSample id={id} name={name} look={choice} /> : <UnitSample id={id} name={name} look={choice} />}
              </div>
              <div className="stack tight picture-controls">
                {style ? (
                  <fieldset className="fieldset">
                    <legend>{P.lookGroup}</legend>
                    <div className="kind-row" role="radiogroup" aria-label={P.lookGroup}>
                      {!otherView && <Radio on={entry === undefined} label={P.lookSame} onPick={() => edit((d) => setLook(d, slot, undefined))} />}
                      <Radio on={!!entry} label={P.lookOwn} onPick={() => { if (!entry) setOpen({}) }} />
                      <Radio on={otherView ? !entry : entry === null} label={P.lookNone} onPick={() => edit((d) => setLook(d, slot, otherView ? undefined : null))} />
                    </div>
                    {otherView && <p className="help">{P.lookOtherView}</p>}
                    {!otherView && entry === undefined && <p className="help">{P.lookSameHelp}</p>}
                    {!otherView && entry === null && <p className="help">{P.lookNoneHelp}</p>}
                  </fieldset>
                ) : (!entry && <p className="help">{P.none}</p>)}
                <div className="row">
                  {(!style || entry) && <button type="button" className="btn primary" onClick={() => setOpen({})}>{entry ? P.change : P.choose}</button>}
                  {!style && entry && <button type="button" className="btn" onClick={() => edit((d) => setLook(d, slot, undefined))}>{P.remove}</button>}
                </div>
              </div>
            </div>
          )}
        {kind !== 'terrain' && hasImages && !modelMode && <SideList kind={kind} slot={slot} entry={entry} choice={choice} id={id} name={name} onChoose={(side) => setOpen({ side })} />}
      </div>
      {open && (
        <PickerDialog kind={kind} thingId={id} look={choice} side={open.side}
          what={open.side ? `${name} (${gameSides(doc).find((s) => s.id === open.side)?.name ?? open.side})` : name}
          current={open.side ? entry?.sides?.[open.side] : entry}
          apply={(d: ProjectDoc, l: Look) => (open.side ? setSideLook(d, slot, open.side, l) : setLook(d, slot, l))}
          onClose={() => setOpen(null)}
          onChoose={(l) => { edit((d) => (open.side ? setSideLook(d, slot, open.side, l) : setLook(d, slot, l))); setOpen(null) }} />
      )}
    </Card>
  )
}

function Radio({ on, label, onPick }: { on: boolean; label: string; onPick: () => void }) {
  return <button type="button" role="radio" aria-checked={on} className="pick pick-inline" onClick={onPick}><span className="pick-name">{label}</span></button>
}

function TerrainSample({ id, name, look }: { id: string; name: string; look: LookChoice }) {
  return <ArtCanvas spec={useTerrainSample(id, look)} label={P.previewTerrain(name)} />
}
function StructureSample({ id, name, look }: { id: string; name: string; look: LookChoice }) {
  return <ArtCanvas spec={useStructureSample(id, look)} label={P.previewStructure(name)} />
}
function UnitSample({ id, name, look }: { id: string; name: string; look: LookChoice }) {
  return <ArtCanvas spec={useUnitSample(id, look)} label={P.previewUnit(name)} />
}

/** One row per side of the game (from the maps): that side's own picture, if any. */
function SideList({ kind, slot, entry, choice, id, name, onChoose }: { kind: LookKind; slot: Slot; entry: Look | null | undefined; choice: LookChoice; id: string; name: string; onChoose: (side: string) => void }) {
  const doc = useDoc()
  const edit = useEdit()
  const sides = gameSides(doc)
  const structure = kind === 'structures'
  return (
    <section className="side-pictures" aria-label={structure ? P.ownersTitle : P.sidesTitle}>
      <h3>{structure ? P.ownersTitle : P.sidesTitle}</h3>
      <p className="help">{structure ? P.ownersHelp : P.sidesHelp}</p>
      {!entry && <p className="help">{structure ? P.ownersNeedPicture : P.sidesNeedPicture}</p>}
      {entry && sides.length === 0 && <p className="help">{P.sidesNone}</p>}
      {entry && sides.length > 0 && (
        <ul className="side-list">
          {sides.map((s) => {
            const own = entry.sides?.[s.id]
            return (
              <li key={s.id}>
                <SideRow kind={kind} side={s} own={!!own} id={id} name={name} choice={choice} />
                <span className="row">
                  <button type="button" className="btn sm" aria-label={`${own ? P.change : P.sideChoose} ${s.name}`} onClick={() => onChoose(s.id)}>{own ? P.change : P.sideChoose}</button>
                  {own && <button type="button" className="btn sm" aria-label={`${P.sideRemove} ${s.name}`} onClick={() => edit((d) => setSideLook(d, slot, s.id, undefined))}>{P.sideRemove}</button>}
                </span>
              </li>
            )
          })}
        </ul>
      )}
    </section>
  )
}

function SideRow({ kind, side, own, id, name, choice }: { kind: LookKind; side: GameSide; own: boolean; id: string; name: string; choice: LookChoice }) {
  const structure = kind === 'structures'
  return (
    <span className="side-row">
      <span className="swatch" style={{ background: side.color ?? '#888' }} aria-hidden="true" />
      <span className="side-name">
        <strong>{side.name}</strong>
        <span className="help">{side.orphan ? P.sideOrphan : own ? '' : P.sideSame}</span>
      </span>
      {own && (structure ? <StructureSideSample id={id} name={`${name}, ${side.name}`} side={side.id} look={choice} /> : <UnitSideSample id={id} name={`${name}, ${side.name}`} side={side.id} look={choice} />)}
    </span>
  )
}

function UnitSideSample({ id, name, side, look }: { id: string; name: string; side: string; look: LookChoice }) {
  return <ArtCanvas spec={useUnitSample(id, look, undefined, [side])} label={P.previewUnit(name)} />
}
function StructureSideSample({ id, name, side, look }: { id: string; name: string; side: string; look: LookChoice }) {
  return <ArtCanvas spec={useStructureSample(id, look, undefined, [side])} label={P.previewStructure(name)} />
}
