// The small "Sounds" group on the Weapons, Units and Items forms: which sound plays for each event of this one, instead
// of the game's standard sound. Shown only when the game has sounds at all (an empty list of choices helps nobody).
import { hasSounds, overrideOf, setOverride } from '../../model/audio'
import { OVERRIDE_KEY_LABEL, OVERRIDE_KEYS, UNIT_KEY_LABEL, type OverrideOwner } from '../../model/audioLabels'
import { useDoc, useEdit } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { AudioChoice } from './AudioChoice'
import { T } from './text'

function Rows({ owner, id }: { owner: OverrideOwner; id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const labels = owner === 'units' ? UNIT_KEY_LABEL : OVERRIDE_KEY_LABEL
  return (
    <div className="stack tight" data-testid={`sounds-${owner}`}>
      <p className="help">{T.soundsGroupHelp}</p>
      <FormGrid>
        {OVERRIDE_KEYS[owner].map((key) => (
          <Field key={key} label={labels[key] ?? key}>
            <AudioChoice kind="sound" mode="game" value={overrideOf(doc, owner, id, key)} aria-label={`${T.soundsGroup}: ${labels[key] ?? key}`}
              onChange={(v) => edit((d) => setOverride(d, owner, id, key, v))} />
          </Field>
        ))}
      </FormGrid>
    </div>
  )
}

/** A card (weapons, items) or, with `collapsed`, a closed "Sounds" fold (units). */
export function SoundsGroup({ owner, id, collapsed }: { owner: OverrideOwner; id: string; collapsed?: boolean }) {
  const doc = useDoc()
  // A game with no sounds shows nothing, unless this entry already has some (they must stay visible and changeable).
  const entry = (doc[owner] as Array<{ id: string; sounds?: Record<string, string> }> | null)?.find((x) => x.id === id)
  if (!hasSounds(doc) && !(entry?.sounds && Object.keys(entry.sounds).length)) return null
  return collapsed
    ? <Advanced label={T.soundsGroup}><Rows owner={owner} id={id} /></Advanced>
    : <Card title={T.soundsGroup}><Rows owner={owner} id={id} /></Card>
}
