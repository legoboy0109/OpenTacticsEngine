import { ATTACK_WEAPON_CHOICES, MENU_UNAVAILABLE, attackWeaponChoice, menuOptionProblems, menuUnavailable, setAttackWeaponChoice, setMenuUnavailable } from '../../model/menuOptions'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { MO } from './menuOptionsText'

/** The player's action menu: what happens to actions with nothing to act on, and whether Attack asks for the weapon (docs/SRPG.md "Menu options"). */
export function MenuOptionsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const unavailable = menuUnavailable(doc)
  const attack = attackWeaponChoice(doc)
  return (
    <Card title={MO.title} help={MO.help} id="menu-options">
      <div className="stack tight" data-testid="menu-options">
        <div className="kind-row" role="radiogroup" aria-label={MO.unavailable}>
          {MENU_UNAVAILABLE.map((m) => (
            <button key={m} type="button" role="radio" aria-checked={unavailable === m} className="pick pick-inline" onClick={() => edit((d) => setMenuUnavailable(d, m))}>
              <span className="pick-name">{MO.unavailableChoices[m].label}</span>
            </button>
          ))}
        </div>
        <p className="help"><strong>{MO.unavailable}.</strong> {MO.unavailableChoices[unavailable].help}</p>
        <div className="kind-row" role="radiogroup" aria-label={MO.attack}>
          {ATTACK_WEAPON_CHOICES.map((m) => (
            <button key={m} type="button" role="radio" aria-checked={attack === m} className="pick pick-inline" onClick={() => edit((d) => setAttackWeaponChoice(d, m))}>
              <span className="pick-name">{MO.attackChoices[m].label}</span>
            </button>
          ))}
        </div>
        <p className="help"><strong>{MO.attack}.</strong> {MO.attackChoices[attack].help}</p>
        {menuOptionProblems(doc).map((p) => <p key={p} className="help" role="alert">{p}</p>)}
      </div>
    </Card>
  )
}
