// The blanks of one condition row, by kind (docs/SRPG.md "CONDITIONS"). Each change hands back the whole new condition.
import { condKindOf } from '../../model/skillRules'
import { weaponKinds } from '../../model/skillBlocks'
import { structureOptions, terrainOptions, unitOptions, weaponOptions } from './options'
import type { Extra, ProjectDoc } from '../../model/types'
import { Field } from '../../ui/Field'
import { MultiSelect } from '../../ui/MultiSelect'
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
    case 'terrain': {
      const many = Array.isArray(cond.terrain)
      const list = (many ? cond.terrain : []) as string[]
      const options = terrainOptions(doc, '')
      const first = many ? (list[0] ?? options[0]?.value ?? '') : String(cond.terrain ?? options[0]?.value ?? '')
      return (
        <>
          <Field label={T.terrainHow}>
            <Select value={many ? 'many' : 'one'} options={[{ value: 'one', label: T.terrainOne }, { value: 'many', label: T.terrainMany }]}
              onChange={(m) => change({ ...cond, terrain: m === 'many' ? (first ? [first] : []) : first })} />
          </Field>
          {many
            ? <MultiSelect label={T.terrainPickMany} options={[...options, ...list.filter((t) => !options.some((x) => x.value === t)).map((t) => ({ value: t, label: `${t} (not in this game)` }))]} value={list} onChange={(v) => change({ ...cond, terrain: v })} />
            : <Field label={T.terrainPick}><Select value={String(cond.terrain ?? '')} options={terrainOptions(doc, String(cond.terrain ?? ''))} onChange={(v) => change({ ...cond, terrain: v })} /></Field>}
        </>
      )
    }
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
    case 'condition': {
      const id = String(cond.condition ?? '')
      const options = (doc.conditions ?? []).map((c) => ({ value: c.id, label: c.name }))
      if (id && !options.some((o) => o.value === id)) options.push({ value: id, label: `${id} (not a condition)` })
      return <Field label={T.hasConditionPick}><Select value={id} options={options} onChange={(v) => change({ ...cond, condition: v })} /></Field>
    }
    case 'weaponKind': {
      const kinds = weaponKinds(doc)
      const cur = String(cond.weaponKind ?? '')
      const options = (kinds.includes(cur) || cur === '' ? kinds : [...kinds, cur]).map((k) => ({ value: k, label: kinds.includes(k) ? k : `${k} (no weapon has it)` }))
      return <Field label={T.weaponKindPick} help={T.weaponKindHelp}><Select value={cur} options={options} onChange={(v) => change({ ...cond, weaponKind: v })} /></Field>
    }
    case 'turn': {
      const mode = o.odd ? 'odd' : o.even ? 'even' : o.every !== undefined ? 'every' : 'range'
      const setMode = (m: string) => change({ ...cond, turn: m === 'odd' ? { odd: true } : m === 'even' ? { even: true } : m === 'every' ? { every: 3, offset: 0 } : { from: 2 } })
      const put = (patch: Extra, key: string) => { const next = { ...o, ...patch }; for (const k of Object.keys(next)) if (next[k] === undefined) delete next[k]; change({ ...cond, turn: next }, key) }
      return (
        <>
          <Field label={T.turnPick}><Select value={mode} options={Object.entries(T.turnModes).map(([value, label]) => ({ value, label }))} onChange={setMode} /></Field>
          {mode === 'range' && (
            <>
              <Field label={T.turnFrom}><Num value={Number(o.from ?? 1)} min={1} max={1000000} onChange={(v) => put({ from: v }, 'turnFrom')} /></Field>
              <Field label={T.turnTo} help={T.turnToHelp}><Num value={Number(o.to ?? 1000000)} min={1} max={1000000} onChange={(v) => put({ to: v }, 'turnTo')} /></Field>
            </>
          )}
          {mode === 'every' && (
            <>
              <Field label={T.turnEvery}><Num value={Number(o.every ?? 3)} min={2} max={1000} onChange={(v) => put({ every: v }, 'turnEvery')} /></Field>
              <Field label={T.turnOffset} help={T.turnOffsetHelp}><Num value={Number(o.offset ?? 0)} min={0} max={999} onChange={(v) => put({ offset: v }, 'turnOffset')} /></Field>
            </>
          )}
        </>
      )
    }
    case 'counterattack':
      return (
        <Field label={T.counterPick}>
          <Select value={cond.counterattack === true ? 'yes' : 'no'} options={[{ value: 'no', label: T.counterNo }, { value: 'yes', label: T.counterYes }]} onChange={(v) => change({ ...cond, counterattack: v === 'yes' })} />
        </Field>
      )
    case 'carries': {
      const id = String(cond.carries ?? '')
      const options = (doc.items ?? []).map((i) => ({ value: i.id, label: i.name }))
      if (id && !options.some((x) => x.value === id)) options.push({ value: id, label: `${id} (not an item)` })
      return <Field label={T.carriesPick}><Select value={id} options={options} onChange={(v) => change({ ...cond, carries: v })} /></Field>
    }
    case 'inventory': {
      const v = cond.inventory
      const mode = v === 'full' ? 'full' : v === 'notFull' ? 'notFull' : 'count'
      const setMode = (m: string) => change({ ...cond, inventory: m === 'count' ? { op: '>=', value: 1 } : m })
      return (
        <>
          <Field label={T.inventoryMode}><Select value={mode} options={Object.entries(T.inventoryModes).map(([value, label]) => ({ value, label }))} onChange={setMode} /></Field>
          {mode === 'count' && (
            <>
              <Field label={T.compare}><Select value={String(o.op ?? '>=')} options={opOptions} onChange={(x) => change({ ...cond, inventory: { ...o, op: x } })} /></Field>
              <Field label={T.inventoryCount}><Num value={Number(o.value ?? 1)} min={0} max={1000} onChange={(n) => change({ ...cond, inventory: { ...o, value: n } }, 'inventory')} /></Field>
            </>
          )}
        </>
      )
    }
    case 'strike': {
      const v = cond.strike
      const mode = v === 'first' ? 'first' : v === 'followUp' ? 'followUp' : typeof v === 'number' ? 'number' : v && typeof v === 'object' ? 'compare' : 'first'
      const setMode = (m: string) => change({ ...cond, strike: m === 'number' ? 2 : m === 'compare' ? { op: '>=', value: 2 } : m })
      return (
        <>
          <Field label={T.strikePick} help={T.strikeHelp}><Select value={mode} options={Object.entries(T.strikeModes).map(([value, label]) => ({ value, label }))} onChange={setMode} /></Field>
          {mode === 'number' && <Field label={T.strikeNumber}><Num value={Number(v)} min={1} max={1000} onChange={(n) => change({ ...cond, strike: n }, 'strike')} /></Field>}
          {mode === 'compare' && (
            <>
              <Field label={T.compare}><Select value={String(o.op ?? '>=')} options={opOptions} onChange={(x) => change({ ...cond, strike: { ...o, op: x } })} /></Field>
              <Field label={T.strikeNumber}><Num value={Number(o.value ?? 2)} min={1} max={1000} onChange={(n) => change({ ...cond, strike: { ...o, value: n } }, 'strike')} /></Field>
            </>
          )}
        </>
      )
    }
    case 'formula':
      return <Field label={T.formula} help={T.formulaHelp}><TextInput mono value={String(cond.formula ?? '')} onChange={(v) => change({ ...cond, formula: v }, 'formula')} /></Field>
    default: return <p className="help">{T.condKept}</p>
  }
}
