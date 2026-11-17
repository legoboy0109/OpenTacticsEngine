// The "Sides" tab: who plays, their colour, team and starting resources.
import { useState } from 'react'
import { editMap } from '../../../../model/mapCtx'
import { addSide, canRemoveSide, setSideResource, sideRemovalLoss, updateSide } from '../../../../model/mapSides'
import { removeSideAndStory, sideRemovalUsages } from '../../../../model/storyUnits'
import { S } from '../story/text'
import type { MapFile, MapSide, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { ColorInput } from '../../../../ui/ColorInput'
import { ConfirmDialog } from '../../../../ui/Dialog'
import { ItemListEditor } from '../../../../ui/ItemListEditor'
import { storageMode } from '../../../../model/itemRules'
import { Field, FieldGroup } from '../../../../ui/Field'
import { Icon } from '../../../../ui/Icon'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { T } from '../text'
import { MC } from '../story/forms/mapChangeText'
import { hasDifficulties } from '../../../../model/difficulty'
import { Check } from '../../../../ui/Toggle'
import { D } from '../../../rules/difficultyText'

export function SidesPanel({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const [removing, setRemoving] = useState<string | null>(null)
  const sides = map.sides ?? []
  const target = sides.find((s) => s.id === removing)
  const loss = target ? sideRemovalLoss(map, target.id) : { units: 0, structures: 0 }
  const storyLines = target ? sideRemovalUsages(doc, mapId, target.id) : []
  const apply = (fn: (m: MapFile, c: Parameters<Parameters<typeof editMap>[2]>[1]) => MapFile, key?: string) => edit((d) => editMap(d, mapId, fn), key)
  return (
    <div className="stack tight">
      <div className="row space-between">
        <h3>{T.sidesTitle}</h3>
        <button type="button" className="btn sm" onClick={() => apply((m) => addSide(m))}><Icon name="plus" />{T.addSide}</button>
      </div>
      <p className="help">{T.sidesHelp}</p>
      {sides.map((s, i) => (
        <SideCard key={s.id} side={s} index={i} fog={map.fog === true} resources={doc.game?.resources ?? []} doc={doc} canRemove={canRemoveSide(map)}
          onPatch={(p, k) => apply((m) => updateSide(m, s.id, p), k && `side:${mapId}:${s.id}:${k}`)}
          onResource={(r, v) => apply((m, c) => setSideResource(m, c, s.id, r, v), `side:${mapId}:${s.id}:res:${r}`)}
          onRemove={() => setRemoving(s.id)} />
      ))}
      <ConfirmDialog open={!!target} title={T.removeSideTitle(target?.name ?? '')} confirmLabel={T.removeSide} danger
        onCancel={() => setRemoving(null)}
        onConfirm={() => { if (target) edit((d) => removeSideAndStory(d, mapId, target.id)); setRemoving(null) }}>
        <p>{T.removeSideBody(loss.units, loss.structures)}</p>
        {target && storyLines.length > 0 && (
          <>
            <p>{S.removeSideStory}</p>
            <ul className="usage-list" data-testid="side-story-usages">{storyLines.map((u, i) => <li key={i}>{u.text}</li>)}</ul>
          </>
        )}
      </ConfirmDialog>
    </div>
  )
}

interface CardProps {
  fog: boolean
  side: MapSide
  index: number
  resources: Array<{ id: string; name: string }>
  doc: ProjectDoc
  canRemove: boolean
  onPatch: (p: Parameters<typeof updateSide>[2], key?: string) => void
  onResource: (resource: string, v: number | undefined) => void
  onRemove: () => void
}

function SideCard({ fog, side, index, resources, doc, canRemove, onPatch, onResource, onRemove }: CardProps) {
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
      {fog && side.controller === 'ai' && (
        <Field label={MC.sideSees} help={MC.sideSeesHelp}>
          <Select aria-label={`${side.name} ${MC.sideSees}`} value={side.aiSeesAll === undefined ? 'follow' : side.aiSeesAll ? 'all' : 'bound'}
            options={[{ value: 'follow', label: MC.sideSeesFollow }, { value: 'bound', label: MC.sideSeesBound }, { value: 'all', label: MC.sideSeesAll }]}
            onChange={(v) => onPatch({ aiSeesAll: v === 'follow' ? undefined : v === 'all' })} />
        </Field>
      )}
      {hasDifficulties(doc) && side.controller === 'ai' && (
        <Check checked={side.difficultyShift !== false} label={D.sideShift} help={D.sideShiftHelp}
          onChange={(v) => onPatch({ difficultyShift: v ? undefined : false })} />
      )}
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
      {(storageMode(doc) !== 'off' && (doc.items?.length ?? 0) > 0) || side.storage?.length ? (
        <ItemListEditor label={T.sideStorage} help={storageMode(doc) === 'off' ? T.sideStorageOff : T.sideStorageHelp} entries={side.storage ?? []} items={doc.items ?? []}
          testId={`side-storage-${side.id}`} onChange={(next, k) => onPatch({ storage: next }, k && `storage${k}`)} />
      ) : null}
    </div>
  )
}
