// Rules card "Weapon ranks" (docs/SRPG.md section 7): how ranks grow, the ladder (id, name, experience) and the default E-to-S ladder.
// Shown while the feature is on. The editor-side checks ("worth a look") sit at the bottom of the card; the loader's own refusals
// reach the Problems list.
import { useMemo } from 'react'
import { rankChecks } from '../../../model/rankChecks'
import { MAX_RANKS, addRank, growthOf, ladderOf, ranksOn, setGrowth, fillDefaultLadder } from '../../../model/ranks'
import type { RankGrowth } from '../../../model/srpgTypes'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { Field, FieldGroup } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { Select } from '../../../ui/Select'
import { R } from '../../ranks/text'
import { LadderRows } from './LadderRows'

export function RanksCard() {
  const doc = useDoc()
  const edit = useEdit()
  const checks = useMemo(() => rankChecks(doc).filter((c) => c.section === 'rules'), [doc])
  if (!ranksOn(doc)) return null
  const growth = growthOf(doc)
  const count = ladderOf(doc).length
  return (
    <Card title={R.ranksTitle} help={R.ranksIntro} id="weapon-ranks" learn="rules-ranks">
      <div className="stack" data-testid="ranks-card">
        <Field label={R.growth} help={growth === 'use' ? R.growthUseHelp : R.growthFixedHelp}>
          <Select value={growth} onChange={(v) => edit((d) => setGrowth(d, v as RankGrowth))}
            options={[{ value: 'fixed', label: R.growthFixed }, { value: 'use', label: R.growthUse }]} />
        </Field>
        <FieldGroup label={R.ladder} help={R.ladderHelp}>
          {count === 0 && <p className="help" data-testid="ladder-empty">{R.ladderEmpty}</p>}
          <LadderRows />
          <div className="rank-actions">
            <button type="button" className="btn sm" disabled={count >= MAX_RANKS} onClick={() => edit((d) => addRank(d))}><Icon name="plus" />{R.addRank}</button>
            {count === 0 && <button type="button" className="btn sm" onClick={() => edit(fillDefaultLadder)}>{R.defaultLadder}</button>}
          </div>
          {count === 0 && <p className="help">{R.defaultLadderHelp}</p>}
          {count >= MAX_RANKS && <p className="help">{R.ladderFull(MAX_RANKS)}</p>}
        </FieldGroup>
        {checks.length > 0 && (
          <Banner tone="warn" title={R.worthALook}><ul className="usage-list" data-testid="rank-checks">{checks.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>
        )}
      </div>
    </Card>
  )
}
