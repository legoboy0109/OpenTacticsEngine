// The "Sides" tab: who plays, their colour, team and starting resources.
import { useState } from 'react'
import { editMap } from '../../../../model/mapCtx'
import { addSide, canRemoveSide, removeSide, setSideResource, sideRemovalLoss, updateSide } from '../../../../model/mapSides'
import type { MapFile, MapSide, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { ColorInput } from '../../../../ui/ColorInput'
import { ConfirmDialog } from '../../../../ui/Dialog'
import { Field, FieldGroup } from '../../../../ui/Field'
import { Icon } from '../../../../ui/Icon'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { T } from '../text'

export function SidesPanel({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const [removing, setRemoving] = useState<string | null>(null)
  const sides = map.sides ?? []
  const target = sides.find((s) => s.id === removing)
  const loss = target ? sideRemovalLoss(map, target.id) : { units: 0, owners: 0 }
  const apply = (fn: (m: MapFile, c: Parameters<Parameters<typeof editMap>[2]>[1]) => MapFile, key?: string) => edit((d) => editMap(d, mapId, fn), key)
  return (
    <div className="stack tight">
      <div className="row space-between">
        <h3>{T.sidesTitle}</h3>
        <button type="button" className="btn sm" onClick={() => apply((m) => addSide(m))}><Icon name="plus" />{T.addSide}</button>
      </div>
      <p className="help">{T.sidesHelp}</p>
      {sides.map((s, i) => (
        <SideCard key={s.id} side={s} index={i} resources={doc.game?.resources ?? []} canRemove={canRemoveSide(map)}
          onPatch={(p, k) => apply((m) => updateSide(m, s.id, p), k && `side:${mapId}:${s.id}:${k}`)}
          onResource={(r, v) => apply((m, c) => setSideResource(m, c, s.id, r, v), `side:${mapId}:${s.id}:res:${r}`)}
          onRemove={() => setRemoving(s.id)} />
      ))}
      <ConfirmDialog open={!!target} title={T.removeSideTitle(target?.name ?? '')} confirmLabel={T.removeSide} danger
        onCancel={() => setRemoving(null)}
        onConfirm={() => { if (target) apply((m) => removeSide(m, target.id)); setRemoving(null) }}>
        <p>{T.removeSideBody(loss.units, loss.owners)}</p>
      </ConfirmDialog>
    </div>
  )
}

interface CardProps {
  side: MapSide
  index: number
  resources: Array<{ id: string; name: string }>
  canRemove: boolean
  onPatch: (p: Parameters<typeof updateSide>[2], key?: string) => void
  onResource: (resource: string, v: number | undefined) => void
  onRemove: () => void
}

function SideCard({ side, index, resources, canRemove, onPatch, onResource, onRemove }: CardProps) {
  return (
    <div className="subcard side-card" data-testid={`side-${side.id}`}>
      <div className="subcard-head">
        <strong>{index + 1}. {side.name || side.id}</strong>
        <button type="button" className="btn ghost sm" disabled={!canRemove} title={canRemove ? undefined : T.removeSideMin} onClick={onRemove} aria-label={`${T.removeSide} ${side.name || side.id}`}>
          <Icon name="trash" />{T.removeSide}
        </button>
      </div>
      <div className="form-grid panel-form">
        <Field label={T.sideName}><TextInput value={side.name} onChange={(v) => onPatch({ name: v }, 'name')} /></Field>
        <Field label={T.sideColor}><ColorInput value={side.color} aria-label={`${side.name} colour`} onChange={(v) => onPatch({ color: v }, 'color')} /></Field>
        <Field label={T.sideControl}>
          <Select value={side.controller === 'ai' ? 'ai' : 'human'} onChange={(v) => onPatch({ controller: v as 'human' | 'ai' })}
            options={[{ value: 'human', label: T.controlHuman }, { value: 'ai', label: T.controlAi }]} />
        </Field>
        <Field label={T.sideTeam} help={T.sideTeamHelp}><TextInput value={side.team ?? ''} placeholder={side.id} onChange={(v) => onPatch({ team: v }, 'team')} /></Field>
      </div>
      {resources.length > 0 && (
        <FieldGroup label={T.sideResources}>
          <table className="kv"><tbody>
            {resources.map((r) => (
              <tr key={r.id}><th scope="row">{r.name}</th><td>
                <NumberInput aria-label={`${side.name} starting ${r.name}`} value={side.resources?.[r.id]} min={0} allowEmpty placeholder="0" onChange={(v) => onResource(r.id, v)} />
              </td></tr>
            ))}
          </tbody></table>
        </FieldGroup>
      )}
    </div>
  )
}
