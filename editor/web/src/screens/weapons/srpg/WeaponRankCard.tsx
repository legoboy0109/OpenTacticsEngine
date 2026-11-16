// The "Weapon rank" card of a weapon (docs/SRPG.md section 7): the rank needed to use it, and (when ranks grow with use) the experience
// it earns per use. Ranks are kept per weapon group, so a weapon without a group is told to get one first. Shown once the game has a
// ladder (or the weapon already names a rank, so it can be cleared).
import { DEFAULT_USE_XP, MAX_USE_XP, growthOf, hasLadder, ladderOf, setWeaponRank, setWeaponRankXp } from '../../../model/ranks'
import { rankChecks } from '../../../model/rankChecks'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { R } from '../../ranks/text'

export function WeaponRankCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const w = doc.weapons?.find((x) => x.id === id)
  if (!w || (!hasLadder(doc) && w.rank === undefined && w.rankXp === undefined)) return null
  const ladder = ladderOf(doc)
  const key = `weapon:${id}`
  const own = rankChecks(doc).filter((c) => c.section === 'weapons' && c.ids?.includes(id))
  const known = w.rank !== undefined && ladder.some((r) => r.id === w.rank)
  const options = [{ value: '', label: R.noRankNeeded }, ...ladder.map((r) => ({ value: r.id, label: r.name })),
    ...(w.rank !== undefined && !known ? [{ value: w.rank, label: `${w.rank} (not in the ladder)` }] : [])]
  return (
    <Card title={R.weaponTitle} help={R.weaponHelp} id="weapon-rank">
      <div className="stack" data-testid="weapon-rank">
        {!w.group && <p className="help" data-testid="weapon-rank-needs-group">{R.needsGroup}</p>}
        <Field label={R.requiredRank}>
          <Select value={w.rank ?? ''} options={options} disabled={!w.group && w.rank === undefined}
            onChange={(v) => edit((d) => setWeaponRank(d, id, v === '' ? undefined : v), `${key}:rank`)} />
        </Field>
        {growthOf(doc) === 'use' && (
          <Field label={R.rankXpPerUse} help={R.rankXpPerUseHelp}>
            <NumberInput value={w.rankXp} allowEmpty min={1} max={MAX_USE_XP} placeholder={String(DEFAULT_USE_XP)} disabled={!w.group && w.rankXp === undefined}
              onChange={(v) => edit((d) => setWeaponRankXp(d, id, v), `${key}:rankXp`)} />
          </Field>
        )}
        {own.length > 0 && <Banner tone="warn" title={R.worthALook}><ul className="usage-list" data-testid="weapon-rank-checks">{own.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
      </div>
    </Card>
  )
}
