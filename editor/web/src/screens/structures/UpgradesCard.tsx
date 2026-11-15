// "Can be upgraded to": kinds this structure can be built up into, each with a cost and an optional technology.
import { addUpgrade, footprintText, patchUpgrade, removeUpgrade, setUpgradeCost, upgradeTargets } from '../../model/structuresEconomy'
import { gridOfGame, isHex } from '../../model/gridSpec'
import { sizeOf } from '../../model/structures'
import { useDoc, useEdit } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { Select } from '../../ui/Select'
import { T } from './text'

export function UpgradesCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return null
  const key = `structure:${id}:upgrade`
  const upgrades = s.upgrades ?? []
  const resources = doc.game?.resources ?? []
  const techs = doc.research ?? []
  const free = upgradeTargets(doc, id)
  const { w, h } = sizeOf(s)
  const hex = isHex(gridOfGame(doc.game))
  const kindName = (kid: string) => doc.structures?.find((x) => x.id === kid)?.name ?? T.missing(kid)

  return (
    <Card title={T.upgrades} help={T.upgradesHelp} learn="structure-upgrades">
      <div className="stack">
        {upgrades.length === 0 && <p className="help">{T.upgradesNone}</p>}
        {upgrades.map((u, i) => {
          const choices = upgradeTargets(doc, id, u.to)
          // A target that is gone (or no longer the same size) stays visible so it can be seen and changed; the checks name it.
          const known = choices.some((c) => c.id === u.to)
          const options = [...(known ? [] : [{ value: u.to, label: doc.structures?.some((x) => x.id === u.to) ? T.wrongSize(kindName(u.to)) : T.missing(u.to) }]),
            ...choices.map((c) => ({ value: c.id, label: c.name }))]
          const techOptions = [{ value: '', label: T.needsNothing },
            ...(u.requires && !techs.some((t) => t.id === u.requires) ? [{ value: u.requires, label: T.missing(u.requires) }] : []),
            ...techs.map((t) => ({ value: t.id, label: t.name }))]
          return (
            <div className="story-row upgrade-row" role="group" aria-label={T.upgradeN(i + 1)} key={i}>
              <div className="story-row-head">
                <Field label={T.upgradeTo}>
                  <Select value={u.to} options={options} onChange={(v) => edit((d) => patchUpgrade(d, id, i, { to: v }), `${key}:${i}:to`)} />
                </Field>
                <button type="button" className="btn ghost sm icon" aria-label={T.removeUpgrade(i + 1)} onClick={() => edit((d) => removeUpgrade(d, id, i))}><Icon name="trash" /></button>
              </div>
              <FormGrid>
                <Field label={T.needsTech} help={techs.length === 0 ? T.needsTechNone : undefined}>
                  <Select value={u.requires ?? ''} options={techOptions} onChange={(v) => edit((d) => patchUpgrade(d, id, i, { requires: v === '' ? undefined : v }))} />
                </Field>
              </FormGrid>
              {resources.length > 0 && (
                <KeyNumberTable label={T.upgradeCost} help={T.upgradeCostHelp} rows={resources.map((r) => ({ key: r.id, label: r.name }))}
                  columns={[{ title: T.upgradeCostCol, values: u.cost, min: 0, placeholder: '0', onChange: (k, v) => edit((d) => setUpgradeCost(d, id, i, k, v), `${key}:${i}:cost:${k}`) }]} />
              )}
            </div>
          )
        })}
        <div>
          <button type="button" className="btn sm" disabled={free.length === 0} onClick={() => edit((d) => addUpgrade(d, id))}><Icon name="plus" />{T.addUpgrade}</button>
        </div>
        {free.length === 0 && <p className="help" data-testid="upgrade-none-free">{upgrades.length ? T.upgradeAllTaken : (hex ? T.upgradeNoTargetHex : T.upgradeNoTarget)(footprintText(s, hex))}</p>}
        <p className="help">{hex ? T.upgradeSizeHex : T.upgradeSize(w, h)}</p>
      </div>
    </Card>
  )
}
