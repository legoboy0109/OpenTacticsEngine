// The ladder of the weapon ranks card: one row per rank (name, experience needed, id) with move up / down / remove. The first rank is
// always 0; typing a number that is not above the rank before is refused with a message, and ranks above move up to make room.
import { useState } from 'react'
import { MAX_RANK_XP, RANK_ID, ladderOf, moveRank, removeRank, renameRank, setRankId, setRankXp } from '../../../model/ranks'
import { rankUsageLines } from '../../../model/rankRefs'
import { useDoc, useEdit } from '../../../store/context'
import { DeleteConfirm } from '../../../ui/DeleteConfirm'
import { IdInput } from '../../../ui/IdInput'
import { NumberInput } from '../../../ui/NumberInput'
import { RowButtons } from '../../../ui/RowButtons'
import { TextInput } from '../../../ui/TextInput'
import { R } from '../../ranks/text'
import '../../ranks/ranks.css'

export function LadderRows() {
  const doc = useDoc()
  const edit = useEdit()
  const [removing, setRemoving] = useState<string | null>(null)
  const ladder = ladderOf(doc)
  const target = ladder.find((r) => r.id === removing)
  const idProblem = (id: string, self: string) => (!RANK_ID.test(id) ? R.rankIdBad : ladder.some((r) => r.id === id && r.id !== self) ? R.rankIdTaken : null)
  return (
    <>
      <ol className="rank-rows" aria-label={R.ladder} data-testid="rank-ladder">
        {/* Keyed by position: an id that is being retyped must not remount its row. */}
        {ladder.map((r, i) => (
          <li key={i} className="rank-row" data-testid={`rank-row-${r.id}`}>
            <span className="rank-no" aria-hidden="true">{i + 1}</span>
            <label className="rank-cell"><span>{R.rankName}</span>
              <TextInput value={r.name} aria-label={R.rankNameLabel(i + 1)} onChange={(v) => edit((d) => renameRank(d, r.id, v), `rank:${r.id}:name`)} />
            </label>
            <label className="rank-cell"><span>{R.rankXp}</span>
              {i === 0
                ? <input className="input num" value="0" readOnly aria-label={R.rankXpLabel(r.name)} title={R.rankXpFirst} />
                : <NumberInput value={r.xp} min={ladder[i - 1].xp + 1} max={MAX_RANK_XP - (ladder.length - 1 - i)} aria-label={R.rankXpLabel(r.name)}
                  onChange={(v) => { if (v !== undefined) edit((d) => setRankXp(d, r.id, v), `rank:${r.id}:xp`) }} />}
            </label>
            <label className="rank-cell"><span>{R.rankId}</span>
              <IdInput value={r.id} aria-label={R.rankIdLabel(r.name)} validate={(t) => idProblem(t, r.id)}
                onChange={(v) => edit((d) => setRankId(d, r.id, v))} />
            </label>
            <RowButtons index={i} count={ladder.length} up={R.moveUp(r.name)} down={R.moveDown(r.name)} del={R.removeRank(r.name)}
              onUp={() => edit((d) => moveRank(d, i, i - 1))} onDown={() => edit((d) => moveRank(d, i, i + 1))} onDelete={() => setRemoving(r.id)} />
          </li>
        ))}
      </ol>
      <DeleteConfirm open={!!target} what={target ? R.removeWhat(target.name) : ''} usages={target ? rankUsageLines(doc, target.id) : []}
        onCancel={() => setRemoving(null)} onConfirm={() => { if (target) edit((d) => removeRank(d, target.id)); setRemoving(null) }} />
    </>
  )
}
