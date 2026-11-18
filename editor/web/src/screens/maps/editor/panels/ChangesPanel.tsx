// The "Changes" tab (docs/EDITOR_UI.md "Map changes and fog options"): named sets of squares that get another terrain during the battle
// (a bridge appears, a wall opens), plus structures that become another kind. Painting records squares into the change (the Terrain
// tool's brush, while "Paint into this change" is on), the change's squares are outlined, and a Before / After switch shows the map with
// or without it. Events apply a change with the "Apply a map change" command (Events tab).
import { useEffect, useState } from 'react'
import { applyCount, addChange, cellsOf, changeIdProblem, changeIds, changesOf, changeUsages, clearChangeCells, deleteChangeAndStory, namedStructures, nextChangeId, removeSwap, renameChange, setSwap, swapKinds, swapsOf } from '../../../../model/mapChanges'
import { editMap } from '../../../../model/mapCtx'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { DeleteConfirm } from '../../../../ui/DeleteConfirm'
import { Field } from '../../../../ui/Field'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { Toggle } from '../../../../ui/Toggle'
import { useViewSlice, type MapViewState } from '../viewState'
import type { ToolState } from '../tools'
import { MC } from '../story/forms/mapChangeText'
import { TerrainPicker } from './pickers'
import { ChangeTiles } from './ChangeTiles'

interface Props {
  doc: ProjectDoc
  map: MapFile
  mapId: string
  vs: MapViewState
  state: ToolState
  onChange: (p: Partial<ToolState>) => void
}

export function ChangesPanel({ doc, map, mapId, vs, state, onChange }: Props) {
  const edit = useEdit()
  const ids = changeIds(map)
  const sel = useViewSlice(vs, (s) => s.changeSel)
  const paint = useViewSlice(vs, (s) => s.paintChange)
  const view = useViewSlice(vs, (s) => s.changeView)
  const current = sel && ids.includes(sel) ? sel : ids[0] ?? null
  const [name, setName] = useState('')
  const [deleting, setDeleting] = useState(false)

  // The tab owns the map's change view: it selects the first change, and leaves everything as it was when it closes.
  useEffect(() => { if (current !== sel) vs.set({ changeSel: current }) }, [vs, current, sel])
  useEffect(() => () => { vs.set({ changeSel: null, paintChange: null, changeView: 'before', changeOutline: [] }) }, [vs])
  // Painting follows the selected change (and stops when it is gone).
  useEffect(() => { if (paint && paint !== current) vs.set({ paintChange: current && paint ? current : null }) }, [vs, paint, current])

  const change = current ? changesOf(map)[current] : undefined
  const cells = cellsOf(change)
  const swaps = swapsOf(change)
  const named = namedStructures(map)
  const [pickStruct, setPickStruct] = useState('')
  const structure = named.some((s) => s.id === pickStruct) ? pickStruct : named[0]?.id ?? ''
  const kinds = swapKinds(doc.structures ?? [], named.find((s) => s.id === structure)?.type ?? '')
  const [pickKind, setPickKind] = useState('')
  const kind = kinds.some((k) => k.id === pickKind) ? pickKind : kinds[0]?.id ?? ''
  const newId = name.trim()
  const problem = newId ? changeIdProblem(map, newId) : null

  const create = (id: string) => {
    edit((d) => editMap(d, mapId, (m) => addChange(m, id)))
    vs.set({ changeSel: id })
    setName('')
  }
  const rename = (to: string) => { if (current && !changeIdProblem(map, to, current)) { edit((d) => renameChange(d, mapId, current, to), `change:${mapId}:${current}`); vs.set({ changeSel: to }) } }
  const startPaint = (on: boolean) => {
    vs.set({ paintChange: on ? current : null })
    if (on) onChange({ tool: 'terrain', fill: false })
  }

  return (
    <div className="stack tight" data-testid="changes-panel">
      <h3>{MC.title}</h3>
      <p className="help">{MC.intro}</p>

      <Field label={MC.add} help={MC.nameHelp}>
        <div className="row">
          <TextInput aria-label={MC.name} value={name} placeholder={nextChangeId(map)} onChange={setName} onEnter={() => { if (!problem) create(newId || nextChangeId(map)) }} />
          <button type="button" className="btn primary" data-testid="change-add" disabled={!!problem} onClick={() => create(newId || nextChangeId(map))}>{MC.add}</button>
        </div>
      </Field>
      {problem && <p className="help error" role="alert">{problem}</p>}

      {ids.length === 0 ? <p className="help" data-testid="changes-none">{MC.none}</p> : (
        <>
          <Field label={MC.select}>
            <Select aria-label={MC.select} value={current ?? ''} options={ids.map((id) => ({ value: id, label: `${id} (${MC.squares(cellsOf(changesOf(map)[id]).length)})` }))} onChange={(v) => vs.set({ changeSel: v })} />
          </Field>
          {current && (
            <>
              <Field label={MC.rename} help={MC.renameFollow}>
                <TextInput key={current} aria-label={`${MC.rename}: ${current}`} value={current} onChange={rename} />
              </Field>
              <p className="help" data-testid="change-summary">{MC.squares(cells.length)}. {MC.usedBy(applyCount(map, current))}</p>

              <h4>{MC.paintTitle}</h4>
              <Toggle checked={paint === current && paint !== null} onChange={startPaint} label={MC.paintOn} help={MC.paintOnHelp} learn="change-paint" />
              <TerrainPicker terrain={doc.terrain ?? []} value={state.terrainId} onChange={(id) => onChange({ terrainId: id, fill: false })} />
              <Field label={MC.view} help={MC.viewHelp}>
                <div className="row" role="radiogroup" aria-label={MC.view}>
                  {(['before', 'after'] as const).map((v) => (
                    <button key={v} type="button" role="radio" aria-checked={view === v} className={`btn sm${view === v ? ' primary' : ''}`} data-testid={`change-view-${v}`} onClick={() => vs.set({ changeView: v })}>{v === 'before' ? MC.before : MC.after}</button>
                  ))}
                </div>
              </Field>
              <div className="row">
                <button type="button" className="btn sm" disabled={cells.length === 0} onClick={() => edit((d) => editMap(d, mapId, (m) => clearChangeCells(m, current)))}>{MC.clear}</button>
                <button type="button" className="btn sm danger" data-testid="change-delete" onClick={() => setDeleting(true)}>{MC.delete}</button>
              </div>

              <ChangeTiles doc={doc} map={map} mapId={mapId} change={current} vs={vs} state={state} onChange={onChange} />

              <h4>{MC.structuresTitle}</h4>
              <p className="help">{MC.structuresHelp}</p>
              {swaps.length > 0 && (
                <ul className="usage-list" data-testid="change-swaps">
                  {swaps.map((s) => (
                    <li key={s.id}>{s.id} → {doc.structures?.find((k) => k.id === s.kind)?.name ?? s.kind}{' '}
                      <button type="button" className="btn sm ghost" aria-label={MC.structureRemove(s.id)} onClick={() => edit((d) => editMap(d, mapId, (m) => removeSwap(m, current, s.id)))}>×</button></li>
                  ))}
                </ul>
              )}
              {named.length === 0 ? <p className="help">{MC.noNamedStructures}</p> : (
                <div className="stack tight">
                  <Field label={MC.structurePick}><Select aria-label={MC.structurePick} value={structure} options={named.map((s) => ({ value: s.id, label: s.id }))} onChange={setPickStruct} /></Field>
                  {kinds.length === 0 ? <p className="help">{MC.noKinds}</p> : (
                    <>
                      <Field label={MC.structureKind}><Select aria-label={MC.structureKind} value={kind} options={kinds.map((k) => ({ value: k.id, label: k.name || k.id }))} onChange={setPickKind} /></Field>
                      <button type="button" className="btn sm" onClick={() => edit((d) => editMap(d, mapId, (m) => setSwap(m, current, structure, kind)))}>{MC.structureAdd}</button>
                    </>
                  )}
                </div>
              )}
            </>
          )}
        </>
      )}
      {current && (
        <DeleteConfirm open={deleting} what={`the change “${current}”`} usages={changeUsages(doc, mapId, current)}
          onCancel={() => setDeleting(false)}
          onConfirm={() => { edit((d) => deleteChangeAndStory(d, mapId, current)); setDeleting(false) }} />
      )}
    </div>
  )
}
