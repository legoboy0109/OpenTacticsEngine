// "Stage a fight" (docs/EVENTS.md "A staged fight"): who fights whom, optionally the weapons, the strikes written down one by one,
// and what the fight leaves out or adds (nobody dies, experience, wear).
import { MAX_FIGHT_STRIKES, STRIKE_RESULTS, addStrike, fightOf, fightProblems, moveStrike, patchStrike, removeStrike, setFightFlag, setFightWeapon, setScripted, type StrikeResult } from '../../../../../model/storyFight'
import { Field } from '../../../../../ui/Field'
import { NumberInput } from '../../../../../ui/NumberInput'
import { RowButtons } from '../../../../../ui/RowButtons'
import { Select } from '../../../../../ui/Select'
import { Check } from '../../../../../ui/Toggle'
import { PickSelect, UnitPick } from '../pickers'
import { useStory } from '../StoryContext'
import { useCommandEdit, type CommandRef } from './useCommandEdit'
import { FX } from './fightText'
import type { Command } from '../../../../../model/storyTypes'

export function FightForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc, units } = useStory()
  const { patch } = useCommandEdit(at)
  const f = fightOf(cmd)
  const strikes = Array.isArray(f.strikes) ? f.strikes : null
  const write = (next: typeof f, k?: string) => patch({ fight: next }, k)
  const weapons = (doc.weapons ?? []).map((w) => ({ value: w.id, label: w.name || w.id }))
  const problems = fightProblems(doc, cmd, units.map((u) => u.id))
  return (
    <div className="stack tight" data-testid="fight-form">
      <UnitPick label={FX.attacker} help={FX.attackerHelp} value={f.attacker} onChange={(v) => write({ ...f, attacker: v })} />
      <UnitPick label={FX.defender} value={f.defender} onChange={(v) => write({ ...f, defender: v })} />
      <PickSelect label={FX.weaponAttacker} help={FX.weaponHelp} value={f.weapon?.attacker ?? ''} options={[{ value: '', label: FX.equipped }, ...weapons]}
        onChange={(v) => write(setFightWeapon(f, 'attacker', v))} />
      <PickSelect label={FX.weaponDefender} value={f.weapon?.defender ?? ''} options={[{ value: '', label: FX.equipped }, ...weapons]}
        onChange={(v) => write(setFightWeapon(f, 'defender', v))} />
      <Check label={FX.scripted} help={FX.scriptedHelp} checked={strikes !== null} onChange={(v) => write(setScripted(f, v))} />
      {strikes && (
        <div className="stack tight" data-testid="fight-strikes">
          {strikes.map((s, i) => (
            <div key={i} className="row" data-testid={`fight-strike-${i + 1}`}>
              <span className="help">{i + 1}.</span>
              <Select aria-label={FX.strikeBy(i + 1)} value={s.by} options={[{ value: 'attacker', label: FX.byAttacker }, { value: 'defender', label: FX.byDefender }]}
                onChange={(v) => write(patchStrike(f, i, { by: v as 'attacker' | 'defender' }))} />
              <Select aria-label={FX.strikeResult(i + 1)} value={s.result} options={STRIKE_RESULTS.map((r) => ({ value: r, label: FX.results[r] }))}
                onChange={(v) => write(patchStrike(f, i, { result: v as StrikeResult }))} />
              {s.result !== 'miss' && (
                <Field label={FX.damage}>
                  <NumberInput value={s.damage} allowEmpty min={0} max={9999} placeholder={FX.damageNormal}
                    onChange={(v) => write(patchStrike(f, i, { damage: v }), `strike${i}`)} />
                </Field>
              )}
              <RowButtons index={i} count={strikes.length} up={FX.up(i + 1)} down={FX.down(i + 1)} del={FX.remove(i + 1)} canDelete={strikes.length > 1}
                onUp={() => write(moveStrike(f, i, i - 1))} onDown={() => write(moveStrike(f, i, i + 1))} onDelete={() => write(removeStrike(f, i))} />
            </div>
          ))}
          <button type="button" className="btn sm" disabled={strikes.length >= MAX_FIGHT_STRIKES} onClick={() => write(addStrike(f))}>{FX.addStrike}</button>
          <p className="help">{FX.strikesHelp}</p>
        </div>
      )}
      <Check label={FX.spare} help={FX.spareHelp} checked={f.lethal === false} onChange={(v) => write(setFightFlag(f, 'lethal', !v))} />
      <Check label={FX.xp} help={FX.xpHelp} checked={f.xp === true} onChange={(v) => write(setFightFlag(f, 'xp', v))} />
      <Check label={FX.wear} help={FX.wearHelp} checked={f.wear === true} onChange={(v) => write(setFightFlag(f, 'wear', v))} />
      {problems.map((p) => <p key={p} className="help" role="alert">{p}</p>)}
      <p className="help">{FX.help}</p>
    </div>
  )
}
