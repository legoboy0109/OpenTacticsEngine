// The map editor: warnings, toolbar, canvas and the options panel for one map.
import { placedCharacterIds, withResolvedTypes } from '../../../model/characterUnits'
import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { styleView } from '../../../art/scene'
import { useArtScene } from '../../../art/useArt'
import { mapWarnings } from '../../../model/mapChecks'
import { slotOverlays } from '../../../model/prep'
import { applyToMap, changeOutline } from '../../../model/mapChanges'
import { seenAtStart } from '../../../model/fogSight'
import { editMap } from '../../../model/mapCtx'
import { gridOfGame } from '../../../model/gridSpec'
import { makeGeo } from '../../../model/mapGeo'
import { gridProblems, repairMap, terrainAt } from '../../../model/mapGrid'
import { inheritsGeneralArt } from '../../../model/styleView'
import { removeStructureAt, structureIndexAt } from '../../../model/mapStructures'
import { unitAt } from '../../../model/mapUnits'
import { useDoc, useEdit, useStore } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { cellWords } from '../../../ui/cellWords'
import { useStored } from '../../../ui/hooks'
import { LookContext } from '../../../ui/picture/usePreview'
import { cellKey, reveal } from './geometry'
import { MapCanvas } from './MapCanvas'
import { OptionsPanel } from './OptionsPanel'
import { StatusLine } from './StatusLine'
import { T } from './text'
import { Toolbar } from './Toolbar'
import { pickStroke, stopPicking } from './story/pickStroke'
import { useRemoveUnit } from './story/UnitRemoval'
import { beginStroke, TOOLS, type StrokeEnv, type Tool, type ToolState } from './tools'
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
  const structureTypes = useMemo(() => doc.structures ?? [], [doc.structures])
  // A unit placed as a character has no `type` in the file: the canvas and status line draw it with the character's.
  const vs = useMemo(() => new MapViewState(), [])
  const changeSel = useViewSlice(vs, (s) => s.changeSel)
  const changeView = useViewSlice(vs, (s) => s.changeView)
  // The Changes tab can show the map with the selected change applied ("After").
  const viewMap = useMemo(() => (changeSel && changeView === 'after' ? applyToMap(map, terrain, changeSel) : map), [map, terrain, changeSel, changeView])
  const shownMap = useMemo(() => withResolvedTypes(viewMap, doc.characters), [viewMap, doc.characters])
  const sides = map.sides ?? []
  // Where cells are on the picture (squares or hexagons); one object for the canvas, the input and the view maths.
  const grid = useMemo(() => gridOfGame(doc.game), [doc.game?.grid]) // eslint-disable-line react-hooks/exhaustive-deps
  const tile = doc.game?.view?.tileSize ?? 24
  const geo = useMemo(() => makeGeo(grid, tile, map.grid.width, map.grid.height), [grid, tile, map.grid.width, map.grid.height])
  const [stored, setStored] = useStored<ToolState>(`ote.mapTools.${store.id}`, { tool: 'terrain', terrainId: '', fill: false, unitType: '', unitSide: '', structureType: '', structureSide: '', heightMode: 'raise', heightValue: 1 })
  const [notice, setNotice] = useState<string | null>(null)
  // The visual style shown on the canvas (the player starts with the first one); only offered when there are several.
  const styles = doc.game?.styles ?? []
  const [storedLook, setLook] = useStored<string>(`ote.mapLook.${store.id}`, '')
  const lookId = styles.some((s) => s.id === storedLook) ? storedLook : styles[0]?.id
  // The canvas always draws top-down. A style of another kind of view contributes only its colours; its pictures are made
  // for that view, so the general pictures are used instead.
  const topDown = inheritsGeneralArt(doc.game, lookId)
  const artStyle = topDown ? lookId : undefined
  const art = useArtScene(artStyle)
  const look = useMemo(() => styleView(doc.game, lookId), [doc.game, lookId])
  const lookChoice = useMemo(() => ({ id: artStyle, view: look }), [artStyle, look])

  // Remembered choices can name things that were deleted since: fall back to the first one.
  const ts: ToolState = {
    ...stored,
    // A tool remembered under an old name (the Owners tool became Structures) falls back to the terrain tool.
    tool: TOOLS.includes(stored.tool) ? stored.tool : 'terrain',
    terrainId: terrain.some((t) => t.id === stored.terrainId) ? stored.terrainId : terrain[0].id,
    unitType: unitTypes.some((u) => u.id === stored.unitType) ? stored.unitType : unitTypes[0]?.id ?? '',
    unitTab: doc.characters !== null && stored.unitTab === 'characters' ? 'characters' : 'types',
    // A character stands on a map once: the choice moves on to one that is not placed yet.
    unitCharacter: [stored.unitCharacter, ...(doc.characters ?? []).map((c) => c.id)].find((id) => id && doc.characters?.some((c) => c.id === id) && !placedCharacterIds(map).includes(id)) ?? '',
    unitSide: sides.some((s) => s.id === stored.unitSide) ? stored.unitSide : sides[0]?.id ?? '',
    structureType: structureTypes.some((s) => s.id === stored.structureType) ? stored.structureType : structureTypes[0]?.id ?? '',
    structureSide: sides.some((s) => s.id === stored.structureSide) ? stored.structureSide : '',
    // Older saved choices have no Heights settings.
    heightMode: stored.heightMode ?? 'raise',
    heightValue: typeof stored.heightValue === 'number' ? stored.heightValue : 1,
  }
  const tsRef = useRef(ts)
  tsRef.current = ts
  const patch = useCallback((p: Partial<ToolState>) => setStored({ ...tsRef.current, ...p }), [setStored])
  const setTool = useCallback((tool: Tool) => patch({ tool }), [patch])

  // The Structures tool shows a footprint ghost under the pointer.
  useEffect(() => {
    vs.set({ placing: ts.tool === 'structures' && ts.structureType ? { type: ts.structureType, side: ts.structureSide } : null })
  }, [vs, ts.tool, ts.structureType, ts.structureSide])

  useEffect(() => { if (notice) { const t = setTimeout(() => setNotice(null), 6000); return () => clearTimeout(t) } }, [notice])

  const warnings = useMemo(() => mapWarnings(doc, map), [doc, map])
  const flagged = useMemo(() => new Set(warnings.filter((w) => w.key.startsWith('blocked:')).map((w) => cellKey(w.cell!)!)), [warnings])

  // The preparation's deployment squares, numbered, over the map.
  useEffect(() => { vs.set({ slots: slotOverlays(map) }) }, [vs, map.prep, map]) // eslint-disable-line react-hooks/exhaustive-deps

  // The Changes tab outlines the squares of the change being edited.
  useEffect(() => { vs.set({ changeOutline: changeSel ? changeOutline(map, changeSel) : [] }) }, [vs, map, changeSel])

  // The "Show what a side sees" aid: only while the map has fog and the picked side still exists.
  const fogSide = useViewSlice(vs, (s) => s.fogSide)
  useEffect(() => {
    const live = map.fog === true && fogSide !== null && sides.some((s) => s.id === fogSide)
    if (live || vs.fogSeen) vs.set({ fogSeen: live ? seenAtStart(doc, map, fogSide) : null })
  }, [vs, doc, map, sides, fogSide])

  const env: StrokeEnv = useMemo(() => ({
    store, mapId: id, view: vs, tools: () => tsRef.current, say: setNotice,
    pickTerrain: (tid) => patch({ terrainId: tid, fill: false }),
  }), [store, id, vs, patch])

  // Deleting a unit that events or objectives point at asks first (the dialog is rendered below).
  const [requestRemove, removeDialog] = useRemoveUnit(id)
  const requestRemoveRef = useRef(requestRemove)
  requestRemoveRef.current = requestRemove
  const removeAt = useCallback(() => {
    const c = vs.cursorVisible ? vs.cursor : vs.selected
    const d0 = store.getState().doc
    const m0 = d0.maps[id]
    if (!c || !m0) return
    // The Structures tool deletes the structure on the square; every other tool deletes the unit there.
    if (tsRef.current.tool === 'structures') {
      if (structureIndexAt(m0, { structureTypes: d0.structures ?? [], grid: gridOfGame(d0.game) }, c.x, c.y) >= 0) store.edit((d) => editMap(d, id, (m, x) => removeStructureAt(m, x, c.x, c.y)))
      vs.set({ selected: null })
      return
    }
    if (!unitAt(m0, c.x, c.y)) return
    requestRemoveRef.current(c)
  }, [vs, store, id])

  const focusCell = useCallback((c: { x: number; y: number }) => {
    const s = vs.size
    const scale = Math.max(vs.view.scale, 28)
    const b = geo.box(c.x, c.y)
    const view = reveal({ scale, x: s.w / 2 - (b.x + b.w / 2) * scale, y: s.h / 2 - (b.y + b.h / 2) * scale }, c, s.w, s.h, geo)
    vs.set({ selected: c, cursor: c, cursorVisible: true, view, autoFit: false })
    setTool('select')
  }, [vs, setTool, geo])

  useMapShortcuts({
    onTool: setTool, onFill: () => patch({ tool: 'terrain', fill: !tsRef.current.fill }), onZoom: (k) => vs.zoomBy(k),
    onFit: () => vs.fit(geo.uw, geo.uh), onRemove: removeAt, onEscape: () => { stopPicking(vs); vs.set({ selected: null }); setNotice(null) },
  })

  const selected = useViewSlice(vs, (s) => cellKey(s.selected))
  const picking = useViewSlice(vs, (s) => s.picking !== null)
  const words = cellWords(geo.hex)
  const big = map.grid.width > 64 || map.grid.height > 64
  return (
    <LookContext.Provider value={lookChoice}>
    <div className="map-editor" data-tool={ts.tool} data-picking={picking || undefined}>
      <Warnings warnings={warnings} onFocus={focusCell} />
      <div className="map-main">
        <Toolbar tool={ts.tool} onTool={setTool} vs={vs} mapW={geo.uw} mapH={geo.uh}
          looks={styles.length > 1 ? { options: styles.map((x) => ({ value: x.id, label: x.name || x.id })), value: lookId ?? '', onChange: setLook } : undefined} />
        {big && <p className="help">{words(T.big(map.grid.width, map.grid.height))}</p>}
        <div className="map-stage">
          <MapCanvas map={shownMap} terrain={terrain} unitTypes={unitTypes} structureTypes={structureTypes} flagged={flagged} art={art} style={look} vs={vs} geo={geo} showHeights={ts.tool === 'heights'}
            begin={(cell, o) => (vs.picking ? pickStroke(vs, vs.picking, cell) : beginStroke(env, cell, o))}
            longPress={(cell) => {
              if (vs.picking || tsRef.current.tool !== 'terrain') return false
              const tid = terrainAt(store.getState().doc.maps[id]!, cell.x, cell.y)
              if (tid) patch({ terrainId: tid, fill: false })
              return !!tid
            }}
            act={(cell) => { (vs.picking ? pickStroke(vs, vs.picking, cell) : beginStroke(env, cell)).end(cell) }}
            remove={removeAt}
            escape={() => { stopPicking(vs); vs.set({ selected: null }); setNotice(null) }} />
        </div>
        <StatusLine vs={vs} map={shownMap} terrain={terrain} structureTypes={structureTypes} unitTypes={unitTypes} notice={notice} />
        <p className="help map-keys">{T.shortcuts}</p>
        <p className="help map-view-note">{T.mapViewNote}</p>
      </div>
      <OptionsPanel doc={doc} map={map} mapId={id} state={ts} selected={selected} onChange={patch} say={setNotice} vs={vs} />
      {removeDialog}
    </div>
    </LookContext.Provider>
  )
}

