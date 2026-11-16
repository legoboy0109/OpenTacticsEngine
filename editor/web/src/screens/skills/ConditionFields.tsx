// The blanks of one condition row, by kind (docs/SRPG.md "CONDITIONS"). Each change hands back the whole new condition.
import { condKindOf } from '../../model/skillRules'
import { structureOptions, terrainOptions, unitOptions, weaponOptions } from './options'
import type { Extra, ProjectDoc } from '../../model/types'
import { Field } from '../../ui/Field'
import { Num } from './Num'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

interface Props { doc: ProjectDoc; cond: Extra; change: (c: Extra, key?: string) => void }

const OPS = ['<=', '>=', '<', '>', '=', '!=']
const opOptions = OPS.map((o) => ({ value: o, label: T.ops[o] }))
const obj = (v: unknown): Extra => (v && typeof v === 'object' ? (v as Extra) : {})

export function ConditionFields({ doc, cond, change }: Props) {
  const kind = condKindOf(cond)
  const o = kind ? obj(cond[kind]) : {}
  switch (kind) {
    case 'hp':
      return (
        <>
          <Field label={T.compare}><Select value={String(o.op ?? '<=')} options={opOptions} onChange={(v) => change({ ...cond, hp: { ...o, op: v } })} /></Field>
          <Field label={T.percent}><Num value={Number(o.percent ?? 50)} min={0} max={100} suffix="%" onChange={(v) => change({ ...cond, hp: { ...o, percent: v } }, 'hp')} /></Field>
        </>
      )
    case 'near': {
      const types = [{ value: '', label: T.nearMatchAny }, ...unitOptions(doc, String(cond.match ?? ''))]
      const setMatch = (v: string) => { const { match: _m, ...rest } = cond; change(v ? { ...rest, match: v } : rest) }
      return (
        <>
          <Field label={T.nearKind}><Select value={cond.near === 'enemy' ? 'enemy' : 'ally'} options={[{ value: 'ally', label: T.nearAlly }, { value: 'enemy', label: T.nearEnemy }]} onChange={(v) => change({ ...cond, near: v })} /></Field>
          <Field label={T.compare}><Select value={String(cond.op ?? '>=')} options={opOptions} onChange={(v) => change({ ...cond, op: v })} /></Field>
          <Field label={T.nearCount}><Num value={Number(cond.count ?? 1)} min={0} max={1000} onChange={(v) => change({ ...cond, count: v }, 'count')} /></Field>
          <Field label={T.nearWithin}><Num value={Number(cond.within ?? 1)} min={1} max={10} onChange={(v) => change({ ...cond, within: v }, 'within')} /></Field>
          <Field label={T.nearMatch}><Select value={String(cond.match ?? '')} options={types} onChange={setMatch} /></Field>
        </>
      )
    }
    case 'weapon': return <Field label={T.weaponPick}><Select value={String(cond.weapon ?? '')} options={weaponOptions(doc, String(cond.weapon ?? ''))} onChange={(v) => change({ ...cond, weapon: v })} /></Field>
    case 'terrain': return <Field label={T.terrainPick}><Select value={String(cond.terrain ?? '')} options={terrainOptions(doc, String(cond.terrain ?? ''))} onChange={(v) => change({ ...cond, terrain: v })} /></Field>
    case 'structure': {
      const v = cond.structure
      const shown = v === true ? 'any' : v === false ? 'none' : String(v ?? '')
      return (
        <Field label={T.structurePick}>
          <Select value={shown} options={structureOptions(doc, shown)} onChange={(x) => change({ ...cond, structure: x === 'any' ? true : x === 'none' ? false : x })} />
        </Field>
      )
    }
    case 'unit': return <Field label={T.unitPick}><Select value={String(cond.unit ?? '')} options={unitOptions(doc, String(cond.unit ?? ''))} onChange={(v) => change({ ...cond, unit: v })} /></Field>
    case 'carrying':
      return (
        <Field label={T.carryingPick}>
          <Select value={cond.carrying === false ? 'no' : 'yes'} options={[{ value: 'yes', label: T.carryingYes }, { value: 'no', label: T.carryingNo }]} onChange={(v) => change({ ...cond, carrying: v === 'yes' })} />
        </Field>
      )
    case 'round':
      return (
        <>
          <Field label={T.compare}><Select value={String(o.op ?? '>=')} options={opOptions} onChange={(v) => change({ ...cond, round: { ...o, op: v } })} /></Field>
          <Field label={T.roundNumber}><Num value={Number(o.value ?? 2)} min={0} max={1000000} onChange={(v) => change({ ...cond, round: { ...o, value: v } }, 'round')} /></Field>
        </>
      )
    case 'initiating':
      return (
        <Field label={T.initiatingPick}>
          <Select value={cond.initiating === false ? 'no' : 'yes'} options={[{ value: 'yes', label: T.initiatingYes }, { value: 'no', label: T.initiatingNo }]} onChange={(v) => change({ ...cond, initiating: v === 'yes' })} />
        </Field>
      )
    case 'distance':
      return (
        <>
          <Field label={T.compare}><Select value={String(o.op ?? '=')} options={opOptions} onChange={(v) => change({ ...cond, distance: { ...o, op: v } })} /></Field>
          <Field label={T.distanceNumber}><Num value={Number(o.value ?? 1)} min={0} max={1000000} onChange={(v) => change({ ...cond, distance: { ...o, value: v } }, 'distance')} /></Field>
        </>
      )
    case 'formula':
      return <Field label={T.formula} help={T.formulaHelp}><TextInput mono value={String(cond.formula ?? '')} onChange={(v) => change({ ...cond, formula: v }, 'formula')} /></Field>
    default: return <p className="help">{T.condKept}</p>
  }
}
