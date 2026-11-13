// Small "pick one" controls used by the tool panels: terrain swatches, side chips, unit type buttons.
import type { MapSide, Terrain, Unit } from '../../../../model/types'
import { glyphOfType, parseHex, readableOn } from '../drawMap'

const rgb = (c: string | undefined, fallback?: [number, number, number]) => `rgb(${parseHex(c, fallback).join(',')})`
const ink = (c: string | undefined) => `rgb(${readableOn(parseHex(c)).join(',')})`

export function TerrainPicker({ terrain, value, onChange }: { terrain: Terrain[]; value: string; onChange: (id: string) => void }) {
  return (
    <div className="pick-grid" role="radiogroup" aria-label="Terrain">
      {terrain.map((t) => (
        <button key={t.id} type="button" role="radio" aria-checked={t.id === value} className="pick" onClick={() => onChange(t.id)} title={t.name}>
          <span className="pick-chip" style={{ background: rgb(t.color), color: ink(t.color) }} aria-hidden="true">{t.glyph?.trim()}</span>
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

export function UnitTypePicker({ types, value, color, onChange }: { types: Unit[]; value: string; color: string | undefined; onChange: (id: string) => void }) {
  return (
    <div className="pick-grid" role="radiogroup" aria-label="Unit type">
      {types.map((u) => (
        <button key={u.id} type="button" role="radio" aria-checked={u.id === value} className="pick" onClick={() => onChange(u.id)} title={u.name}>
          <span className="pick-chip unit" style={{ background: rgb(color, [136, 136, 136]), color: ink(color) }} aria-hidden="true">{glyphOfType(u, '?')}</span>
          <span className="pick-name">{u.name}</span>
        </button>
      ))}
    </div>
  )
}
