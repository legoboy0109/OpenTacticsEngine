// "Promotion sounds" under the Events list: sounds that play some milliseconds after a promotion starts, next to the promotion jingle.
import { soundIds } from '../../model/audio'
import { addPromotionSound, MAX_PROMOTION_MS, MAX_PROMOTION_SOUNDS, patchPromotionSound, promotionSoundProblems, promotionSoundRows, removePromotionSound } from '../../model/promotionSounds'
import { useDoc, useEdit } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { T } from './text'

export function PromotionSounds() {
  const doc = useDoc()
  const edit = useEdit()
  const rows = promotionSoundRows(doc)
  const ids = soundIds(doc)
  if (ids.length === 0 && rows.length === 0) return null
  return (
    <section className="slot-group" aria-label={T.promoSoundsTitle} data-testid="promotion-sounds">
      <h3>{T.promoSoundsTitle}</h3>
      <p className="help">{T.promoSoundsHelp}</p>
      <ul className="slot-list">
        {rows.map((r, i) => (
          <li key={i} className="slot-row" data-testid={`promotion-sound-${i + 1}`}>
            <NumberInput aria-label={T.promoSoundTime(i + 1)} value={r.atMs} min={0} max={MAX_PROMOTION_MS} suffix="ms"
              onChange={(v) => { if (v !== undefined) edit((d) => patchPromotionSound(d, i, { atMs: v }), `promoSound:${i}:time`) }} />
            <Select aria-label={T.promoSoundWhich(i + 1)} value={r.sound} options={[...(r.sound && !ids.includes(r.sound) ? [{ value: r.sound, label: `${r.sound} (not there any more)` }] : []), ...ids.map((s) => ({ value: s, label: s }))]}
              onChange={(v) => edit((d) => patchPromotionSound(d, i, { sound: v }))} />
            <button type="button" className="btn sm" aria-label={T.promoSoundRemove(i + 1)} onClick={() => edit((d) => removePromotionSound(d, i))}>{T.promoSoundRemoveText}</button>
          </li>
        ))}
      </ul>
      <div><button type="button" className="btn sm" disabled={rows.length >= MAX_PROMOTION_SOUNDS || ids.length === 0} onClick={() => edit(addPromotionSound)}>{T.promoSoundAdd}</button></div>
      {promotionSoundProblems(doc).map((p) => <Banner key={p} tone="warn" title={p} />)}
    </section>
  )
}
