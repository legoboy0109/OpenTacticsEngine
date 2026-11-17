// One animation of a set: its image, the row and number of frames (the frames are made from them), the time per frame and the hit frame.
import { artBase } from '../../../model/artFiles'
import { DEFAULT_FRAME_MS, HIT_ANIMS, MAX_FRAMES, framesFromRow, msList, setAnim, type AnimKey } from '../../../model/battleArt'
import { findSheet, sheetsOf } from '../../../model/graphics'
import type { BattleAnimDef } from '../../../model/types'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { T } from './text'

interface Props { setId: string; animKey: AnimKey; anim: BattleAnimDef | undefined }

/** Are the frames one run along a row, starting at the left? (What the Row and Frames boxes can describe.) */
const isRowRun = (a: BattleAnimDef): boolean => a.frames.every(([c, r], i) => c === i && r === a.frames[0][1])

export function AnimRow({ setId, animKey, anim }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const info = T.anim[animKey]
  const patch = (p: Partial<BattleAnimDef>, group?: string) => edit((d) => setAnim(d, setId, animKey, p), group)
  if (!anim) {
    return (
      <div className="battle-anim" data-testid={`anim-${animKey}`}>
        <div className="battle-anim-head">
          <strong>{info.label}</strong>
          <button type="button" className="btn sm" onClick={() => edit((d) => setAnim(d, setId, animKey, {}))}>{T.addAnim(info.label)}</button>
        </div>
        <p className="help">{info.help}</p>
      </div>
    )
  }
  const sheet = findSheet(doc, anim.sheet)
  const ms = msList(anim)
  const uniform = ms.every((m) => m === ms[0])
  const run = isRowRun(anim)
  const hits = HIT_ANIMS.includes(animKey)
  return (
    <div className="battle-anim" data-testid={`anim-${animKey}`}>
      <div className="battle-anim-head">
        <strong>{info.label}</strong>
        {animKey !== 'stand' && <button type="button" className="btn sm" aria-label={T.removeAnim(info.label)} onClick={() => edit((d) => setAnim(d, setId, animKey, null))}>{T.removeShort}</button>}
      </div>
      <p className="help">{info.help}</p>
      {!sheet && <p className="error-text" role="alert">{T.noImage}</p>}
      <div className="battle-fields">
        <Field label={T.image}>
          <Select aria-label={`${info.label}: ${T.image}`} value={anim.sheet} options={sheetsOf(doc).map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))}
            onChange={(v) => patch({ sheet: v })} />
        </Field>
        <Field label={T.row} help={T.rowHelp}>
          <NumberInput aria-label={`${info.label}: ${T.row}`} min={1} max={500} value={anim.frames[0][1] + 1}
            onChange={(v) => v !== undefined && patch({ frames: framesFromRow(v - 1, anim.frames.length) }, `${setId}:${animKey}:row`)} />
        </Field>
        <Field label={T.frames} help={T.framesHelp}>
          <NumberInput aria-label={`${info.label}: ${T.frames}`} min={1} max={MAX_FRAMES} value={anim.frames.length}
            onChange={(v) => v !== undefined && patch({ frames: framesFromRow(anim.frames[0][1], v) }, `${setId}:${animKey}:count`)} />
        </Field>
        <Field label={T.ms} help={uniform ? undefined : T.msMixed}>
          <NumberInput aria-label={`${info.label}: ${T.ms}`} min={10} max={10000} value={uniform ? ms[0] : undefined} placeholder={uniform ? String(DEFAULT_FRAME_MS) : 'mixed'}
            onChange={(v) => v !== undefined && patch({ ms: v }, `${setId}:${animKey}:ms`)} />
        </Field>
        {hits && (
          <Field label={T.hit} help={T.hitHelp}>
            <NumberInput aria-label={`${info.label}: ${T.hit}`} allowEmpty min={1} max={anim.frames.length} value={anim.hit === undefined ? undefined : anim.hit + 1}
              onChange={(v) => patch({ hit: v === undefined ? undefined : v - 1 }, `${setId}:${animKey}:hit`)} />
          </Field>
        )}
      </div>
      {!run && <p className="help">{T.customFrames(anim.frames.slice(0, 8).map(([c, r]) => `${c + 1},${r + 1}`).join(' ') + (anim.frames.length > 8 ? ' …' : ''))}</p>}
    </div>
  )
}
