// What the Units tool places: the unit types, and in a game that has characters a second tab with the characters
// (docs/SRPG.md "Editor"). A character already on this map is greyed (a character stands on a map once).
import { placedCharacterIds } from '../../../../model/characterUnits'
import { classWord } from '../../../../model/characters'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { cap, T as C } from '../../../characters/text'
import type { ToolState } from '../tools'
import { UnitChip, UnitTypePicker } from './pickers'

interface Props { doc: ProjectDoc; map: MapFile; state: ToolState; color: string | undefined; onChange: (p: Partial<ToolState>) => void }

export function UnitPicker({ doc, map, state, color, onChange }: Props) {
  const characters = doc.characters
  const word = classWord(doc, true)
  const types = <UnitTypePicker types={doc.units ?? []} value={state.unitType} color={color} sideId={state.unitSide} label={cap(word)} onChange={(id) => onChange({ unitType: id })} />
  if (characters === null) return types
  const characterTab = state.unitTab === 'characters'
  const placed = placedCharacterIds(map)
  return (
    <div className="stack tight">
      <div className="tabs unit-tabs" role="tablist" aria-label={C.pickerLabel}>
        <button type="button" role="tab" aria-selected={!characterTab} className="tab" onClick={() => onChange({ unitTab: 'types' })}>{cap(word)}</button>
        <button type="button" role="tab" aria-selected={characterTab} className="tab" data-learn="unit-characters-tab" onClick={() => onChange({ unitTab: 'characters' })}>{C.pickerCharacters}</button>
      </div>
      {!characterTab && types}
      {characterTab && characters.length === 0 && <p className="help">{C.noCharacters}</p>}
      {characterTab && characters.length > 0 && (
        <div className="pick-grid" role="radiogroup" aria-label={C.pickerCharacters}>
          {characters.map((c) => {
            const here = placed.includes(c.id)
            const unit = doc.units?.find((u) => u.id === c.type)
            return (
              <button key={c.id} type="button" role="radio" aria-checked={c.id === state.unitCharacter} disabled={here} className="pick" title={here ? `${c.name} (${C.onMap.toLowerCase()})` : c.name}
                onClick={() => onChange({ unitCharacter: c.id })}>
                {unit && <UnitChip unit={unit} sideId={state.unitSide} color={color} />}
                <span className="pick-text">
                  <span className="pick-name">{c.name}</span>
                  {here && <span className="pick-sub">{C.onMap}</span>}
                </span>
              </button>
            )
          })}
        </div>
      )}
    </div>
  )
}
