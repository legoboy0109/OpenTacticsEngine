// Rules card "Promotion" (E3 of docs/SRPG.md): after promoting, the level starts again at 1 or stays. Edits `rules.promotion`
// of game.json ("starts again" is the engine's default and is not written).
import { gameClassSkills, gameLevelAfter, setClassSkillsRule, setLevelAfterRule, type ClassSkills, type LevelAfter } from '../../../model/promotion'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Select } from '../../../ui/Select'
import { C } from './carryText'

export function PromotionRulesCard() {
  const doc = useDoc()
  const edit = useEdit()
  return (
    <Card title={C.promoTitle} help={C.promoHelp} id="promotion-rules">
      <div data-testid="promotion-rules">
        <Field label={C.levelAfter} help={C.levelHelp}>
          <Select value={gameLevelAfter(doc)} options={[{ value: 'reset', label: C.levelReset }, { value: 'keep', label: C.levelKeep }]}
            onChange={(v) => edit((d) => setLevelAfterRule(d, v as LevelAfter))} />
        </Field>
        <Field label={C.classSkills} help={C.classSkillsHelp}>
          <Select value={gameClassSkills(doc)} options={[{ value: 'keep', label: C.classSkillsKeep }, { value: 'replace', label: C.classSkillsReplace }]}
            onChange={(v) => edit((d) => setClassSkillsRule(d, v as ClassSkills))} />
        </Field>
      </div>
    </Card>
  )
}
