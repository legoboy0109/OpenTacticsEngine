// Small "pick one" controls used by the tool panels: terrain swatches, side chips, unit type buttons.
import { useMemo } from 'react'
import type { MapSide, Terrain, Unit } from '../../../../model/types'
import { ArtCanvas } from '../../../../ui/picture/ArtCanvas'
import { useLook, useTerrainThumb, useUnitThumb } from '../../../../ui/picture/usePreview'
import { glyphOfType, parseHex, readableOn } from '../drawMap'

const rgb = (c: string | undefined, fallback?: [number, number, number]) => `rgb(${parseHex(c, fallback).join(',')})`
const ink = (c: string | undefined) => `rgb(${readableOn(parseHex(c)).join(',')})`

export function TerrainPicker({ terrain, value, onChange }: { terrain: Terrain[]; value: string; onChange: (id: string) => void }) {
  return (
    <div className="pick-grid" role="radiogroup" aria-label="Terrain">
      {terrain.map((t) => (
        <button key={t.id} type="button" role="radio" aria-checked={t.id === value} className="pick" onClick={() => onChange(t.id)} title={t.name}>
          <TerrainChip terrain={t} />
          <span className="pick-name">{t.name}</span>
        </button>
      ))}
    </div>
  )
}

interface SideProps { sides: MapSide[]; value: string; onChange: (id: string) => void; neutral?: string; label?: string }

/** Sides as coloured chips; `neutral` adds a "nobody" choice with value ''. */
export function SidePicker({ sides, value, onChange, neutral, label = 'Side' }: SideProps) {
  return (
    <div className="pick-row" role="radiogroup" aria-label={label}>
      {sides.map((s) => (
        <button key={s.id} type="button" role="radio" aria-checked={s.id === value} className="pick pick-inline" onClick={() => onChange(s.id)}>
          <span className="pick-chip small" style={{ background: rgb(s.color, [136, 136, 136]) }} aria-hidden="true" />
          <span className="pick-name">{s.name || s.id}</span>
        </button>
      ))}
      {neutral !== undefined && (
        <button type="button" role="radio" aria-checked={value === ''} className="pick pick-inline" onClick={() => onChange('')}>
          <span className="pick-chip small none" aria-hidden="true" />
          <span className="pick-name">{neutral}</span>
        </button>
      )}
    </div>
  )
}

export function UnitTypePicker({ types, value, color, sideId, onChange }: { types: Unit[]; value: string; color: string | undefined; sideId?: string; onChange: (id: string) => void }) {
  return (
    <div className="pick-grid" role="radiogroup" aria-label="Unit type">
      {types.map((u) => (
        <button key={u.id} type="button" role="radio" aria-checked={u.id === value} className="pick" onClick={() => onChange(u.id)} title={u.name}>
          <UnitChip unit={u} sideId={sideId} color={color} />
          <span className="pick-name">{u.name}</span>
        </button>
      ))}
    </div>
  )
}

/** The terrain's picture when it has one, else the coloured chip with its letter. */
function TerrainChip({ terrain: t }: { terrain: Terrain }) {
  const look = useLook()
  const spec = useTerrainThumb(t.id, look)
  if (spec.scene.art?.terrain.has(t.id)) return <span className="pick-chip art" aria-hidden="true"><ArtCanvas spec={spec} label="" /></span>
  return <span className="pick-chip" style={{ background: rgb(t.color), color: ink(t.color) }} aria-hidden="true">{t.glyph?.trim()}</span>
}

function UnitChip({ unit, sideId, color }: { unit: Unit; sideId?: string; color: string | undefined }) {
  const look = useLook()
  const side = useMemo(() => ({ id: sideId ?? 'preview', name: '', color: color ?? '#888888' }), [sideId, color])
  const spec = useUnitThumb(unit.id, side, look)
  if (spec.scene.art?.units.has(unit.id)) return <span className="pick-chip art unit" aria-hidden="true"><ArtCanvas spec={spec} label="" /></span>
  return <span className="pick-chip unit" style={{ background: rgb(color, [136, 136, 136]), color: ink(color) }} aria-hidden="true">{glyphOfType(unit, '?')}</span>
}
