// "Battle effects" on the Art screen (docs/BATTLE_SCENE.md "Effects"; docs/EDITOR_UI.md "Art"): the spells and arrows drawn over a fight, who plays them,
// and the newer options (sounds, a lead-in before the hit, layers, flash and shake, a charge on the striker, lists, by unit type, a returning shot).
import { useArtState } from '../../../art/useArt'
import { artBase } from '../../../model/artFiles'
import { DEFAULT_FRAME_MS, weaponGroups } from '../../../model/battleArt'
import {
  addEffect, effectBlend, effectFor, effectList, effectName, effectOrder, effectProblems, effectWeaponChoices, flightBySpeed, hasPictures, isRowRun, removeEffect, setEffectBlend,
  setEffectFor, setEffectOrder, setEffectPicture, setEndOffset, setFlightBySpeed, updateEffect, END_OFFSET_MAX, END_OFFSET_MIN, PAN_AT_MAX, setEffectRangeFixed, setPanAt, type EffectFor,
} from '../../../model/battleEffects'
import { sheetsOf } from '../../../model/graphics'
import { useDoc, useEdit, useStore } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { TextInput } from '../../../ui/TextInput'
import { Toggle } from '../../../ui/Toggle'
import { EffectLists } from './EffectLists'
import { EffectRanges } from './EffectRanges'
import { EffectFades, EffectHides, EffectPulses, EffectSounds } from './EffectTimed'
import { ET } from './effectText'

function EffectForm({ index }: { index: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const e = effectList(doc)[index]
  const sheets = sheetsOf(doc)
  const kind = effectFor(e)
  const travel = e.mode === 'travel'
  const layer = e.layer ?? 'front'
  const pics = hasPictures(e)
  const patch = (p: Partial<typeof e>, group?: string) => edit((d) => updateEffect(d, index, p), group)
  const picture = (p: { sheet?: string; row?: number; count?: number }, group?: string) => edit((d) => setEffectPicture(d, index, p), group)
  const problems = effectProblems(doc, index, (file) => store.art.fileInfo(file))
  const group = (k: string) => `fx${index}:${k}`
  const ms = typeof e.ms === 'number' ? e.ms : Array.isArray(e.ms) ? undefined : DEFAULT_FRAME_MS
  const weapons = effectWeaponChoices(doc), groups = weaponGroups(doc)
  const bySpeed = flightBySpeed(e)
  return (
    <div className="stack battle-rule" data-testid={`effect-${index}`}>
      <div className="battle-fields">
        <Field label={ET.for} help={ET.forHelp}>
          <Select aria-label={`Effect ${index + 1}: ${ET.for}`} value={kind}
            options={[{ value: 'weapon', label: ET.forWeapon }, { value: 'group', label: ET.forGroup }, { value: 'library', label: ET.forLibrary }]}
            onChange={(v) => edit((d) => setEffectFor(d, index, v as EffectFor))} />
        </Field>
        {kind === 'weapon' && (
          <Field label={ET.weapon}>
            <Select aria-label={`Effect ${index + 1}: ${ET.weapon}`} value={e.weapon ?? ''} options={weapons.map((w) => ({ value: w.id, label: w.name }))} onChange={(v) => patch({ weapon: v })} />
          </Field>
        )}
        {kind === 'group' && (
          <Field label={ET.group}>
            <Select aria-label={`Effect ${index + 1}: ${ET.group}`} value={e.group ?? ''} options={groups.map((g) => ({ value: g, label: g }))} onChange={(v) => patch({ group: v })} />
          </Field>
        )}
        <Field label={ET.name} help={ET.nameHelp}>
          <TextInput aria-label={`Effect ${index + 1}: ${ET.name}`} value={e.id ?? ''} onChange={(v) => patch({ id: v.replace(/[^A-Za-z0-9_-]/g, '') || undefined }, group('id'))} />
        </Field>
      </div>
      {pics ? (
        <>
          <div className="battle-fields">
            <Field label={ET.image}>
              <Select aria-label={`Effect ${index + 1}: ${ET.image}`} value={e.sheet ?? ''} options={sheets.map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))} onChange={(v) => picture({ sheet: v })} />
            </Field>
            <Field label={ET.row} help={ET.rowHelp}>
              <NumberInput aria-label={`Effect ${index + 1}: ${ET.row}`} min={1} max={500} value={(e.frames?.[0]?.[1] ?? 0) + 1} onChange={(v) => v !== undefined && picture({ row: v - 1 }, group('row'))} />
            </Field>
            <Field label={ET.frames} help={ET.framesHelp}>
              <NumberInput aria-label={`Effect ${index + 1}: ${ET.frames}`} min={1} max={512} value={e.frames?.length ?? 1} onChange={(v) => v !== undefined && picture({ count: v }, group('count'))} />
            </Field>
            <Field label={ET.ms}>
              <NumberInput aria-label={`Effect ${index + 1}: ${ET.ms}`} min={10} max={10000} value={ms} placeholder={ms === undefined ? 'mixed' : String(DEFAULT_FRAME_MS)} onChange={(v) => v !== undefined && patch({ ms: v }, group('ms'))} />
            </Field>
          </div>
          {!isRowRun(e) && <p className="help">{ET.customFrames((e.frames ?? []).slice(0, 8).map((f) => (f.length === 3 ? `${f[0] + 1},${f[1] + 1} ${f[2]}` : `${f[0] + 1},${f[1] + 1}`)).join(' '))}</p>}
          <div className="battle-fields">
            <Field label={ET.mode}>
              <Select aria-label={`Effect ${index + 1}: ${ET.mode}`} value={travel ? 'travel' : 'target'} options={[{ value: 'target', label: ET.modeTarget }, { value: 'travel', label: ET.modeTravel }]}
                onChange={(v) => patch(v === 'travel'
                  ? { mode: 'travel', hitAtMs: undefined, fromHitFrame: undefined, at: undefined, ...(layer === 'screen' ? { layer: undefined, mirror: undefined } : {}) }
                  : { mode: undefined, travelMs: undefined, speed: undefined, returns: undefined, lingerMs: undefined, endOffset: undefined, hitAfterMs: undefined })} />
            </Field>
            {travel && (
              <>
                <Field label={ET.flightBy}>
                  <Select aria-label={`Effect ${index + 1}: ${ET.flightBy}`} value={bySpeed ? 'speed' : 'time'} options={[{ value: 'time', label: ET.flightTime }, { value: 'speed', label: ET.flightSpeed }]}
                    onChange={(v) => edit((d) => setFlightBySpeed(d, index, v === 'speed'))} />
                </Field>
                {bySpeed ? (
                  <Field label={ET.speed} help={ET.speedHelp}>
                    <NumberInput aria-label={`Effect ${index + 1}: ${ET.speed}`} min={1} max={10000} value={e.speed} onChange={(v) => v !== undefined && patch({ speed: v }, group('speed'))} />
                  </Field>
                ) : (
                  <Field label={ET.travelMs} help={ET.travelHelp}>
                    <NumberInput aria-label={`Effect ${index + 1}: ${ET.travelMs}`} min={20} max={5000} value={e.travelMs ?? 250} onChange={(v) => v !== undefined && patch({ travelMs: v }, group('travel'))} />
                  </Field>
                )}
                <Field label={ET.endOffset} help={ET.endOffsetHelp}>
                  <NumberInput aria-label={`Effect ${index + 1}: ${ET.endOffset}`} allowEmpty min={END_OFFSET_MIN} max={END_OFFSET_MAX} value={e.endOffset} onChange={(v) => edit((d) => setEndOffset(d, index, v), group('endoffset'))} />
                </Field>
                <Field label={ET.linger}>
                  <NumberInput aria-label={`Effect ${index + 1}: ${ET.linger}`} allowEmpty min={0} max={5000} value={e.lingerMs} onChange={(v) => patch({ lingerMs: v ? v : undefined }, group('linger'))} />
                </Field>
                <Field label={ET.hitAfter} help={ET.hitAfterHelp}>
                  <NumberInput aria-label={`Effect ${index + 1}: ${ET.hitAfter}`} allowEmpty min={0} max={15000} value={e.hitAfterMs} onChange={(v) => patch({ hitAfterMs: v }, group('hitafter'))} />
                </Field>
              </>
            )}
            {!travel && layer !== 'screen' && (
              <Field label={ET.at}>
                <Select aria-label={`Effect ${index + 1}: ${ET.at}`} value={e.at ?? 'target'} options={[{ value: 'target', label: ET.atTarget }, { value: 'striker', label: ET.atStriker }]}
                  onChange={(v) => patch({ at: v === 'striker' ? 'striker' : undefined })} />
              </Field>
            )}
            {!travel && (
              <Field label={ET.hitAtMs} help={ET.hitAtHelp}>
                <NumberInput aria-label={`Effect ${index + 1}: ${ET.hitAtMs}`} allowEmpty min={0} max={15000} value={e.hitAtMs} onChange={(v) => patch({ hitAtMs: v ? v : undefined }, group('hitat'))} />
              </Field>
            )}
            <Field label={ET.panAt} help={ET.panAtHelp}>
              <NumberInput aria-label={`Effect ${index + 1}: ${ET.panAt}`} allowEmpty min={0} max={PAN_AT_MAX} value={e.panAtMs} onChange={(v) => edit((d) => setPanAt(d, index, v), group('panat'))} />
            </Field>
            <Field label={ET.layer} help={layer === 'screen' ? ET.layerHelp : undefined}>
              <Select aria-label={`Effect ${index + 1}: ${ET.layer}`} value={layer}
                options={[{ value: 'front', label: ET.layerFront }, { value: 'behind', label: ET.layerBehind }, ...(travel ? [] : [{ value: 'screen', label: ET.layerScreen }])]}
                onChange={(v) => patch({ layer: v === 'front' ? undefined : (v as 'behind' | 'screen'), ...(v === 'screen' ? { at: undefined } : { mirror: undefined, order: undefined }) })} />
            </Field>
            {layer === 'screen' && (
              <Field label={ET.orderLabel} help={ET.orderHelp}>
                <Select aria-label={`Effect ${index + 1}: ${ET.orderLabel}`} value={effectOrder(e)}
                  options={[{ value: 'backdrop', label: ET.orderBackdrop }, { value: 'behind', label: ET.orderBehind }, { value: 'front', label: ET.orderFront }]}
                  onChange={(v) => edit((d) => setEffectOrder(d, index, v as 'backdrop' | 'behind' | 'front'))} />
              </Field>
            )}
            <Field label={ET.blend} help={ET.blendHelp}>
              <Select aria-label={`Effect ${index + 1}: ${ET.blend}`} value={effectBlend(e)} options={[{ value: 'normal', label: ET.blendNormal }, { value: 'add', label: ET.blendAdd }]}
                onChange={(v) => edit((d) => setEffectBlend(d, index, v as 'normal' | 'add'))} />
            </Field>
          </div>
          {!travel && <Toggle label={ET.fromHitFrame} help={ET.fromHitFrameHelp} checked={!!e.fromHitFrame} onChange={(v) => patch({ fromHitFrame: v ? true : undefined })} />}
          {travel && <Toggle label={ET.returns} help={ET.returnsHelp} checked={!!e.returns} onChange={(v) => patch({ returns: v ? true : undefined })} />}
          {(!e.atRange?.length || e.atRangeFixed) && <Toggle label={ET.rangeFixed} help={ET.rangeFixedHelp} checked={!!e.atRangeFixed} onChange={(v) => edit((d) => setEffectRangeFixed(d, index, v))} />}
          {layer === 'screen' && <Toggle label={ET.mirror} help={ET.mirrorHelp} checked={!!e.mirror} onChange={(v) => patch({ mirror: v ? true : undefined })} />}
          <EffectRanges index={index} />
          <EffectSounds index={index} />
          <EffectPulses index={index} kind="flash" />
          <EffectPulses index={index} kind="shake" />
          <EffectHides index={index} />
          <EffectFades index={index} />
        </>
      ) : (
        <>
          <p className="help">{ET.noPictures}</p>
          {sheets.length > 0 && <div><button type="button" className="btn sm" onClick={() => edit((d) => updateEffect(d, index, { sheet: sheets[0].id, frames: [[0, 0]] }))}>{ET.addPictures}</button></div>}
        </>
      )}
      {kind !== 'library' && <EffectLists index={index} />}
      {problems.map((p, i) => <p key={i} className="error-text" role="alert">{ET.blocked}: {p}</p>)}
    </div>
  )
}

export function EffectsCard() {
  const doc = useDoc()
  const edit = useEdit()
  useArtState()
  const list = effectList(doc)
  const hasImages = sheetsOf(doc).length > 0
  return (
    <Card title={ET.title} help={ET.help} id="battle-effects">
      <div className="stack" data-testid="battle-effects">
        {!hasImages && <p className="help">{ET.needImage}</p>}
        {hasImages && list.length === 0 && <p className="help">{ET.none}</p>}
        <ul className="battle-rules" aria-label={ET.title}>
          {list.map((e, i) => (
            <li key={i}>
              <div className="battle-anim-head">
                <strong>{ET.effectN(i + 1, effectName(e, i))}</strong>
                <button type="button" className="btn sm" aria-label={ET.delete(effectName(e, i))} onClick={() => edit((d) => removeEffect(d, i))}><Icon name="trash" />{ET.removeShort}</button>
              </div>
              <EffectForm index={i} />
            </li>
          ))}
        </ul>
        {hasImages && <div><button type="button" className="btn sm" onClick={() => edit((d) => addEffect(d))}><Icon name="plus" />{ET.add}</button></div>}
      </div>
    </Card>
  )
}
