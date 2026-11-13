// The map editor: warnings, toolbar, canvas and the options panel for one map.
import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { styleView } from '../../../art/scene'
import { useArtScene } from '../../../art/useArt'
import { mapWarnings } from '../../../model/mapChecks'
import { editMap } from '../../../model/mapCtx'
import { gridProblems, repairMap, terrainAt } from '../../../model/mapGrid'
import { removeMapUnit, unitAt } from '../../../model/mapUnits'
import { useDoc, useEdit, useStore } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { useStored } from '../../../ui/hooks'
import { LookContext } from '../../../ui/picture/usePreview'
import { cellKey, reveal } from './geometry'
import { MapCanvas } from './MapCanvas'
import { OptionsPanel } from './OptionsPanel'
import { StatusLine } from './StatusLine'
import { T } from './text'
import { Toolbar } from './Toolbar'
import { beginStroke, type StrokeEnv, type Tool, type ToolState } from './tools'
import { useMapShortcuts } from './useMapShortcuts'
import { MapViewState, useViewSlice } from './viewState'
import { Warnings } from './Warnings'

export function MapEditor({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const map = doc.maps[id]
  const terrain = doc.terrain
  if (!map || !terrain || !doc.units) return null
  const problems = gridProblems(map, terrain)
  if (problems.length) {
    return (
      <Banner tone="error" title={T.repairTitle}
        actions={<button type="button" className="btn primary" onClick={() => edit((d) => editMap(d, id, (m, c) => repairMap(m, c.terrain)))}>{T.repairButton}</button>}>
        <p>{T.repairBody}</p>
        <ul className="usage-list">{problems.map((p, i) => <li key={i}>{p}</li>)}</ul>
        <p className="help">{T.repairUndo}</p>
      </Banner>
    )
  }
  return <Editor id={id} />
}

function Editor({ id }: { id: string }) {
  const doc = useDoc()
  const store = useStore()
  const map = doc.maps[id]!
  const terrain = doc.terrain!
  const unitTypes = doc.units!
  const sides = map.sides ?? []
  const vs = useMemo(() => new MapViewState(), [])
  const [stored, setStored] = useStored<ToolState>(`ote.mapTools.${store.id}`, { tool: 'terrain', terrainId: '', fill: false, unitType: '', unitSide: '', ownerSide: '' })
  const [notice, setNotice] = useState<string | null>(null)
  // The visual style shown on the canvas (the player starts with the first one); only offered when there are several.
  const styles = doc.game?.styles ?? []
  const [storedLook, setLook] = useStored<string>(`ote.mapLook.${store.id}`, '')
  const lookId = styles.some((s) => s.id === storedLook) ? storedLook : styles[0]?.id
  const art = useArtScene(lookId)
  const look = useMemo(() => styleView(doc.game, lookId), [doc.game, lookId])
  const lookChoice = useMemo(() => ({ id: lookId, view: look }), [lookId, look])

  // Remembered choices can name things that were deleted since: fall back to the first one.
  const ts: ToolState = {
    ...stored,
    terrainId: terrain.some((t) => t.id === stored.terrainId) ? stored.terrainId : terrain[0].id,
    unitType: unitTypes.some((u) => u.id === stored.unitType) ? stored.unitType : unitTypes[0]?.id ?? '',
    unitSide: sides.some((s) => s.id === stored.unitSide) ? stored.unitSide : sides[0]?.id ?? '',
    ownerSide: sides.some((s) => s.id === stored.ownerSide) ? stored.ownerSide : '',
  }
  const tsRef = useRef(ts)
  tsRef.current = ts
  const patch = useCallback((p: Partial<ToolState>) => setStored({ ...tsRef.current, ...p }), [setStored])
  const setTool = useCallback((tool: Tool) => patch({ tool }), [patch])

  useEffect(() => { if (notice) { const t = setTimeout(() => setNotice(null), 6000); return () => clearTimeout(t) } }, [notice])

  const warnings = useMemo(() => mapWarnings(doc, map), [doc, map])
  const flagged = useMemo(() => new Set(warnings.filter((w) => w.key.startsWith('blocked:')).map((w) => cellKey(w.cell!)!)), [warnings])

  const env: StrokeEnv = useMemo(() => ({
    store, mapId: id, view: vs, tools: () => tsRef.current, say: setNotice,
    pickTerrain: (tid) => patch({ terrainId: tid, fill: false }),
  }), [store, id, vs, patch])

  const removeAt = useCallback(() => {
    const c = vs.cursorVisible ? vs.cursor : vs.selected
    if (!c || !unitAt(store.getState().doc.maps[id]!, c.x, c.y)) return
    store.edit((d) => editMap(d, id, (m) => removeMapUnit(m, c.x, c.y)))
  }, [vs, store, id])

  const focusCell = useCallback((c: { x: number; y: number }) => {
    const s = vs.size
    const scale = Math.max(vs.view.scale, 28)
    const view = reveal({ scale, x: s.w / 2 - (c.x + 0.5) * scale, y: s.h / 2 - (c.y + 0.5) * scale }, c, s.w, s.h)
    vs.set({ selected: c, cursor: c, cursorVisible: true, view, autoFit: false })
    setTool('select')
  }, [vs, setTool])

  useMapShortcuts({
    onTool: setTool, onFill: () => patch({ tool: 'terrain', fill: !tsRef.current.fill }), onZoom: (k) => vs.zoomBy(k),
    onFit: () => vs.fit(map.grid.width, map.grid.height), onRemove: removeAt, onEscape: () => { vs.set({ selected: null }); setNotice(null) },
  })

  const selected = useViewSlice(vs, (s) => cellKey(s.selected))
  const big = map.grid.width > 64 || map.grid.height > 64
  return (
    <LookContext.Provider value={lookChoice}>
    <div className="map-editor" data-tool={ts.tool}>
      <Warnings warnings={warnings} onFocus={focusCell} />
      <div className="map-main">
        <Toolbar tool={ts.tool} onTool={setTool} vs={vs} mapW={map.grid.width} mapH={map.grid.height}
          looks={styles.length > 1 ? { options: styles.map((x) => ({ value: x.id, label: x.name || x.id })), value: lookId ?? '', onChange: setLook } : undefined} />
        {big && <p className="help">{T.big(map.grid.width, map.grid.height)}</p>}
        <div className="map-stage">
          <MapCanvas map={map} terrain={terrain} unitTypes={unitTypes} flagged={flagged} art={art} style={look} vs={vs}
            begin={(cell, o) => beginStroke(env, cell, o)}
            longPress={(cell) => {
              if (tsRef.current.tool !== 'terrain') return false
              const tid = terrainAt(store.getState().doc.maps[id]!, cell.x, cell.y)
              if (tid) patch({ terrainId: tid, fill: false })
              return !!tid
            }}
            act={(cell) => { beginStroke(env, cell).end(cell) }}
            remove={removeAt}
            escape={() => { vs.set({ selected: null }); setNotice(null) }} />
        </div>
        <StatusLine vs={vs} map={map} terrain={terrain} unitTypes={unitTypes} notice={notice} />
        <p className="help map-keys">{T.shortcuts}</p>
      </div>
      <OptionsPanel doc={doc} map={map} mapId={id} state={ts} selected={selected} onChange={patch} say={setNotice} />
    </div>
    </LookContext.Provider>
  )
}

