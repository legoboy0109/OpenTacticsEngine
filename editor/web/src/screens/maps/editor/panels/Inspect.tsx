// The Select tool's panel: what a square is, with the ground AND the structure on it, and what that does to movement and
// combat numbers (the engine's combination rule, model/structuresEffective.ts).
import { ctxOf } from '../../../../model/mapCtx'
import { terrainAt, type Cell } from '../../../../model/mapGrid'
import { sideOf } from '../../../../model/mapSides'
import { structureAt } from '../../../../model/mapStructures'
import { unitAt } from '../../../../model/mapUnits'
import { sizeOf } from '../../../../model/structures'
import { effectiveNumbers, enterCost } from '../../../../model/structuresEffective'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { T } from '../text'
import { UnitProps } from './UnitProps'

export function Inspect({ doc, map, mapId, cell, say }: { doc: ProjectDoc; map: MapFile; mapId: string; cell: Cell | null; say: (m: string | null) => void }) {
  if (!cell) return <div className="stack tight"><h3>{T.selectTitle}</h3><p className="help">{T.selectHelp}</p><p className="help">{T.selectNone}</p></div>
  const ctx = ctxOf(doc)
  const t = doc.terrain?.find((x) => x.id === terrainAt(map, cell.x, cell.y))
  const placed = ctx ? structureAt(map, ctx, cell.x, cell.y) : undefined
  const s = placed ? doc.structures?.find((d) => d.id === placed.type) : undefined
  const unit = unitAt(map, cell.x, cell.y)
  const costs = doc.game?.moveTypes.map((m) => ({ name: m.name, ...enterCost(t, s, m.id) })) ?? []
  const numbers = effectiveNumbers(t, s)
  const size = sizeOf(s)
  const from = (src: 'ground' | 'structure') => (src === 'structure' ? T.fromStructure : T.fromGround)
  return (
    <div className="stack tight">
      <h3>{T.selectTitle} <span className="help inline">{T.statusSquare(cell.x, cell.y)}</span></h3>
      <dl className="facts compact">
        <div><dt>{T.terrainOf}</dt><dd>{t?.name ?? '?'}</dd></div>
        <div><dt>{T.structureOf}</dt><dd>{placed ? `${s?.name ?? placed.type}${size.w * size.h > 1 ? ` (${size.w} × ${size.h})` : ''}` : T.noStructure}</dd></div>
        {placed && <div><dt>{T.ownerOf}</dt><dd>{placed.side ? sideOf(map, placed.side)?.name ?? placed.side : T.noOwner}</dd></div>}
        <div><dt>{T.unitOf}</dt><dd>{unit ? doc.units?.find((u) => u.id === unit.type)?.name ?? unit.type : T.noUnit}</dd></div>
      </dl>
      {costs.length > 0 && (
        <p className="help" data-testid="effective-costs">{T.moveCostsTitle}: {costs.map((c) => `${c.name} ${c.cost > 0 ? c.cost : T.cannotEnter}${s && c.from === 'structure' ? ` (${from(c.from)})` : ''}`).join(', ')}</p>
      )}
      <p className="help" data-testid="effective-numbers">{T.numbersTitle}: {numbers.length ? numbers.map((n) => `${n.key} ${n.value}${s && n.from === 'structure' ? ` (${from(n.from)})` : ''}`).join(', ') : T.noNumbers}</p>
      {unit && <><hr className="rule" /><UnitProps mapId={mapId} cell={cell} say={say} /></>}
    </div>
  )
}
