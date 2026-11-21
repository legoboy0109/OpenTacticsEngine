// One level of a pair: the points it needs, its bonus (the combat numbers, in words) and its conversation.
// The conversation is edited with the campaign's scene editor (SceneCommands), limited to lines of dialogue.
import { useMemo } from 'react'
import { talkBase } from '../../model/supportTalk'
import { BONUS_KEYS, levelName, pairIndex, pairsOf, setBonus, setLevelPoints, setTalk, STAT_PREFIX, removeLevel, MAX_POINTS } from '../../model/supports'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { KeyNumberTable, type KvRow } from '../../ui/KeyNumberTable'
import { NumberInput } from '../../ui/NumberInput'
import { SceneCommands, type SceneSource } from '../campaign/SceneCommands'
import { T } from './text'
import { SUPPORT_BONUS_LIMIT } from '../../model/dataLimits'

export function LevelCard({ pairKey, level }: { pairKey: string; level: number }) {
  const doc = useDoc()
  const edit = useEdit()
  const i = pairIndex(doc, pairKey)
  const pair = pairsOf(doc)[i]
  const lv = pair?.levels?.[level]
  // The pair is found again by its key at each edit, so a conversation edit lands on the right pair even if the list changed.
  const source = useMemo<SceneSource>(() => ({
    eventId: `support:${pairKey}:${level}`,
    read: (d) => pairsOf(d)[pairIndex(d, pairKey)]?.levels?.[level]?.talk ?? [],
    write: (d, list) => setTalk(d, pairIndex(d, pairKey), level, list),
    goto: false,
    only: ['say'],
    base: talkBase,
  }), [pairKey, level])
  if (!pair || !lv) return null
  const name = levelName(doc.supports?.rules, level)
  const bonus = lv.bonus ?? {}
  const statName = (id: string) => doc.game?.stats.find((s) => s.id === id)?.name ?? id
  // Rows in a stable order: the combat numbers, then stats (HP and move have no combat bonus, so are never offered).
  const have: KvRow[] = [
    ...BONUS_KEYS.filter((k) => bonus[k] !== undefined).map((k) => ({ key: k, label: T.bonusKeys[k] })),
    ...Object.keys(bonus.stats ?? {}).map((s) => ({ key: STAT_PREFIX + s, label: statName(s) })),
  ]
  const options: KvRow[] = [
    ...BONUS_KEYS.map((k) => ({ key: k, label: T.bonusKeys[k] })),
    ...(doc.game?.stats ?? []).filter((s) => s.id !== doc.game?.roles.hp && s.id !== doc.game?.roles.move).map((s) => ({ key: STAT_PREFIX + s.id, label: s.name })),
  ]
  const key = `support:${pairKey}:${level}`
  const amounts: Record<string, number> = {}
  for (const r of have) {
    const v = r.key.startsWith(STAT_PREFIX) ? bonus.stats?.[r.key.slice(STAT_PREFIX.length)] : bonus[r.key]
    if (typeof v === 'number') amounts[r.key] = v
  }
  return (
    <Card title={T.levelTitle(name)} id={`support-level-${level + 1}`}
      actions={(pair.levels.length > 1) && <button type="button" className="btn sm danger" aria-label={T.removeLevel(name)} onClick={() => edit((d) => removeLevel(d, i, level))}>Remove</button>}>
      <div className="stack" data-testid={`support-level-${level + 1}`}>
        <Field label={T.needed}>
          <NumberInput value={lv.points} min={level === 0 ? 0 : 1} max={MAX_POINTS} aria-label={T.neededFor(level + 1)} onChange={(v) => { if (v !== undefined) edit((d) => setLevelPoints(d, i, level, v), `${key}:points`) }} />
        </Field>
        <KeyNumberTable label={T.bonus} help={`${T.bonusHelp} ${T.bonusTakenHelp}`} rows={have} emptyText={T.bonusNone}
          columns={[{ title: T.bonusAmount, values: amounts, min: -SUPPORT_BONUS_LIMIT, max: SUPPORT_BONUS_LIMIT, onChange: (k, v) => { if (v !== undefined) edit((d) => setBonus(d, i, level, k, v), `${key}:bonus:${k}`) } }]}
          onRemove={(k) => edit((d) => setBonus(d, i, level, k, undefined))}
          addOptions={options} onAdd={(k) => { if (options.some((o) => o.key === k)) edit((d) => setBonus(d, i, level, k, 1)) }} addLabel={T.bonusAdd} />
        <div>
          <h3 className="sub-title">{T.talk}</h3>
          <p className="help">{lv.talk?.length ? T.talkHelp : `${T.talkNone} ${T.talkHelp}`}</p>
          <div data-testid={`support-talk-${level + 1}`}>
            <SceneCommands source={source} name={T.talkName(name)} />
          </div>
        </div>
      </div>
    </Card>
  )
}
