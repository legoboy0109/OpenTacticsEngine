// The scripted-style fields of the behaviour control (docs/AI.md "Scripted-style computer players", docs/EDITOR_UI.md "Behaviour"): the route of a patrol, the
// measure of a guardArea distance, and the group of options every mode has (acting order, attack weights, caution, idle turns, never chases, retreat).
// Written through BehaviourEditor's `patch`, so a placed unit, an addUnit and a setBehaviour command all write the same JSON.
import { defaultRetreat, defaultRoute, MAX_IDLE_TURNS, MAX_ORDER, MAX_ROUTE, MAX_STAFF_FIRST, MIN_ROUTE, weightNames, type Retreat, type RouteSquare } from '../../../../model/behaviourScripted'
import type { BehaviourObj } from '../../../../model/behaviour'
import { namedUnits } from '../../../../model/storyIds'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { Field, FieldGroup } from '../../../../ui/Field'
import { MultiSelect } from '../../../../ui/MultiSelect'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { Check } from '../../../../ui/Toggle'
import { S } from './behaviourScriptedText'

interface Props {
  o: BehaviourObj
  patch: (p: Partial<BehaviourObj>, key?: string) => void
  doc: ProjectDoc
  map: MapFile
  id: string
}

/** The patrol's route: a list of squares, counted from 1. */
export function RouteFields({ o, patch, map, id }: Omit<Props, 'doc'>) {
  const route = (Array.isArray(o.route) ? o.route : defaultRoute(map.grid.width, map.grid.height)) as RouteSquare[]
  const set = (next: RouteSquare[], key?: string) => patch({ route: next }, key)
  const change = (i: number, p: Partial<RouteSquare>) => set(route.map((s, j) => (j === i ? { ...s, ...p } : s)), `route:${i}`)
  return (
    <fieldset className="fieldset" data-testid={`${id}-route`}>
      <legend>{S.routeTitle}</legend>
      <p className="help">{S.routeHelp}</p>
      {route.map((s, i) => (
        <div className="form-grid" key={i}>
          <Field label={S.routeSquare(i + 1)}>
            <div className="form-grid">
              <NumberInput aria-label={S.routeColumn(i + 1)} value={s.x + 1} min={1} max={map.grid.width} onChange={(v) => v !== undefined && change(i, { x: v - 1 })} />
              <NumberInput aria-label={S.routeRow(i + 1)} value={s.y + 1} min={1} max={map.grid.height} onChange={(v) => v !== undefined && change(i, { y: v - 1 })} />
            </div>
          </Field>
          <button type="button" className="btn sm" disabled={route.length <= MIN_ROUTE} aria-label={S.routeRemove(i + 1)} onClick={() => set(route.filter((_, j) => j !== i))}>×</button>
        </div>
      ))}
      <button type="button" className="btn sm" disabled={route.length >= MAX_ROUTE}
        onClick={() => { const last = route[route.length - 1]; set([...route, { x: Math.min(map.grid.width - 1, last.x + 1), y: last.y }]) }}>{S.routeAdd}</button>
    </fieldset>
  )
}

/** guardArea: is `within` a number of squares or a walking cost? Only offered with a distance. */
export function MeasureField({ o, patch }: Pick<Props, 'o' | 'patch'>) {
  if (o.within === undefined) return null
  return (
    <Field label={S.measure} help={S.measureHelp}>
      <Select aria-label={S.measure} value={o.measure === 'moveCost' ? 'moveCost' : 'steps'} options={(['steps', 'moveCost'] as const).map((m) => ({ value: m, label: S.measures[m] }))}
        onChange={(v) => patch({ measure: v === 'moveCost' ? 'moveCost' : undefined })} />
    </Field>
  )
}

export function ScriptedFields({ o, patch, doc, map, id }: Props) {
  const sets = weightNames(doc)
  const units = namedUnits(doc, map)
  const retreat = o.retreat !== undefined && typeof o.retreat === 'object' && o.retreat !== null ? (o.retreat as Retreat) : undefined
  const setRetreat = (p: Partial<Retreat>, key?: string) => {
    const next: Retreat = { ...(retreat ?? defaultRetreat()), ...p }
    for (const k of Object.keys(next)) if (next[k] === undefined) delete next[k]  // a cleared choice leaves no key behind
    patch({ retreat: next }, key && `retreat:${key}`)
  }
  const terrain = retreat?.to?.terrain ?? []
  const weights = typeof o.weights === 'string' ? o.weights : ''
  return (
    <FieldGroup label={S.scriptedTitle} help={S.scriptedHelp} className="stack tight">
      <div className="stack tight" data-testid={`${id}-scripted`}>
        <Field label={S.order} help={S.orderHelp}>
          <NumberInput value={o.order} min={-MAX_ORDER} max={MAX_ORDER} allowEmpty placeholder={S.orderPlaceholder} onChange={(v) => patch({ order: v === 0 ? undefined : v }, 'order')} />
        </Field>
        <Field label={S.weights} help={S.weightsHelp}>
          <Select aria-label={S.weights} value={weights} onChange={(v) => patch({ weights: v === '' ? undefined : v })}
            options={[{ value: '', label: S.weightsNone }, ...(weights !== '' && !sets.includes(weights) ? [{ value: weights, label: S.weightsMissing(weights) }] : []), ...sets.map((n) => ({ value: n, label: n }))]} />
        </Field>
        <Field label={S.caution} help={S.cautionHelp}>
          <Select aria-label={S.caution} value={o.caution === 'none' ? 'none' : 'normal'} options={(['normal', 'none'] as const).map((c) => ({ value: c, label: S.cautions[c] }))}
            onChange={(v) => patch({ caution: v === 'none' ? 'none' : undefined })} />
        </Field>
        <Field label={S.idleTurns} help={S.idleTurnsHelp}>
          <NumberInput value={o.idleTurns} min={1} max={MAX_IDLE_TURNS} allowEmpty onChange={(v) => patch({ idleTurns: v }, 'idleTurns')} />
        </Field>
        <MultiSelect label={S.ignores} help={S.ignoresHelp} emptyText={S.ignoresNone} options={units.map((u) => ({ value: u.id, label: u.label }))}
          value={Array.isArray(o.ignores) ? o.ignores : []} onChange={(v) => patch({ ignores: v.length ? v : undefined })} />
        <Field label={S.heals} help={S.healsHelp}>
          <Select aria-label={S.heals} value={o.heals === 'retreating' ? 'retreating' : 'anyone'} options={(['anyone', 'retreating'] as const).map((h) => ({ value: h, label: S.healsChoices[h] }))}
            onChange={(v) => patch({ heals: v === 'retreating' ? 'retreating' : undefined })} />
        </Field>
        <Field label={S.staffFirst} help={S.staffFirstHelp}>
          <NumberInput value={o.staffFirst} min={0} max={MAX_STAFF_FIRST} allowEmpty placeholder={S.staffFirstPlaceholder} onChange={(v) => patch({ staffFirst: v }, 'staffFirst')} />
        </Field>
        <Field label={S.staffPick} help={S.staffPickHelp}>
          <Select aria-label={S.staffPick} value={o.staffPick === 'lastPlaced' ? 'lastPlaced' : 'best'} options={(['best', 'lastPlaced'] as const).map((h) => ({ value: h, label: S.staffPickChoices[h] }))}
            onChange={(v) => patch({ staffPick: v === 'lastPlaced' ? 'lastPlaced' : undefined })} />
        </Field>
        <Field label={S.inflictOnlyOn} help={S.inflictOnlyOnHelp}>
          <Select aria-label={S.inflictOnlyOn} value={o.inflictOnlyOn === 'casters' ? 'casters' : 'anyone'} options={(['anyone', 'casters'] as const).map((h) => ({ value: h, label: S.inflictOnlyOnChoices[h] }))}
            onChange={(v) => patch({ inflictOnlyOn: v === 'casters' ? 'casters' : undefined })} />
        </Field>
        {o.mode === 'loot' && (
          <Field label={S.lootBlockedBy} help={S.lootBlockedByHelp}>
            <Select aria-label={S.lootBlockedBy} value={o.lootBlockedBy === 'units' ? 'units' : 'nothing'} options={(['nothing', 'units'] as const).map((h) => ({ value: h, label: S.lootBlockedByChoices[h] }))}
              onChange={(v) => patch({ lootBlockedBy: v === 'units' ? 'units' : undefined })} />
          </Field>
        )}
        <Check label={S.retreat} help={S.retreatHelp} checked={retreat !== undefined} onChange={(v) => patch({ retreat: v ? defaultRetreat() : undefined })} />
        {retreat !== undefined && (
          <div className="stack tight" data-testid={`${id}-retreat`}>
            <Field label={S.retreatBelow}>
              <NumberInput value={retreat.below} min={1} max={99} onChange={(v) => v !== undefined && setRetreat({ below: v }, 'below')} />
            </Field>
            <Field label={S.retreatUntil}>
              <NumberInput value={retreat.until ?? 100} min={2} max={100} onChange={(v) => v !== undefined && setRetreat({ until: v }, 'until')} />
            </Field>
            <Check label={S.retreatUseItems} checked={retreat.useItems !== false} onChange={(v) => setRetreat({ useItems: v ? undefined : false })} />
            <MultiSelect label={S.retreatTo} help={S.retreatToHelp} emptyText={S.retreatNoTerrain} options={(doc.terrain ?? []).map((t) => ({ value: t.id, label: t.name }))} value={terrain}
              onChange={(v) => setRetreat({ to: v.length ? { terrain: v } : undefined })} />
          </div>
        )}
      </div>
    </FieldGroup>
  )
}
