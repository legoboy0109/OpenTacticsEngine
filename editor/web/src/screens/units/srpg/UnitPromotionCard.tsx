// The Promotion card of a unit type (E3 of docs/SRPG.md): rows of "becomes <unit type>, from level N, by item, on reaching the
// level, level afterwards, stat changes". Edits `promotions` of the unit type.
import { addPromotion, baseDifference, gameLevelAfter, maxLevelOf, movePromotion, patchPromotion, progressionOn, promotionItems, promotionPatches, promotionsOf, removePromotion, setGain, type AtLevel, type LevelAfter } from '../../../model/promotion'
import type { Promotion } from '../../../model/srpgTypes'
import type { NumMap } from '../../../model/types'
import { useDoc, useEdit } from '../../../store/context'
import { Card, FormGrid } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { KeyNumberTable } from '../../../ui/KeyNumberTable'
import { MultiSelect } from '../../../ui/MultiSelect'
import { NumberInput } from '../../../ui/NumberInput'
import { RowButtons } from '../../../ui/RowButtons'
import { Select } from '../../../ui/Select'
import { SrpgChecks } from '../../srpg/SrpgChecks'
import '../../srpg/e3.css'
import { P } from './promotionText'

export function UnitPromotionCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const unit = doc.units?.find((u) => u.id === id)
  if (!unit) return null
  const list = promotionsOf(unit)
  const others = (doc.units ?? []).filter((u) => u.id !== id)
  const key = `unit:${id}:promotion`
  const patch = (i: number, p: Record<string, unknown>, k?: string) => edit((d) => patchPromotion(d, id, i, p), k && `${key}:${i}:${k}`)

  return (
    <Card title={P.cardTitle} help={P.cardHelp} id="unit-promotion">
      <div className="stack" data-testid="unit-promotion">
        <SrpgChecks section="units" id={id} testId="promotion-checks" />
        {list.length === 0 && <p className="help">{others.length === 0 ? P.needsTwo : P.none}</p>}
        {list.map((p, i) => (
          <PromotionRow key={i} index={i} count={list.length} from={id} promo={p}
            patch={(x, k) => patch(i, x, k)} onUp={() => edit((d) => movePromotion(d, id, i, i - 1))}
            onDown={() => edit((d) => movePromotion(d, id, i, i + 1))} onRemove={() => edit((d) => removePromotion(d, id, i))} />
        ))}
        <div><button type="button" className="btn" disabled={others.length === 0} onClick={() => edit((d) => addPromotion(d, id))}>{P.add}</button></div>
      </div>
    </Card>
  )
}

interface RowProps {
  index: number; count: number; from: string; promo: Promotion
  patch: (p: Record<string, unknown>, key?: string) => void
  onUp: () => void; onDown: () => void; onRemove: () => void
}

function PromotionRow({ index, count, from, promo, patch, onUp, onDown, onRemove }: RowProps) {
  const doc = useDoc()
  const n = index + 1
  const targets = (doc.units ?? []).filter((u) => u.id !== from)
  const known = targets.some((u) => u.id === promo.to)
  const levels = progressionOn(doc)
  const items = promotionItems(doc)
  const hand = promo.gains !== undefined
  const statRows = (doc.game?.stats ?? []).map((s) => ({ key: s.id, label: s.name }))
  const diff = baseDifference(doc, from, promo.to)
  const diffText = Object.entries(diff).map(([s, v]) => `${doc.game?.stats.find((x) => x.id === s)?.name ?? s} ${v > 0 ? '+' : ''}${v}`).join(', ') || P.gainsNothing
  const gains = (promo.gains ?? {}) as NumMap
  const atLevel: AtLevel = promo.atLevel ?? 'off'
  const afterValue = promo.levelAfter ?? 'game'

  return (
    <div className="promotion-row" role="group" aria-label={P.rowTitle(n)}>
      <div className="story-row-head">
        <strong>{P.rowTitle(n)}</strong>
        <RowButtons index={index} count={count} up={P.up(n)} down={P.down(n)} del={P.remove(n)} onUp={onUp} onDown={onDown} onDelete={onRemove} />
      </div>
      <FormGrid>
        <Field label={P.becomes} help={P.becomesHelp}>
          <Select value={promo.to} options={[...(known ? [] : [{ value: promo.to, label: `${promo.to} (not a unit type)` }]), ...targets.map((u) => ({ value: u.id, label: u.name }))]}
            onChange={(v) => patch(promotionPatches.to(v))} />
        </Field>
        <Field label={P.fromLevel} help={P.fromLevelHelp}>
          <NumberInput value={promo.fromLevel ?? 1} min={1} max={maxLevelOf(doc)} onChange={(v) => { if (v !== undefined) patch(promotionPatches.fromLevel(v), 'fromLevel') }} />
        </Field>
        <Field label={P.atLevel} help={levels ? P.atLevelHelp : P.atLevelNeedsLevels}>
          <Select value={atLevel} options={(['off', 'offer', 'auto'] as const).map((v) => ({ value: v, label: P.atLevelOptions[v], disabled: !levels && v !== 'off' && atLevel !== v }))}
            onChange={(v) => patch(promotionPatches.atLevel(v as AtLevel))} />
        </Field>
        <Field label={P.levelAfter}>
          <Select value={afterValue} options={[{ value: 'game', label: P.levelAfterGame(gameLevelAfter(doc) === 'reset') }, { value: 'reset', label: P.levelAfterReset }, { value: 'keep', label: P.levelAfterKeep }]}
            onChange={(v) => patch(promotionPatches.levelAfter(v as LevelAfter | 'game'))} />
        </Field>
      </FormGrid>
      <MultiSelect label={P.byItem} help={P.byItemHelp} emptyText={P.noItems} value={promo.items ?? []}
        options={items.map((i) => ({ value: i.id, label: i.name }))} onChange={(v) => patch(promotionPatches.items(v))} />
      <Field label={P.gains} help={hand ? P.gainsTableHelp : `${P.gainsHelp} ${P.gainsNowDiff(diffText)}`}>
        <Select value={hand ? 'table' : 'diff'} options={[{ value: 'diff', label: P.gainsDiff }, { value: 'table', label: P.gainsTable }]}
          onChange={(v) => patch({ gains: v === 'table' ? diff : undefined })} />
      </Field>
      {hand && (
        <KeyNumberTable label={P.gains} rows={statRows}
          columns={[{ title: P.gainsCol, values: gains, placeholder: '0', onChange: (k, v) => patch({ gains: setGain(gains, k, v) }, `gain:${k}`) }]} />
      )}
    </div>
  )
}
