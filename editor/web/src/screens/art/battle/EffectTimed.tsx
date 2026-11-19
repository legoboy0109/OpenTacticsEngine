// The timed parts of one effect: its sounds (the game's sound picker), flashes and shakes (docs/BATTLE_SCENE.md "Effect sounds", "Flash and shake").
import { DEFAULT_FADE, DEFAULT_FLASH, DEFAULT_HIDE, DEFAULT_SHAKE, effectList, effectSounds, nextSoundMs, setEffectSound, setEffectTimed } from '../../../model/battleEffects'
import { soundIds } from '../../../model/audio'
import type { BattleEffectDef, FxFadeDef, FxFlashDef, FxHideDef, FxShakeDef } from '../../../model/types'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { Toggle } from '../../../ui/Toggle'
import { ET } from './effectText'

/** "atRangeFixed" of one row: shown when the effect is delayed by distance (or the row already has it). */
function RowFixed({ effect, row, onChange }: { effect: BattleEffectDef; row: { atRangeFixed?: boolean }; onChange: (on: boolean) => void }) {
  if (!effect.atRange?.length && !row.atRangeFixed) return null
  return <Toggle label={ET.rangeFixedRow} help={ET.rangeFixedRowHelp} checked={!!row.atRangeFixed} onChange={onChange} />
}

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
            <RowFixed effect={e} row={r} onChange={(on) => patchRow(n, { atRangeFixed: on ? true : undefined })} />
            <button type="button" className="btn sm" aria-label={kind === 'flash' ? ET.removeFlash(n + 1) : ET.removeShake(n + 1)} onClick={() => edit((d) => setEffectTimed(d, index, kind, rows.filter((_, k) => k !== n)))}>{ET.removeShort}</button>
          </div>
        ))}
        <div><button type="button" className="btn sm" aria-label={`${kind === 'flash' ? ET.addFlash : ET.addShake} (effect ${index + 1})`}
          onClick={() => edit((d) => setEffectTimed(d, index, kind, [...rows, kind === 'flash' ? DEFAULT_FLASH : DEFAULT_SHAKE]))}><Icon name="plus" />{kind === 'flash' ? ET.addFlash : ET.addShake}</button></div>
      </div>
    </Field>
  )
}

/** Hiding windows: for a while the backdrop and / or the platforms are not drawn (docs/BATTLE_SCENE.md "Hiding and fading"). */
export function EffectHides({ index }: { index: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const e = effectList(doc)[index]
  const rows: FxHideDef[] = e.hide ?? []
  const patchRow = (n: number, p: Partial<FxHideDef>, group?: string) => edit((d) => setEffectTimed(d, index, 'hide', rows.map((r, k) => (k === n ? { ...r, ...p } : r))), group)
  const toggle = (n: number, what: 'backdrop' | 'platforms', on: boolean) => {
    const cur = rows[n].what
    const next = on ? [...cur.filter((w) => w !== what), what] : cur.filter((w) => w !== what)
    patchRow(n, { what: next.length ? next : cur })  // (at least one layer stays)
  }
  return (
    <Field label={ET.hide} help={ET.hideHelp}>
      <div className="stack tight" data-testid={`effect-hide-${index}`}>
        {rows.map((r, n) => (
          <div className="stack tight battle-rule" key={n}>
            <div className="battle-fields">
              <Field label={ET.atMs}><NumberInput aria-label={`${ET.hide} ${n + 1}: ${ET.atMs}`} min={0} max={60000} value={r.atMs} onChange={(v) => v !== undefined && patchRow(n, { atMs: v }, `fx${index}:hide${n}:at`)} /></Field>
              <Field label={ET.length}><NumberInput aria-label={`${ET.hide} ${n + 1}: ${ET.length}`} min={10} max={60000} value={r.ms} onChange={(v) => v !== undefined && patchRow(n, { ms: v }, `fx${index}:hide${n}:ms`)} /></Field>
              <Field label={ET.hideColor}>
                <div className="row">
                  <input type="color" className="input" aria-label={`${ET.hide} ${n + 1}: ${ET.hideColor}`} value={r.color ?? '#000000'} onChange={(ev) => patchRow(n, { color: ev.target.value }, `fx${index}:hide${n}:color`)} />
                  {r.color !== undefined && <button type="button" className="btn sm" aria-label={`${ET.hide} ${n + 1}: ${ET.hideNoColor}`} onClick={() => patchRow(n, { color: undefined })}>{ET.hideNoColor}</button>}
                </div>
              </Field>
            </div>
            <div className="row wrap">
              <Toggle label={ET.hideBackdrop} checked={r.what.includes('backdrop')} onChange={(on) => toggle(n, 'backdrop', on)} />
              <Toggle label={ET.hidePlatforms} checked={r.what.includes('platforms')} onChange={(on) => toggle(n, 'platforms', on)} />
              <RowFixed effect={e} row={r} onChange={(on) => patchRow(n, { atRangeFixed: on ? true : undefined })} />
              <button type="button" className="btn sm" aria-label={ET.removeHide(n + 1)} onClick={() => edit((d) => setEffectTimed(d, index, 'hide', rows.filter((_, k) => k !== n)))}>{ET.removeShort}</button>
            </div>
          </div>
        ))}
        <div><button type="button" className="btn sm" aria-label={`${ET.addHide} (effect ${index + 1})`} onClick={() => edit((d) => setEffectTimed(d, index, 'hide', [...rows, DEFAULT_HIDE]))}><Icon name="plus" />{ET.addHide}</button></div>
      </div>
    </Field>
  )
}

/** Fades: a colour over everything (or the backdrop only) that fades in, is held, and fades out. */
export function EffectFades({ index }: { index: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const e = effectList(doc)[index]
  const rows: FxFadeDef[] = e.fade ?? []
  const patchRow = (n: number, p: Partial<FxFadeDef>, group?: string) => edit((d) => setEffectTimed(d, index, 'fade', rows.map((r, k) => (k === n ? { ...r, ...p } : r))), group)
  return (
    <Field label={ET.fade} help={ET.fadeHelp}>
      <div className="stack tight" data-testid={`effect-fade-${index}`}>
        {rows.map((r, n) => (
          <div className="stack tight battle-rule" key={n}>
            <div className="battle-fields">
              <Field label={ET.atMs}><NumberInput aria-label={`${ET.fade} ${n + 1}: ${ET.atMs}`} min={0} max={60000} value={r.atMs} onChange={(v) => v !== undefined && patchRow(n, { atMs: v }, `fx${index}:fade${n}:at`)} /></Field>
              <Field label={ET.fadeIn}><NumberInput aria-label={`${ET.fade} ${n + 1}: ${ET.fadeIn}`} min={10} max={5000} value={r.ms} onChange={(v) => v !== undefined && patchRow(n, { ms: v }, `fx${index}:fade${n}:ms`)} /></Field>
              <Field label={ET.fadeHold}><NumberInput aria-label={`${ET.fade} ${n + 1}: ${ET.fadeHold}`} allowEmpty min={0} max={60000} value={r.holdMs} onChange={(v) => patchRow(n, { holdMs: v ? v : undefined }, `fx${index}:fade${n}:hold`)} /></Field>
              <Field label={ET.fadeAlpha}><NumberInput aria-label={`${ET.fade} ${n + 1}: ${ET.fadeAlpha}`} allowEmpty integer={false} min={0.05} max={1} value={r.alpha} onChange={(v) => patchRow(n, { alpha: v }, `fx${index}:fade${n}:alpha`)} /></Field>
              <Field label={ET.color}><input type="color" className="input" aria-label={`${ET.fade} ${n + 1}: ${ET.color}`} value={r.to ?? '#000000'} onChange={(ev) => patchRow(n, { to: ev.target.value }, `fx${index}:fade${n}:color`)} /></Field>
              <Field label={ET.fadeOver}>
                <Select aria-label={`${ET.fade} ${n + 1}: ${ET.fadeOver}`} value={r.over === 'backdrop' ? 'backdrop' : 'all'} options={[{ value: 'all', label: ET.fadeOverAll }, { value: 'backdrop', label: ET.fadeOverBackdrop }]}
                  onChange={(v) => patchRow(n, { over: v === 'backdrop' ? 'backdrop' : undefined })} />
              </Field>
            </div>
            <div className="row wrap">
              <RowFixed effect={e} row={r} onChange={(on) => patchRow(n, { atRangeFixed: on ? true : undefined })} />
              <button type="button" className="btn sm" aria-label={ET.removeFade(n + 1)} onClick={() => edit((d) => setEffectTimed(d, index, 'fade', rows.filter((_, k) => k !== n)))}>{ET.removeShort}</button>
            </div>
          </div>
        ))}
        <div><button type="button" className="btn sm" aria-label={`${ET.addFade} (effect ${index + 1})`} onClick={() => edit((d) => setEffectTimed(d, index, 'fade', [...rows, DEFAULT_FADE]))}><Icon name="plus" />{ET.addFade}</button></div>
      </div>
    </Field>
  )
}
