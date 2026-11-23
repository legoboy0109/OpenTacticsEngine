// Two more choices of the "Battle animations" default (docs/BATTLE_SCENE.md): whose square decides the backdrop and platform, and which
// side of the scene each fighter stands on. Shown with the scene's default; both are presentation only.
import { sceneChoiceProblems } from '../../model/battleSceneChecks'
import { SCENE_SIDES, backgroundFrom, platformFrom, quotesPerFight, sceneSides, sceneStaves, setBackgroundFrom, setPlatformFrom, setQuotesPerFight, setSceneSides, setSceneStaves, type BackgroundFrom, type PlatformFrom, type QuotesPerFight, type SceneSides } from '../../model/battleTables'
import { useDoc, useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { Toggle } from '../../ui/Toggle'
import { BC } from './battleSceneText'

export function BattleSceneChoices() {
  const doc = useDoc()
  const edit = useEdit()
  const from = backgroundFrom(doc)
  const sides = sceneSides(doc)
  return (
    <div className="stack tight" data-testid="battle-scene-choices">
      <Field label={BC.from} help={BC.fromHelp}>
        <Select aria-label={BC.from} value={from} options={[{ value: 'defender', label: BC.fromDefender }, { value: 'attacker', label: BC.fromAttacker }]}
          onChange={(v) => edit((d) => setBackgroundFrom(d, v as BackgroundFrom))} />
      </Field>
      <Field label={BC.platformFrom} help={BC.platformFromHelp}>
        <Select aria-label={BC.platformFrom} value={platformFrom(doc)} options={[{ value: 'same', label: BC.platformSame }, { value: 'own', label: BC.platformOwn }]}
          onChange={(v) => edit((d) => setPlatformFrom(d, v as PlatformFrom))} />
      </Field>
      <Field label={BC.sides} help={BC.sidesHelp}>
        <Select aria-label={BC.sides} value={sides} options={SCENE_SIDES.map((s) => ({ value: s, label: BC.sidesChoices[s] }))}
          onChange={(v) => edit((d) => setSceneSides(d, v as SceneSides))} />
      </Field>
      <p className="help">{BC.sidesChoicesHelp[sides]}</p>
      <Field label={BC.quotes} help={BC.quotesHelp}>
        <Select aria-label={BC.quotes} value={quotesPerFight(doc)} options={[{ value: 'both', label: BC.quotesBoth }, { value: 'one', label: BC.quotesOne }]}
          onChange={(v) => edit((d) => setQuotesPerFight(d, v as QuotesPerFight))} />
      </Field>
      <Toggle label={BC.staves} help={BC.stavesHelp} checked={sceneStaves(doc)} onChange={(on) => edit((d) => setSceneStaves(d, on))} />
      {sceneChoiceProblems(doc).map((p) => <p key={p} className="help" role="alert">{p}</p>)}
    </div>
  )
}
