// The behaviour control (docs/AI.md, docs/EDITOR_UI.md "Behaviour"): one block of fields shared by a placed computer unit, the `addUnit` command and the
// `setBehaviour` command. It reads and writes the block through model/behaviour.ts, so every form writes the same JSON.
import { useId } from 'react'
import {
  behaviourObj, contextOf, groupNames, MAX_FROM_TURN, MAX_WITHIN, modeOf, seekKindOf, tidyBehaviour, withMode, withSeekKind,
  BEHAVIOUR_MODES, type Behaviour, type BehaviourMode, type BehaviourObj, type Prefer, type SeekKind,
} from '../../../../model/behaviour'
import { namedUnits } from '../../../../model/storyIds'
import { placesOf } from '../../../../model/placeStory'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { Field } from '../../../../ui/Field'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { Check } from '../../../../ui/Toggle'
import { B } from './behaviourText'
import { MeasureField, RouteFields, ScriptedFields } from './ScriptedFields'

interface Props {
  value: unknown
  /** The new block (undefined = the default, which is written as nothing) and an optional key that merges typing into one undo step. */
  onChange: (next: Behaviour | undefined, key?: string) => void
  doc: ProjectDoc
  map: MapFile
  /** Prefix of the test ids. */
  id?: string
}

const preferKind = (p: Prefer | undefined): 'none' | 'weakest' | 'nearest' | 'unit' | 'type' | 'tag' =>
  p === undefined ? 'none' : typeof p === 'string' ? p : 'unit' in p ? 'unit' : 'type' in p ? 'type' : 'tag'

export function BehaviourEditor({ value, onChange, doc, map, id = 'behaviour' }: Props) {
  const list = useId()
  const o = behaviourObj(value)
  const mode = modeOf(o)
  const ctx = contextOf(doc, map)
  const set = (next: BehaviourObj, key?: string) => onChange(tidyBehaviour(next), key)
  const patch = (p: Partial<BehaviourObj>, key?: string) => set({ ...o, ...p }, key)
  const waits = mode === 'guard' || mode === 'guardArea'
  const units = namedUnits(doc, map)
  const places = placesOf(doc, map)
  const groups = groupNames(map)
  const seek = seekKindOf(o)
  const prefer = preferKind(o.prefer)
  const area = o.area ?? { x: 0, y: 0, w: 1, h: 1 }
  const setPrefer = (kind: string) => {
    const first = kind === 'unit' ? units[0]?.id ?? '' : kind === 'type' ? doc.units?.[0]?.id ?? '' : ''
    patch({ prefer: kind === 'none' ? undefined : kind === 'weakest' || kind === 'nearest' ? kind : kind === 'unit' ? { unit: first } : kind === 'type' ? { type: first } : { tag: '' } })
  }
  const preferText = typeof o.prefer === 'object' ? Object.values(o.prefer)[0] : ''
  return (
    <div className="stack tight" data-testid={id}>
      <Field label={B.mode} help={B.modeHelp[mode]}>
        <Select aria-label={B.mode} value={mode} options={BEHAVIOUR_MODES.map((m) => ({ value: m, label: B.modes[m] }))}
          onChange={(v) => set(withMode(o, v as BehaviourMode, ctx))} />
      </Field>
      {mode === 'guard' && <Check label={B.react} help={B.reactHelp} checked={o.wake === 'react'} onChange={(v) => patch({ wake: v ? 'react' : undefined })} />}
      {mode === 'guardArea' && (
        <>
          {o.area === undefined ? (
            <button type="button" className="btn sm" onClick={() => patch({ area: { x: 0, y: 0, w: 3, h: 3 } })}>{B.addArea}</button>
          ) : (
            <fieldset className="fieldset" data-testid={`${id}-area`}>
              <legend>{B.areaTitle}</legend>
              <p className="help">{B.areaHelp}</p>
              <div className="form-grid">
                <Field label={B.areaLeft}><NumberInput value={area.x + 1} min={1} max={map.grid.width} onChange={(v) => v !== undefined && patch({ area: { ...area, x: v - 1 } }, 'area')} /></Field>
                <Field label={B.areaTop}><NumberInput value={area.y + 1} min={1} max={map.grid.height} onChange={(v) => v !== undefined && patch({ area: { ...area, y: v - 1 } }, 'area')} /></Field>
                <Field label={B.areaWidth}><NumberInput value={area.w ?? 1} min={1} max={map.grid.width} onChange={(v) => v !== undefined && patch({ area: { ...area, w: v } }, 'area')} /></Field>
                <Field label={B.areaHeight}><NumberInput value={area.h ?? 1} min={1} max={map.grid.height} onChange={(v) => v !== undefined && patch({ area: { ...area, h: v } }, 'area')} /></Field>
              </div>
              <button type="button" className="btn sm" onClick={() => patch({ area: undefined })}>{B.noArea}</button>
            </fieldset>
          )}
          <Field label={B.within} help={B.withinHelp}>
            <NumberInput value={o.within} min={1} max={MAX_WITHIN} allowEmpty onChange={(v) => patch({ within: v }, 'within')} />
          </Field>
          <MeasureField o={o} patch={patch} />
        </>
      )}
      {waits && (
        <Field label={B.group} help={B.groupHelp}>
          <TextInput value={o.group ?? ''} placeholder={B.groupPlaceholder} list={`${list}-groups`} onChange={(v) => patch({ group: v.trim() === '' ? undefined : v.replace(/[^A-Za-z0-9_-]/g, '') }, 'group')} />
          <datalist id={`${list}-groups`} aria-label={B.groupSuggestions}>{groups.map((g) => <option key={g} value={g} />)}</datalist>
        </Field>
      )}
      {mode === 'patrol' && <RouteFields o={o} patch={patch} map={map} id={id} />}
      {mode === 'seek' && (
        <>
          <Field label={B.seekWhat}>
            <Select aria-label={B.seekWhat} value={seek} options={(['unit', 'place', 'square'] as SeekKind[]).map((k) => ({ value: k, label: B.seekKinds[k] }))}
              onChange={(v) => set(withSeekKind(o, v as SeekKind, ctx))} />
          </Field>
          {seek === 'unit' && (
            <Field label={B.seekUnit} help={units.length === 0 ? B.seekNeedsName : B.seekUnitHelp}>
              <Select aria-label={B.seekUnit} value={o.unit ?? ''} placeholder={B.noneToPick} options={withUnknown(units.map((u) => ({ value: u.id, label: u.label })), o.unit)} onChange={(v) => patch({ unit: v })} />
            </Field>
          )}
          {seek === 'place' && (
            <Field label={B.seekPlace} help={places.length === 0 ? B.seekNeedsName : B.seekPlaceHelp}>
              <Select aria-label={B.seekPlace} value={o.place ?? ''} placeholder={B.noneToPick} options={withUnknown(places.map((p) => ({ value: p.id, label: `${p.id} (${p.kindName})` })), o.place)} onChange={(v) => patch({ place: v })} />
            </Field>
          )}
          {seek === 'square' && (
            <Field label={B.seekSquare} help={B.seekSquareHelp}>
              <div className="form-grid">
                <NumberInput aria-label={`${B.seekSquare}: column`} value={(o.x ?? 0) + 1} min={1} max={map.grid.width} onChange={(v) => v !== undefined && patch({ x: v - 1 }, 'x')} />
                <NumberInput aria-label={`${B.seekSquare}: row`} value={(o.y ?? 0) + 1} min={1} max={map.grid.height} onChange={(v) => v !== undefined && patch({ y: v - 1 }, 'y')} />
              </div>
            </Field>
          )}
        </>
      )}
      <Field label={B.fromTurn} help={B.fromTurnHelp}>
        <NumberInput value={o.fromTurn} min={1} max={MAX_FROM_TURN} allowEmpty placeholder={B.fromTurnPlaceholder} onChange={(v) => patch({ fromTurn: v }, 'fromTurn')} />
      </Field>
      <Field label={B.prefer} help={B.preferHelp}>
        <Select aria-label={B.prefer} value={prefer} options={(['none', 'weakest', 'nearest', 'unit', 'type', 'tag'] as const).map((k) => ({ value: k, label: B.prefers[k] }))} onChange={setPrefer} />
      </Field>
      {prefer === 'unit' && (
        <Field label={B.preferUnit}>
          <Select aria-label={B.preferUnit} value={String(preferText)} placeholder={B.noneToPick} options={withUnknown(units.map((u) => ({ value: u.id, label: u.label })), String(preferText))} onChange={(v) => patch({ prefer: { unit: v } })} />
        </Field>
      )}
      {prefer === 'type' && (
        <Field label={B.preferType}>
          <Select aria-label={B.preferType} value={String(preferText)} placeholder={B.noneToPick} options={withUnknown((doc.units ?? []).map((t) => ({ value: t.id, label: t.name })), String(preferText))} onChange={(v) => patch({ prefer: { type: v } })} />
        </Field>
      )}
      {prefer === 'tag' && (
        <Field label={B.preferTag}>
          <TextInput value={String(preferText)} onChange={(v) => patch({ prefer: { tag: v } }, 'prefer')} />
        </Field>
      )}
      <ScriptedFields o={o} patch={patch} doc={doc} map={map} id={id} />
    </div>
  )
}

/** A name that is not there any more stays in the list (marked) so it can be seen and changed. */
function withUnknown(options: Array<{ value: string; label: string }>, current: string | undefined) {
  return current && !options.some((x) => x.value === current) ? [{ value: current, label: `${current} (not there any more)` }, ...options] : options
}
