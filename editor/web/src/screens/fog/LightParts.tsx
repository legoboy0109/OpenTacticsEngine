// The fog / map change parts of the data screens (docs/EDITOR_UI.md "Map changes and fog options"): a structure kind's fixed light, a terrain's
// "blocks sight", a torch item's light and a breakable door's word. Wording: screens/maps/editor/story/forms/mapChangeText.ts.
import { LIGHT_MAX, setDoorWord, setItemLight, setItemLightOn, setStructureLight, setTerrainBlocksSight } from '../../model/lightParts'
import { useDoc, useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { TextInput } from '../../ui/TextInput'
import { Check } from '../../ui/Toggle'
import { MC } from '../maps/editor/story/forms/mapChangeText'

/** On a structure kind: "Gives light" with its radius (0 = none). */
export function StructureLight({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return null
  return (
    <Field label={MC.light} help={MC.lightHelp}>
      <NumberInput data-learn="structure-light" aria-label={MC.light} value={s.light ?? 0} min={0} max={LIGHT_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setStructureLight(d, id, v), `structure:${id}:light`) }} />
    </Field>
  )
}

/** On a terrain: "Blocks sight". */
export function TerrainBlocksSight({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const t = doc.terrain?.find((x) => x.id === id)
  if (!t) return null
  return <Check label={MC.blocksSightTerrain} help={MC.blocksSightTerrainHelp} checked={t.blocksSight === true} onChange={(on) => edit((d) => setTerrainBlocksSight(d, id, on))} />
}

/** On a usable item: the torch. */
export function ItemTorch({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const item = doc.items?.find((x) => x.id === id)
  if (!item) return null
  const l = item.light
  return (
    <div className="stack tight" data-testid="item-torch">
      <Check label={MC.torch} help={MC.torchHelp} checked={l !== undefined} onChange={(on) => edit((d) => setItemLightOn(d, id, on))} />
      {l && (
        <div className="indent stack tight">
          <Field label={MC.torchRadius}><NumberInput aria-label={MC.torchRadius} value={l.radius} min={1} max={LIGHT_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setItemLight(d, id, 'radius', v), `item:${id}:lightRadius`) }} /></Field>
          <Field label={MC.torchTurns}><NumberInput aria-label={MC.torchTurns} value={l.turns ?? 1} min={1} max={LIGHT_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setItemLight(d, id, 'turns', v), `item:${id}:lightTurns`) }} /></Field>
          <Field label={MC.torchShrink} help={MC.torchShrinkHelp}><NumberInput aria-label={MC.torchShrink} value={l.shrink ?? 0} min={0} max={LIGHT_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setItemLight(d, id, 'shrink', v), `item:${id}:lightShrink`) }} /></Field>
        </div>
      )}
    </div>
  )
}

/** On a door that can be broken: what the menu calls it. */
export function DoorWord({ id, word }: { id: string; word: string | undefined }) {
  const edit = useEdit()
  return (
    <>
      <Field label={MC.breakWord} help={MC.breakWordHelp}>
        <TextInput aria-label={MC.breakWord} value={word ?? ''} placeholder={MC.breakWordHint} onChange={(v) => edit((d) => setDoorWord(d, id, v), `structure:${id}:doorWord`)} />
      </Field>
      <p className="help">{MC.breakHook}</p>
    </>
  )
}
