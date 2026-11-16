// The "Weapon rank" card of a weapon or a staff (docs/SRPG.md sections 7 and 8): the rank needed to use it, and (when ranks grow with use)
// the experience it earns per use. Ranks are kept per group, so one without a group is told to get one first. Shown once the game has a
// ladder (or the item already names a rank, so it can be cleared). `owner` picks the weapon or the staff: the card is the same.
import { gearOf, type GearOwner } from '../../../model/gear'
import { DEFAULT_USE_XP, MAX_USE_XP, growthOf, hasLadder, ladderOf, setGearRank, setGearRankXp } from '../../../model/ranks'
import { rankChecks } from '../../../model/rankChecks'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { R } from '../../ranks/text'

export function WeaponRankCard({ id, owner = 'weapon' }: { id: string; owner?: GearOwner }) {
  const doc = useDoc()
  const edit = useEdit()
  const g = gearOf(doc, owner, id)
  if (!g || (!hasLadder(doc) && g.keys.rank === undefined && g.keys.rankXp === undefined)) return null
  const w = g.keys
  const staff = owner === 'item'
  const ladder = ladderOf(doc)
  const key = `${staff ? 'item' : 'weapon'}:${id}`
  const own = rankChecks(doc).filter((c) => c.section === g.section && c.ids?.includes(id))
  const known = w.rank !== undefined && ladder.some((r) => r.id === w.rank)
  const options = [{ value: '', label: R.noRankNeeded }, ...ladder.map((r) => ({ value: r.id, label: r.name })),
    ...(w.rank !== undefined && !known ? [{ value: w.rank, label: `${w.rank} (not in the ladder)` }] : [])]
  return (
    <Card title={staff ? R.staffTitle : R.weaponTitle} help={staff ? R.staffHelp : R.weaponHelp} id={staff ? 'staff-rank' : 'weapon-rank'}>
      <div className="stack" data-testid={staff ? 'staff-rank' : 'weapon-rank'}>
        {!w.group && <p className="help" data-testid="weapon-rank-needs-group">{staff ? R.staffNeedsGroup : R.needsGroup}</p>}
        <Field label={R.requiredRank}>
          <Select value={w.rank ?? ''} options={options} disabled={!w.group && w.rank === undefined}
            onChange={(v) => edit((d) => setGearRank(d, owner, id, v === '' ? undefined : v), `${key}:rank`)} />
        </Field>
        {growthOf(doc) === 'use' && (
          <Field label={R.rankXpPerUse} help={staff ? R.staffRankXpPerUseHelp : R.rankXpPerUseHelp}>
            <NumberInput value={w.rankXp} allowEmpty min={1} max={MAX_USE_XP} placeholder={String(DEFAULT_USE_XP)} disabled={!w.group && w.rankXp === undefined}
              onChange={(v) => edit((d) => setGearRankXp(d, owner, id, v), `${key}:rankXp`)} />
          </Field>
        )}
        {own.length > 0 && <Banner tone="warn" title={R.worthALook}><ul className="usage-list" data-testid="weapon-rank-checks">{own.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
      </div>
    </Card>
  )
}
