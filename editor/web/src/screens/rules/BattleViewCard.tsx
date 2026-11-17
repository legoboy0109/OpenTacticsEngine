import { battleSceneMode, setBattleSceneMode, type BattleSceneMode } from '../../model/battleArt'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { T } from './text'

const MODES: BattleSceneMode[] = ['map', 'scene']

/** The game's default for showing fights: quick on the map, or the side-view battle scene (docs/BATTLE_SCENE.md). */
export function BattleViewCard() {
  const mode = battleSceneMode(useDoc())
  const edit = useEdit()
  return (
    <Card title={T.battleView} help={T.battleViewHelp} id="battle-view">
      <div className="kind-row" role="radiogroup" aria-label={T.battleView}>
        {MODES.map((m) => (
          <button key={m} type="button" role="radio" aria-checked={mode === m} className="pick pick-inline" onClick={() => edit((d) => setBattleSceneMode(d, m))}>
            <span className="pick-name">{T.battleViewChoices[m].label}</span>
          </button>
        ))}
      </div>
      <p className="help">{T.battleViewChoices[mode].help}</p>
    </Card>
  )
}
