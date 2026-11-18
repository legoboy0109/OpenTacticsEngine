// The Tiles tool's panel (docs/EDITOR_UI.md "Tiles tool"): the map's tileset, the palette of its tiles, what a click does (paint, erase, rectangle,
// pick) and the "show terrain colours" aid. Clicking or dragging on the map does the work (tileStrokes.ts).
import { useEffect } from 'react'
import { editMap } from '../../../../model/mapCtx'
import { gridOfGame, isHex } from '../../../../model/gridSpec'
import { setLayerTileset, tileCount, tileLayerOf } from '../../../../model/tiles'
import { stripMapTiles, tilesetList } from '../../../../model/tilesets'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit, useStore } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { Select } from '../../../../ui/Select'
import { Toggle } from '../../../../ui/Toggle'
import { useViewSlice, type MapViewState } from '../viewState'
import { TilePalette, startPaletteZoom } from './TilePalette'
import type { ToolState } from '../tools'
import { TL } from '../tilesText'
import { findSheet } from '../../../../model/graphics'

const MODES = ['paint', 'erase', 'rect', 'pick'] as const

interface Props { doc: ProjectDoc; map: MapFile; mapId: string; state: ToolState; vs: MapViewState; onChange: (p: Partial<ToolState>) => void }

export function TilesTool({ doc, map, mapId, state, vs, onChange }: Props) {
  const edit = useEdit()
  const store = useStore()
  const hex = isHex(gridOfGame(doc.game))
  const list = tilesetList(doc)
  const layer = tileLayerOf(map)
  const into = useViewSlice(vs, (s) => s.paintChange)
  // The tileset in use: the layer's, else the remembered one, else the first.
  const tileset = layer && list.some((t) => t.id === layer.tileset) ? layer.tileset : list.some((t) => t.id === state.tileTileset) ? state.tileTileset! : list[0]?.id ?? ''
  const ts = list.find((t) => t.id === tileset)?.tileset
  const sheet = ts ? findSheet(doc, ts.sheet) : undefined
  const info = sheet ? store.art.fileInfo(sheet.file) : undefined
  const cols = sheet && info?.width ? Math.floor(info.width / Math.max(1, sheet.cellWidth)) : 0
  const rows = sheet && info?.height ? Math.floor(info.height / Math.max(1, sheet.cellHeight)) : 0
  const zoom = state.tileZoom ?? (cols > 0 ? startPaletteZoom(sheet!.cellWidth, cols, rows, sheet!.cellHeight) : 2)
  const mode = state.tileMode ?? 'paint'
  // The chosen block must lie inside this tileset.
  const count = cols * rows
  const block = state.tileBlock && state.tileBlock.tiles.length > 0 && (count === 0 || state.tileBlock.tiles.every((n) => n < count)) ? state.tileBlock : null
  const sel = block ? state.tileSel ?? null : null

  // A tileset remembered for a new layer that no longer exists falls back; remember the one in use.
  useEffect(() => { if (tileset && state.tileTileset !== tileset) onChange({ tileTileset: tileset }) }, [tileset, state.tileTileset, onChange])

  if (hex) return <div className="stack tight" data-testid="tiles-tool"><h3>{TL.title}</h3><p className="help">{TL.hexNote}</p></div>
  if (list.length === 0) return <div className="stack tight" data-testid="tiles-tool"><h3>{TL.title}</h3><p className="help">{TL.noTilesets}</p></div>

  const chooseTileset = (id: string) => {
    if (layer) edit((d) => editMap(d, mapId, (m) => setLayerTileset(m, id)))
    onChange({ tileTileset: id, tileBlock: undefined, tileSel: undefined })
  }
  const squares = tileCount(map)
  return (
    <div className="stack tight" data-testid="tiles-tool">
      <h3>{TL.title}</h3>
      <p className="help">{TL.help}</p>
      {into && <p className="help" data-testid="tiles-into-change">{TL.intoChange}</p>}
      <Field label={TL.tileset} help={layer ? TL.tilesetHelp : TL.startLayerHelp}>
        <Select aria-label={TL.tileset} value={tileset} options={list.map((t) => ({ value: t.id, label: t.id }))} onChange={chooseTileset} />
      </Field>
      <div className="kind-row" role="radiogroup" aria-label={TL.modeLabel}>
        {MODES.map((m) => (
          <button key={m} type="button" role="radio" aria-checked={mode === m} className="pick pick-inline" data-testid={`tile-mode-${m}`} title={TL.modeHelp[m]} onClick={() => onChange({ tileMode: m })}>
            <span className="pick-name">{TL.modes[m]}</span>
          </button>
        ))}
      </div>
      <p className="help">{TL.modeHelp[mode]}</p>
      <h4>{TL.palette}</h4>
      <TilePalette doc={doc} tileset={tileset} sel={sel} zoom={zoom} onZoom={(z) => onChange({ tileZoom: z })}
        onChoose={(s, tiles) => onChange({ tileSel: s, tileBlock: { w: s.w, h: s.h, tiles }, tileMode: mode === 'pick' || mode === 'erase' ? 'paint' : mode })} />
      <p className="help" data-testid="tile-chosen">{block ? (block.w * block.h === 1 ? TL.chosen(block.tiles[0], block.tiles[0] % Math.max(1, cols), Math.floor(block.tiles[0] / Math.max(1, cols))) : TL.chosenBlock(block.w, block.h, block.tiles[0])) : TL.noneChosen}</p>
      <Toggle checked={!!state.tileTerrain} onChange={(v) => onChange({ tileTerrain: v })} label={TL.showTerrain} help={TL.showTerrainHelp} />
      {layer && (
        <>
          <p className="help" data-testid="tile-layer-count">{TL.layerCount(squares)}</p>
          <button type="button" className="btn sm danger" data-testid="tile-layer-remove" title={TL.removeLayerHelp} onClick={() => edit((d) => editMap(d, mapId, (m) => stripMapTiles(m)))}>{TL.removeLayer}</button>
        </>
      )}
    </div>
  )
}
