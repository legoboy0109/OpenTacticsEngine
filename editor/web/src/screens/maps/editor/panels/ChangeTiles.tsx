// The tiles of one map change (docs/GRAPHICS.md "Tile layer", docs/EVENTS.md "Changing the map"): "Paint tiles into this change" (the Tiles tool then
// writes into the change instead of the map), "Also record the tiles of these squares", forgetting them, and "Keep the tiles". Only for maps with a
// tile layer; this panel holds its own palette so the Changes tab need not be left to choose a tile.
import { cellsOf, changeTileCount, clearChangeTiles, changesOf, recordChangeTiles, setChangeKeepTile } from '../../../../model/mapChanges'
import { editMap } from '../../../../model/mapCtx'
import { findSheet } from '../../../../model/graphics'
import { tileLayerOf } from '../../../../model/tiles'
import { findTileset } from '../../../../model/tilesets'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit, useStore } from '../../../../store/context'
import { Toggle } from '../../../../ui/Toggle'
import { TL } from '../tilesText'
import type { ToolState } from '../tools'
import { useViewSlice, type MapViewState } from '../viewState'
import { TilePalette, startPaletteZoom } from './TilePalette'

interface Props { doc: ProjectDoc; map: MapFile; mapId: string; change: string; vs: MapViewState; state: ToolState; onChange: (p: Partial<ToolState>) => void }

export function ChangeTiles({ doc, map, mapId, change, vs, state, onChange }: Props) {
  const edit = useEdit()
  const store = useStore()
  const layer = tileLayerOf(map)
  const paint = useViewSlice(vs, (s) => s.paintChange)
  if (!layer) return null
  const ts = findTileset(doc, layer.tileset)
  const sheet = ts ? findSheet(doc, ts.sheet) : undefined
  const info = sheet ? store.art.fileInfo(sheet.file) : undefined
  const cols = sheet && info?.width ? Math.floor(info.width / Math.max(1, sheet.cellWidth)) : 0
  const rows = sheet && info?.height ? Math.floor(info.height / Math.max(1, sheet.cellHeight)) : 0
  const ch = changesOf(map)[change]
  const into = paint === change && state.tool === 'tiles'
  const on = (v: boolean) => {
    vs.set({ paintChange: v ? change : null })
    if (v) onChange({ tool: 'tiles', tileTileset: layer.tileset })
  }
  return (
    <div className="stack tight" data-testid="change-tiles">
      <h4>{TL.changeTilesTitle}</h4>
      <p className="help">{TL.changeTilesHelp}</p>
      <Toggle checked={into} onChange={on} label={TL.changeTilesPaint} help={TL.changeTilesPaintHelp} />
      {into && cols > 0 && (
        <TilePalette doc={doc} tileset={layer.tileset} sel={state.tileBlock ? state.tileSel ?? null : null} zoom={state.tileZoom ?? startPaletteZoom(sheet!.cellWidth, cols, rows, sheet!.cellHeight)} onZoom={(z) => onChange({ tileZoom: z })}
          onChoose={(s, tiles) => onChange({ tileSel: s, tileBlock: { w: s.w, h: s.h, tiles }, tileMode: state.tileMode === 'pick' || state.tileMode === 'erase' ? 'paint' : state.tileMode })} />
      )}
      <p className="help" data-testid="change-tile-count">{TL.changeTileCount(changeTileCount(ch))}</p>
      <div className="row">
        <button type="button" className="btn sm" data-testid="change-record-tiles" disabled={cellsOf(ch).length === 0} title={TL.changeRecordHelp} onClick={() => edit((d) => editMap(d, mapId, (m) => recordChangeTiles(m, change)))}>{TL.changeRecord}</button>
        <button type="button" className="btn sm" data-testid="change-forget-tiles" disabled={changeTileCount(ch) === 0 && !ch?.keepTile} onClick={() => edit((d) => editMap(d, mapId, (m) => clearChangeTiles(m, change)))}>{TL.changeForget}</button>
      </div>
      <Toggle checked={ch?.keepTile === true} onChange={(v) => edit((d) => editMap(d, mapId, (m) => setChangeKeepTile(m, change, v)))} label={TL.changeKeep} help={TL.changeKeepHelp} />
    </div>
  )
}
