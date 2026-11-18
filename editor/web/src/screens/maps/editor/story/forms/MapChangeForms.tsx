// The story commands that change the map and the fog (docs/EVENTS.md "Changing the map"): setTerrain, applyChange, revertChange, setFog,
// setVision. Wording in mapChangeText.ts.
import { changeIds } from '../../../../../model/mapChanges'
import { rectOf, type Command } from '../../../../../model/storyTypes'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { Check } from '../../../../../ui/Toggle'
import { AreaField } from '../AreaField'
import { PickSelect } from '../pickers'
import { useStory } from '../StoryContext'
import { MC } from './mapChangeText'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

/** Turns the squares of an area into one terrain. A command written with a list of squares (by hand or by another tool) keeps that list. */
export function SetTerrainForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  const { doc, map } = useStory()
  const tiled = !!map.tileLayer || cmd.tile !== undefined || cmd.keepTile !== undefined  // the tile fields need a tile layer on the map (the loader refuses them otherwise)
  const options = (doc.terrain ?? []).map((t) => ({ value: t.id, label: t.name || t.id }))
  const cells = Array.isArray(cmd.cells)
  return (
    <div className="stack tight" data-testid="set-terrain">
      <PickSelect label={MC.terrain} help={MC.terrainHelp} value={cmd.setTerrain ?? ''} options={options} onChange={(v) => patch({ setTerrain: v })} />
      {cells
        ? <p className="help">{MC.cellsList(cmd.cells!.length)} <button type="button" className="btn sm" onClick={() => patch({ cells: undefined, area: { x: 0, y: 0, w: 1, h: 1 } })}>{MC.useArea}</button></p>
        : <AreaField label={MC.area} mode="area" value={rectOf(cmd.area)} onChange={(r) => patch({ area: r })} help={MC.areaHelp} />}
      {tiled && (
        <>
          <Field label={MC.tile} help={MC.tileHelp}>
            <NumberInput aria-label={MC.tile} value={typeof cmd.tile === 'number' ? cmd.tile : undefined} allowEmpty min={-1} max={65534} placeholder={MC.tileNone}
              onChange={(v) => patch({ tile: v === undefined ? undefined : Math.round(v) }, 'tile')} />
          </Field>
          <Check label={MC.keepTile} help={MC.keepTileHelp} checked={cmd.keepTile === true} onChange={(v) => patch({ keepTile: v ? true : undefined })} />
        </>
      )}
    </div>
  )
}

/** applyChange / revertChange: which of the map's named changes. */
export function ChangeCommandForm({ cmd, at, apply }: { cmd: Command; at: CommandRef; apply: boolean }) {
  const { patch } = useCommandEdit(at)
  const { map } = useStory()
  const key = apply ? 'applyChange' : 'revertChange'
  return (
    <div className="stack tight" data-testid="change-command">
      <PickSelect label={MC.change} value={String(cmd[key] ?? '')} options={changeIds(map).map((id) => ({ value: id, label: id }))} empty={MC.noChanges} onChange={(v) => patch({ [key]: v })} />
      <p className="help">{apply ? MC.applyHelp : MC.revertHelp}</p>
    </div>
  )
}

export function SetFogForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight" data-testid="set-fog">
      <PickSelect label={MC.fog} value={cmd.setFog === true ? 'on' : 'off'} options={[{ value: 'off', label: MC.fogOff }, { value: 'on', label: MC.fogOn }]} onChange={(v) => patch({ setFog: v === 'on' })} />
      <p className="help">{MC.fogHelp}</p>
    </div>
  )
}

export function SetVisionForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { patch } = useCommandEdit(at)
  return (
    <div className="stack tight" data-testid="set-vision">
      <Field label={MC.vision} help={MC.visionHelp}>
        <NumberInput aria-label={MC.vision} value={cmd.setVision ?? 0} min={-10} max={10} onChange={(v) => { if (v !== undefined) patch({ setVision: Math.max(-10, Math.min(10, Math.round(v))) }, 'vision') }} />
      </Field>
    </div>
  )
}
