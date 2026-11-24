// "Poses" on a unit's or character's Picture card: the standing picture seen from the front and from behind ("facing"), and the
// pictures for striking, being hurt and being defeated. Each has the shape of the walking pictures; this form edits the front
// (down-left) and back (up-left) rows, the number of frames and the time per frame. Other rows in the file are kept. Checks mirror the
// game's loader (model/artChecks.ts).
import { useState } from 'react'
import { useArtState } from '../../../art/useArt'
import { artBase } from '../../../model/artFiles'
import { sheetsOf } from '../../../model/graphics'
import { type Slot } from '../../../model/graphicsLooks'
import { gameSides } from '../../../model/graphicsUsage'
import { DEFAULT_WALK_MS, POSE_KEYS, WALK_DIRS, WALK_MAX_FRAMES, addPose, frameCount, handWritten, lookOf, removeWalk, rowOfRun, setWalkCount, setWalkMs, setWalkRow, setWalkSheet, walkProblems, type Dims, type PoseKey, type WalkDir, type WalkTarget } from '../../../model/walking'
import type { Look } from '../../../model/types'
import { useDoc, useEdit, useStore } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { PS } from './posesText'
import { W } from './text'

interface Props { slot: Slot; entry: Look }

export function PosesCard({ slot, entry }: Props) {
  const doc = useDoc()
  const store = useStore()
  useArtState()
  const sideIds = Object.keys(entry.sides ?? {})
  const [picked, setPicked] = useState('')
  const side = sideIds.includes(picked) ? picked : ''
  const look = lookOf(doc, { slot, side: side || undefined })
  if (!look) return null
  const sides = gameSides(doc)
  return (
    <section className="walking-card stack tight" aria-label={PS.title} data-testid="poses-card">
      <h3>{PS.title}</h3>
      <p className="help">{PS.help}</p>
      {sideIds.length > 0 && (
        <Field label={PS.forWho}>
          <Select aria-label={`${PS.title}: ${PS.forWho}`} value={side} options={[{ value: '', label: PS.forAll }, ...sideIds.map((s) => ({ value: s, label: PS.forSide(sides.find((x) => x.id === s)?.name ?? s) }))]} onChange={setPicked} />
        </Field>
      )}
      {POSE_KEYS.map((key) => <PoseBlock key={key} poseKey={key} target={{ slot, side: side || undefined, key }} look={look} dims={(f) => store.art.fileInfo(f)} />)}
    </section>
  )
}

function PoseBlock({ poseKey, target, look, dims }: { poseKey: PoseKey; target: WalkTarget; look: Look; dims: Dims }) {
  const doc = useDoc()
  const edit = useEdit()
  const meta = PS.kinds.find((k) => k.key === poseKey)!
  const pose = look[poseKey]
  const key = `pose:${poseKey}:${target.slot.kind}:${target.slot.id}:${target.slot.style ?? ''}:${target.side ?? ''}`
  const rowValue = (dir: WalkDir) => (rowOfRun(pose?.[dir]) === undefined ? undefined : rowOfRun(pose?.[dir])! + 1)
  const sheet = pose ? sheetsOf(doc).find((s) => s.id === (pose.sheet ?? look.sheet)) : sheetsOf(doc).find((s) => s.id === look.sheet)
  const info = sheet ? dims(sheet.file) : undefined
  const problems = pose ? walkProblems(doc, look, dims, poseKey) : []
  const hand = pose ? handWritten(pose) : []
  const others = pose ? WALK_DIRS.filter((d) => d !== 'downLeft' && d !== 'upLeft' && pose[d]) : []
  return (
    <div className="stack tight" data-testid={`pose-${poseKey}`}>
      <h4>{meta.label}</h4>
      <p className="help">{meta.help}</p>
      {!pose ? (
        <div><button type="button" className="btn" onClick={() => edit((d) => addPose(d, target, info && sheet ? Math.floor(info.width / sheet.cellWidth) : undefined))}>{PS.add(meta.label)}</button></div>
      ) : (
        <>
          <div className="battle-fields">
            <Field label={PS.image}>
              <Select aria-label={`${meta.label}: ${PS.image}`} value={pose.sheet ?? ''}
                options={[{ value: '', label: PS.sameImage }, ...sheetsOf(doc).filter((s) => s.id !== look.sheet).map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))]}
                onChange={(v) => edit((d) => setWalkSheet(d, target, v))} />
            </Field>
            <Field label={PS.frames} help={PS.framesHelp}>
              <NumberInput aria-label={`${meta.label}: ${PS.frames}`} min={1} max={WALK_MAX_FRAMES} value={frameCount(pose)}
                onChange={(v) => v !== undefined && edit((d) => setWalkCount(d, target, v), `${key}:count`)} />
            </Field>
            <Field label={PS.ms}>
              <NumberInput aria-label={`${meta.label}: ${PS.ms}`} min={20} max={60000} value={pose.frameMs ?? DEFAULT_WALK_MS}
                onChange={(v) => v !== undefined && edit((d) => setWalkMs(d, target, v), `${key}:ms`)} />
            </Field>
            {(['downLeft', 'upLeft'] as const).map((dir) => (
              <Field key={dir} label={dir === 'downLeft' ? PS.front : PS.back} help={PS.rowHelp}>
                <NumberInput aria-label={`${meta.label}: ${dir === 'downLeft' ? PS.front : PS.back}`} allowEmpty min={1} max={500} value={rowValue(dir)}
                  placeholder={pose[dir] && rowOfRun(pose[dir]) === undefined ? 'by hand' : undefined}
                  onChange={(v) => edit((d) => setWalkRow(d, target, dir, v === undefined ? undefined : v - 1), `${key}:row:${dir}`)} />
              </Field>
            ))}
          </div>
          {others.length > 0 && <p className="help">{PS.other(others.map((o) => W.row[o].toLowerCase()).join(', '))}</p>}
          {hand.length > 0 && <p className="help">{PS.handWritten(hand.map((h) => W.row[h].toLowerCase()).join(', '))}</p>}
          {problems.map((p, i) => <p key={i} className="error-text" role="alert">{p}</p>)}
          <div><button type="button" className="btn" onClick={() => edit((d) => removeWalk(d, target))}>{PS.remove(meta.label)}</button></div>
        </>
      )}
    </div>
  )
}
