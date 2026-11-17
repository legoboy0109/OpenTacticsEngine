// "Doors, chests and places" card on the Rules screen: rules.places.actions in game.json (docs/SRPG.md section 10). A separate, self-contained card.
import { placeActions, setPlaceActions } from '../../model/placeRules'
import type { PlaceActions } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { PR } from './placesRuleText'

export function PlacesRuleCard() {
  const doc = useDoc()
  const edit = useEdit()
  const mode = placeActions(doc)
  return (
    <Card title={PR.title} help={PR.help}>
      <div className="stack" data-testid="places-rule">
        <Field label={PR.actions} help={PR.actionsHelp}>
          <Select value={mode} onChange={(v) => edit((d) => setPlaceActions(d, v as PlaceActions))}
            options={[{ value: 'menu', label: PR.menu }, { value: 'auto', label: PR.auto }]} />
        </Field>
        <p className="help" data-testid="places-rule-explain">{mode === 'auto' ? PR.autoExplain : PR.menuExplain}</p>
      </div>
    </Card>
  )
}
