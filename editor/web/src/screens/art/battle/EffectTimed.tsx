// The timed parts of one effect: its sounds (the game's sound picker), flashes and shakes (docs/BATTLE_SCENE.md "Effect sounds", "Flash and shake").
import { DEFAULT_FLASH, DEFAULT_SHAKE, effectList, effectSounds, nextSoundMs, setEffectSound, setEffectTimed } from '../../../model/battleEffects'
import { soundIds } from '../../../model/audio'
import type { FxFlashDef, FxShakeDef } from '../../../model/types'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { ET } from './effectText'

export function EffectSounds({ index }: { index: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const e = effectList(doc)[index]
  const ids = soundIds(doc)
  const rows = effectSounds(e)
  if (ids.length === 0 && rows.length === 0) return <p className="help">{ET.noSounds}</p>
  return (
    <Field label={ET.sounds} help={ET.soundsHelp}>
      <div className="stack tight" data-testid={`effect-sounds-${index}`}>
        {rows.map(([ms, id]) => (
          <div className="row" key={ms}>
            <NumberInput aria-label={`${ET.sounds}: ${ET.soundMs}`} min={0} max={60000} value={ms}
              onChange={(v) => v !== undefined && v !== ms && !(String(v) in (e.sounds ?? {})) && edit((d) => setEffectSound(setEffectSound(d, index, ms, undefined), index, v, id))} />
            <Select aria-label={ET.soundWhich(ms)} value={id}
              options={[...(ids.includes(id) ? [] : [{ value: id, label: ET.soundMissing(id) }]), ...ids.map((s) => ({ value: s, label: s }))]}
              onChange={(v) => edit((d) => setEffectSound(d, index, ms, v))} />
            <button type="button" className="btn sm" aria-label={ET.soundRemove(ms)} onClick={() => edit((d) => setEffectSound(d, index, ms, undefined))}>{ET.removeShort}</button>
          </div>
        ))}
        {ids.length > 0 && <div><button type="button" className="btn sm" aria-label={`${ET.soundAdd} (effect ${index + 1})`} onClick={() => edit((d) => setEffectSound(d, index, nextSoundMs(e), ids[0]))}><Icon name="plus" />{ET.soundAdd}</button></div>}
      </div>
    </Field>
  )
}

/** Flash rows (colour) or shake rows (strength): start, length and the one extra number. */
export function EffectPulses({ index, kind }: { index: number; kind: 'flash' | 'shake' }) {
  const doc = useDoc()
  const edit = useEdit()
  const e = effectList(doc)[index]
  const rows: Array<FxFlashDef | FxShakeDef> = (kind === 'flash' ? e.flash : e.shake) ?? []
  const title = kind === 'flash' ? ET.flash : ET.shake
  const patchRow = (n: number, p: Partial<FxFlashDef & FxShakeDef>, group?: string) =>
    edit((d) => setEffectTimed(d, index, kind, rows.map((r, k) => (k === n ? { ...r, ...p } : r))), group)
  return (
    <Field label={title} help={kind === 'flash' ? ET.flashHelp : ET.shakeHelp}>
      <div className="stack tight" data-testid={`effect-${kind}-${index}`}>
        {rows.map((r, n) => (
          <div className="battle-fields" key={n}>
            <Field label={ET.atMs}><NumberInput aria-label={`${title} ${n + 1}: ${ET.atMs}`} min={0} max={60000} value={r.atMs} onChange={(v) => v !== undefined && patchRow(n, { atMs: v }, `fx${index}:${kind}${n}:at`)} /></Field>
            <Field label={ET.length}><NumberInput aria-label={`${title} ${n + 1}: ${ET.length}`} min={10} max={5000} value={r.ms} onChange={(v) => v !== undefined && patchRow(n, { ms: v }, `fx${index}:${kind}${n}:ms`)} /></Field>
            {kind === 'flash'
              ? <Field label={ET.color}><input type="color" className="input" aria-label={`${title} ${n + 1}: ${ET.color}`} value={(r as FxFlashDef).color ?? '#ffffff'} onChange={(ev) => patchRow(n, { color: ev.target.value }, `fx${index}:flash${n}:color`)} /></Field>
              : <Field label={ET.strength}><NumberInput aria-label={`${title} ${n + 1}: ${ET.strength}`} min={1} max={64} value={(r as FxShakeDef).strength} onChange={(v) => v !== undefined && patchRow(n, { strength: v }, `fx${index}:shake${n}:s`)} /></Field>}
            <button type="button" className="btn sm" aria-label={kind === 'flash' ? ET.removeFlash(n + 1) : ET.removeShake(n + 1)} onClick={() => edit((d) => setEffectTimed(d, index, kind, rows.filter((_, k) => k !== n)))}>{ET.removeShort}</button>
          </div>
        ))}
        <div><button type="button" className="btn sm" aria-label={`${kind === 'flash' ? ET.addFlash : ET.addShake} (effect ${index + 1})`}
          onClick={() => edit((d) => setEffectTimed(d, index, kind, [...rows, kind === 'flash' ? DEFAULT_FLASH : DEFAULT_SHAKE]))}><Icon name="plus" />{kind === 'flash' ? ET.addFlash : ET.addShake}</button></div>
      </div>
    </Field>
  )
}
