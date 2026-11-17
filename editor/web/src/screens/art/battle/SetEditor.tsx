// The details of one animation set: which way it faces, mirroring, the foot point, every animation, and the live preview.
import { useState } from 'react'
import { ANIM_KEYS, assignList, deleteSet, renameSet, setAnchor, setFaces, setIdProblem, setList, setMirror, type AnimKey } from '../../../model/battleArt'
import type { BattleSetDef } from '../../../model/types'
import { useDoc, useEdit } from '../../../store/context'
import { ConfirmDialog } from '../../../ui/Dialog'
import { Field } from '../../../ui/Field'
import { IdInput } from '../../../ui/IdInput'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { Toggle } from '../../../ui/Toggle'
import { AnimRow } from './AnimRow'
import { BattlePreview, type PreviewMode } from './BattlePreview'
import { T } from './text'

interface Props { id: string; set: BattleSetDef; onGone: () => void }

export function SetEditor({ id, set, onGone }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const [asking, setAsking] = useState(false)
  const [showKey, setShowKey] = useState<AnimKey>('stand')
  const [fight, setFight] = useState(false)
  const [opponent, setOpponent] = useState('')  // '' = the map picture, else a set id
  const have = ANIM_KEYS.filter((k) => set.animations[k])
  const key = have.includes(showKey) ? showKey : 'stand'
  const mode: PreviewMode = fight ? { kind: 'fight', opponent: opponent === '' ? undefined : setList(doc).find((s) => s.id === opponent)?.set } : { kind: 'anim', key }
  const rules = assignList(doc).filter((a) => a.set === id).length
  const anchorX = set.anchor?.[0], anchorY = set.anchor?.[1]
  const setAnchorPart = (x: number | undefined, y: number | undefined) =>
    edit((d) => setAnchor(d, id, x === undefined && y === undefined ? undefined : [x ?? 0, y ?? 0]), `${id}:anchor`)
  return (
    <div className="stack battle-set" data-testid={`set-${id}`}>
      <div className="battle-preview-box stack tight">
        <BattlePreview set={set} mode={mode} label={T.previewLabel(id)} />
        <div className="row wrap">
          {!fight && (
            <Field label={T.previewAnim}>
              <Select aria-label={T.previewAnim} value={key} options={have.map((k) => ({ value: k, label: T.anim[k].label }))} onChange={(v) => setShowKey(v as AnimKey)} />
            </Field>
          )}
          {fight && (
            <Field label={T.opponent}>
              <Select aria-label={T.opponent} value={opponent} options={[{ value: '', label: T.opponentMap }, ...setList(doc).map((s) => ({ value: s.id, label: s.id === id ? T.opponentSame : s.id }))]} onChange={setOpponent} />
            </Field>
          )}
          <button type="button" className="btn" aria-pressed={fight} onClick={() => setFight(!fight)}>{fight ? T.previewStop : T.previewFight}</button>
        </div>
        {fight && <p className="help">{T.previewFightHelp}</p>}
      </div>
      <div className="row wrap">
        <Field label={T.rename}>
          <IdInput aria-label={T.rename} value={id} validate={(t) => setIdProblem(doc, t, id)} onChange={(v) => edit((d) => renameSet(d, id, v), `set:${id}:name`)} />
        </Field>
        <button type="button" className="btn danger sm" onClick={() => setAsking(true)}>{T.delete}</button>
      </div>
      <div className="battle-fields">
        <Field label={T.faces} help={T.facesHelp}>
          <Select aria-label={T.faces} value={set.faces === 'left' ? 'left' : 'right'} options={[{ value: 'right', label: T.right }, { value: 'left', label: T.left }]}
            onChange={(v) => edit((d) => setFaces(d, id, v === 'left' ? 'left' : 'right'))} />
        </Field>
        <Field label={T.anchorX} help={T.anchorHelp}>
          <NumberInput aria-label={T.anchorX} allowEmpty min={0} value={anchorX} onChange={(v) => setAnchorPart(v, anchorY)} />
        </Field>
        <Field label={T.anchorY}>
          <NumberInput aria-label={T.anchorY} allowEmpty min={0} value={anchorY} onChange={(v) => setAnchorPart(anchorX, v)} />
        </Field>
      </div>
      <Toggle label={T.mirror} help={T.mirrorHelp} checked={set.mirror !== false} onChange={(v) => edit((d) => setMirror(d, id, v))} />
      <h3>{T.animations}</h3>
      <div className="stack tight">
        {ANIM_KEYS.map((k) => <AnimRow key={k} setId={id} animKey={k} anim={set.animations[k]} />)}
      </div>
      <ConfirmDialog open={asking} title={T.deleteTitle(id)} confirmLabel={T.deleteConfirm} danger
        onConfirm={() => { edit((d) => deleteSet(d, id)); setAsking(false); onGone() }} onCancel={() => setAsking(false)}>
        <p>{T.deleteBody(rules)}</p>
      </ConfirmDialog>
    </div>
  )
}
