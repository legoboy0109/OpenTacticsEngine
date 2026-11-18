// "Tilesets": images whose pieces are the tiles of hand-painted maps (docs/GRAPHICS.md "Tile layer"). Each tileset has a name, an image,
// a numbered tile grid and, for tiles that cycle (water, lava), a list of frames and a time per frame. Deleting one that maps use asks
// first; their tile layers go with it in the same undo step.
import { useState } from 'react'
import { useArtState } from '../../art/useArt'
import { artBase } from '../../model/artFiles'
import { findSheet, sheetsOf } from '../../model/graphics'
import { tidyId } from '../../model/storyIds'
import {
  addTileset, animationOf, findTileset, markAnimated, moveTileset, parseFrames, removeTileset, renameTileset, setAnimation, setTilesetSheet,
  sheetTiles, TILE_ANIM_DEFAULT_MS, TILE_ANIM_MAX_FRAMES, TILE_ANIM_MAX_MS, TILE_ANIM_MIN_MS, tilesetIdProblem, tilesetList, tilesetUsageLines, unmarkAnimated,
} from '../../model/tilesets'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Card } from '../../ui/Card'
import { ConfirmDialog } from '../../ui/Dialog'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { IdInput } from '../../ui/IdInput'
import { PieceGrid } from '../../ui/picture/PieceGrid'
import { RowButtons } from '../../ui/RowButtons'
import { Select } from '../../ui/Select'
import { Toggle } from '../../ui/Toggle'
import { T } from './text'

/** The animation of the selected tile: frames text and milliseconds; only valid input reaches the document. */
function AnimationFields({ id, tile, count }: { id: string; tile: number; count: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const anim = animationOf(findTileset(doc, id), tile)
  const [text, setText] = useState(anim?.frames.join(', ') ?? '')
  const [ms, setMs] = useState(String(anim?.ms ?? TILE_ANIM_DEFAULT_MS))
  const frames = parseFrames(text)
  const framesOk = !!frames && frames.length >= 2 && frames.length <= TILE_ANIM_MAX_FRAMES && (count === 0 || frames.every((f) => f < count))
  const msN = /^\d+$/.test(ms) ? Number(ms) : NaN
  const msOk = msN >= TILE_ANIM_MIN_MS && msN <= TILE_ANIM_MAX_MS
  if (!anim) return null
  return (
    <div className="stack tight">
      <Field label={T.tilesetFrames} help={T.tilesetFramesHelp}>
        <input className="input" data-testid="tile-frames" value={text} autoComplete="off" spellCheck={false}
          onChange={(e) => {
            setText(e.target.value)
            const f = parseFrames(e.target.value)
            if (f && f.length >= 2 && f.length <= TILE_ANIM_MAX_FRAMES && (count === 0 || f.every((n) => n < count))) edit((d) => setAnimation(d, id, tile, f, anim.ms), `tileanim:${id}:${tile}:frames`)
          }} />
      </Field>
      {!framesOk && <p className="error-text" role="alert">{T.tilesetFramesBad(Math.max(0, count - 1))}</p>}
      <Field label={T.tilesetMs}>
        <input className="input" type="number" min={TILE_ANIM_MIN_MS} max={TILE_ANIM_MAX_MS} data-testid="tile-ms" value={ms}
          onChange={(e) => {
            setMs(e.target.value)
            const n = /^\d+$/.test(e.target.value) ? Number(e.target.value) : NaN
            if (n >= TILE_ANIM_MIN_MS && n <= TILE_ANIM_MAX_MS) edit((d) => setAnimation(d, id, tile, anim.frames, n), `tileanim:${id}:${tile}:ms`)
          }} />
      </Field>
      {!msOk && <p className="error-text" role="alert">{T.tilesetMsBad}</p>}
    </div>
  )
}

function TilesetRow({ id, index, total, onDelete }: { id: string; index: number; total: number; onDelete: () => void }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  useArtState()
  const ts = findTileset(doc, id)!
  const sheets = sheetsOf(doc)
  const sheet = findSheet(doc, ts.sheet)
  const img = sheet ? store.art.image(sheet.file) : null
  const { cols, rows, count } = sheetTiles(sheet, img?.status === 'ready' ? img.width : 0, img?.status === 'ready' ? img.height : 0)
  const [tile, setTile] = useState<number | null>(null)
  const anims = Object.keys(ts.animations ?? {}).map(Number)
  const here = tile !== null && tile < count ? tile : null
  const isAnim = here !== null && !!animationOf(ts, here)
  const outside = count > 0 ? anims.filter((a) => a >= count || (ts.animations![String(a)].frames ?? []).some((f) => f >= count)) : []
  const marks = [
    ...anims.filter((a) => a < count).map((a) => ({ col: a % cols, row: Math.floor(a / cols), label: '~' })),
    ...(here !== null && !isAnim ? [{ col: here % cols, row: Math.floor(here / cols) }] : []),
  ]
  return (
    <li className="portrait-row" data-testid={`tileset-${id}`} style={{ display: 'block' }}>
      <div className="stack tight">
        <div className="row wrap">
          <div style={{ flex: '1 1 140px', minWidth: 0 }}>
            <IdInput aria-label={T.tilesetName(index + 1)} value={id} validate={(t) => tilesetIdProblem(doc, t, id)} onChange={(v) => edit((d) => renameTileset(d, id, v), `tileset:${id}:name`)} />
          </div>
          <RowButtons index={index} count={total} up={T.tilesetUp(id)} down={T.tilesetDown(id)} del={T.tilesetDelete(id)}
            onUp={() => edit((d) => moveTileset(d, index, index - 1))} onDown={() => edit((d) => moveTileset(d, index, index + 1))} onDelete={onDelete} />
        </div>
        <Field label={T.tilesetImage}>
          <Select value={sheet ? ts.sheet : ''} placeholder={sheet ? undefined : ts.sheet} options={sheets.map((s) => ({ value: s.id, label: s.name || artBase(s.file) }))}
            onChange={(v) => { setTile(null); edit((d) => setTilesetSheet(d, id, v)) }} />
        </Field>
        {!sheet && <p className="error-text" role="alert">{T.tilesetSheetMissing}</p>}
        {outside.map((a) => <p key={a} className="error-text" role="alert">{T.tilesetAnimOutside(a, count - 1)}</p>)}
        {sheet && img?.status === 'loading' && <p className="help" role="status">{T.tilesetLoading}</p>}
        {sheet && img?.status === 'ready' && count > 0 && (
          <>
            <p className="help">{T.tilesetCount(count, cols, rows)}</p>
            <PieceGrid key={sheet.id + sheet.file + sheet.cellWidth + sheet.cellHeight} image={img.img} width={img.width} height={img.height} cellWidth={sheet.cellWidth} cellHeight={sheet.cellHeight}
              marks={marks} onPick={(c, r) => setTile(r * cols + c)} label={T.tilesetGridLabel(id)} />
            <p className="help" data-testid={`tileset-${id}-selected`}>{here === null ? T.tilesetNoTile : T.tilesetTile(here)}</p>
            {/* The grid is one canvas; the number box makes every tile reachable by number too (keyboard, screen readers). */}
            <Field label={T.tilesetPickNumber}>
              <input className="input" type="number" min={0} max={count - 1} data-testid={`tileset-${id}-tile-input`} value={here ?? ''}
                onChange={(e) => { const n = Number(e.target.value); if (e.target.value !== '' && Number.isInteger(n) && n >= 0 && n < count) setTile(n) }} />
            </Field>
            {here !== null && (
              <>
                <Toggle label={T.tilesetAnimated} help={T.tilesetAnimatedHelp} checked={isAnim}
                  onChange={(v) => edit((d) => (v ? markAnimated(d, id, here, count) : unmarkAnimated(d, id, here)))} />
                {isAnim && <AnimationFields key={`${id}:${here}`} id={id} tile={here} count={count} />}
              </>
            )}
            {anims.length > 0 && (
              <p className="help" data-testid={`tileset-${id}-animated`}>{T.tilesetAnimatedList} {anims.join(', ')}</p>
            )}
          </>
        )}
      </div>
    </li>
  )
}

export function TilesetsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = tilesetList(doc)
  const hasImages = sheetsOf(doc).length > 0
  const [removing, setRemoving] = useState<string | null>(null)
  const [text, setText] = useState('')
  const problem = text === '' ? null : tilesetIdProblem(doc, text)
  const add = (id: string) => { if (!tilesetIdProblem(doc, id)) edit((d) => addTileset(d, id)) }
  const uses = removing ? tilesetUsageLines(doc, [removing]) : []
  const remove = (id: string) => { edit((d) => removeTileset(d, id)); setRemoving(null) }
  return (
    <Card title={T.tilesetsTitle} help={T.tilesetsHelp} id="tilesets">
      <div className="stack tight" data-testid="tilesets">
        {!hasImages && <p className="help">{T.tilesetsNeedImage}</p>}
        {hasImages && list.length === 0 && <p className="help">{T.tilesetsNone}</p>}
        <ul className="portrait-list" aria-label={T.tilesetsTitle}>
          {list.map((t, i) => (
            <TilesetRow key={t.id} id={t.id} index={i} total={list.length}
              onDelete={() => { if (tilesetUsageLines(doc, [t.id]).length) setRemoving(t.id); else remove(t.id) }} />
          ))}
        </ul>
        {hasImages && (
          <>
            <div className="add-row">
              <input className="input" aria-label={T.tilesetNewName} placeholder={T.tilesetNewName} value={text} autoComplete="off" spellCheck={false}
                onChange={(e) => setText(tidyId(e.target.value))}
                onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); if (text && !problem) { add(text); setText('') } } }} />
              <button type="button" className="btn sm" disabled={!text || !!problem} onClick={() => { add(text); setText('') }}><Icon name="plus" />{T.tilesetAdd}</button>
            </div>
            {problem && <p className="error-text" role="alert">{problem}</p>}
          </>
        )}
      </div>
      <ConfirmDialog open={!!removing} title={T.tilesetDeleteTitle(removing ?? '')} confirmLabel={T.tilesetDeleteConfirm} danger
        onConfirm={() => removing && remove(removing)} onCancel={() => setRemoving(null)}>
        <p>{T.tilesetDeleteIntro}</p>
        <ul className="usage-list">{uses.map((u) => <li key={u.mapId}>{T.tilesetUsedMap(u.id, u.mapName)}</li>)}</ul>
      </ConfirmDialog>
    </Card>
  )
}
