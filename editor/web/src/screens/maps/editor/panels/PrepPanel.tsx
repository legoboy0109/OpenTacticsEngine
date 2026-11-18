// The "Preparation" tab: whether the map has a preparation screen, how many may be deployed, who must / must not / stays,
// the preparation shop, and what is wrong with it (docs/EVENTS.md "Preparation"). The deployment squares themselves are
// painted with the Slots tool.
import { storyIdOf } from '../../../../model/characterUnits'
import { editMap } from '../../../../model/mapCtx'
import {
  addFixed, addForbidden, addRequired, disablePrep, enablePrep, isChoosable, removeFixed, removeForbidden, removeRequired, requiredId, requiredSlot,
  setLimit, setRequiredSlot, setShop, setSide,
} from '../../../../model/prep'
import { prepChecks } from '../../../../model/prepChecks'
import { isSeveralShops, setSingleShopMusic, toSeveralShops } from '../../../../model/prepShops'
import { musicIds } from '../../../../model/audio'
import { AudioChoice } from '../../../sound/AudioChoice'
import { PrepShops } from './PrepShops'
import { setShopBuys, shopBuys } from '../../../../model/sell'
import { peopleOf } from '../../../../model/prepRoster'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { Icon } from '../../../../ui/Icon'
import { NumberInput } from '../../../../ui/NumberInput'
import { Select } from '../../../../ui/Select'
import { Toggle } from '../../../../ui/Toggle'
import { P } from './prepText'

interface Props {
  doc: ProjectDoc
  map: MapFile
  mapId: string
  /** Switches the map editor to the Slots tool. */
  onSlots: () => void
  say: (m: string | null) => void
}

export function PrepPanel({ doc, map, mapId, onSlots, say }: Props) {
  const edit = useEdit()
  const prep = map.prep
  const apply = (fn: (m: MapFile) => MapFile, key?: string) => edit((d) => editMap(d, mapId, (m) => fn(m)), key)
  const applyChecked = (fn: (m: MapFile) => { map: MapFile; error: string | null }) => {
    const r = fn(map)
    if (r.error) { say(r.error); return }
    edit((d) => editMap(d, mapId, (m) => fn(m).map))
  }
  const people = peopleOf(doc)
  const nameOf = (id: string) => people.find((p) => p.id === id)?.name ?? id
  const humanSides = (map.sides ?? []).filter((s) => s.controller !== 'ai')
  return (
    <div className="stack tight" data-testid="prep-panel">
      <h3>{P.title}</h3>
      <Toggle label={P.switchLabel} help={P.switchHelp} checked={!!prep}
        onChange={(v) => edit((d) => editMap(d, mapId, (m, c) => (v ? enablePrep(m, c) : disablePrep(m))))} />
      {prep && (
        <>
          <p className="help" data-testid="prep-slot-count">{P.slotsLabel(prep.slots.length)}. {P.slotsHelp}</p>
          <button type="button" className="btn sm" onClick={onSlots}>{P.slotsButton}</button>
          <Field label={P.limitLabel} help={P.limitHelp}>
            <NumberInput value={prep.limit ?? prep.slots.length} min={1} max={Math.max(1, prep.slots.length)} onChange={(v) => v !== undefined && apply((m) => setLimit(m, v), `prep:${mapId}:limit`)} />
          </Field>
          {humanSides.length > 1 && (
            <Field label={P.sideLabel} help={P.sideHelp}>
              <Select value={prep.side ?? humanSides[0].id} options={humanSides.map((s) => ({ value: s.id, label: s.name || s.id }))} onChange={(v) => apply((m) => setSide(m, v))} />
            </Field>
          )}

          <h4>{P.requiredTitle}</h4>
          <p className="help">{P.requiredHelp}</p>
          {(prep.required ?? []).length === 0 && <p className="help">{P.requiredNone}</p>}
          {(prep.required ?? []).map((r) => {
            const id = requiredId(r)
            return (
              <div className="row" key={id} data-testid={`prep-required-${id}`}>
                <strong className="grow">{nameOf(id)}</strong>
                <Select aria-label={`${nameOf(id)}: ${P.slotsLabel(0).replace('0 ', '')}`} value={String(requiredSlot(r) ?? 0)}
                  options={[{ value: '0', label: P.requiredAnySlot }, ...prep.slots.map((_, i) => ({ value: String(i + 1), label: P.slotOf(i + 1) }))]}
                  onChange={(v) => applyChecked((m) => setRequiredSlot(m, id, Number(v) || undefined))} />
                <button type="button" className="btn ghost sm icon" aria-label={P.remove(nameOf(id))} onClick={() => apply((m) => removeRequired(m, id))}><Icon name="trash" /></button>
              </div>
            )
          })}
          <Select aria-label={P.requiredAdd} value="" placeholder={P.requiredAdd}
            options={people.filter((p) => !(prep.required ?? []).some((r) => requiredId(r) === p.id)).map((p) => ({ value: p.id, label: p.name }))}
            onChange={(v) => applyChecked((m) => addRequired(m, v))} />

          <h4>{P.forbiddenTitle}</h4>
          <p className="help">{P.forbiddenHelp}</p>
          {(prep.forbidden ?? []).map((id) => (
            <div className="row" key={id} data-testid={`prep-forbidden-${id}`}>
              <strong className="grow">{nameOf(id)}</strong>
              <button type="button" className="btn ghost sm icon" aria-label={P.remove(nameOf(id))} onClick={() => apply((m) => removeForbidden(m, id))}><Icon name="trash" /></button>
            </div>
          ))}
          <Select aria-label={P.forbiddenAdd} value="" placeholder={P.forbiddenAdd}
            options={people.filter((p) => !(prep.forbidden ?? []).includes(p.id)).map((p) => ({ value: p.id, label: p.name }))}
            onChange={(v) => apply((m) => addForbidden(m, v))} />

          <h4>{P.fixedTitle}</h4>
          <p className="help">{P.fixedHelp}</p>
          {(prep.fixed ?? []).map((id) => (
            <div className="row" key={id} data-testid={`prep-fixed-${id}`}>
              <strong className="grow">{nameOf(id)}</strong>
              <button type="button" className="btn ghost sm icon" aria-label={P.remove(nameOf(id))} onClick={() => apply((m) => removeFixed(m, id))}><Icon name="trash" /></button>
            </div>
          ))}
          <Select aria-label={P.fixedAdd} value="" placeholder={P.fixedAdd}
            options={(map.units ?? []).filter((u) => storyIdOf(u) !== undefined && isChoosable(map, prep, u)).map((u) => ({ value: storyIdOf(u)!, label: u.name ?? storyIdOf(u)! }))}
            onChange={(v) => apply((m) => addFixed(m, v))} />

          <h4>{P.shopTitle}</h4>
          {isSeveralShops(map) ? <PrepShops doc={doc} map={map} apply={apply} /> : (
            <Toggle label={P.shopSwitch} help={P.shopHelp} checked={!!prep.shop} onChange={(v) => apply((m) => setShop(m, v ? [] : null))} />
          )}
          {!isSeveralShops(map) && prep.shop && (
            <>
              <Toggle label={P.shopBuys} help={P.shopBuysHelp} checked={shopBuys(map)} onChange={(v) => apply((m) => setShopBuys(m, v))} />
              {prep.shop.sells.length === 0 && <p className="help">{P.shopNone}</p>}
              {prep.shop.sells.map((id) => (
                <div className="row" key={id} data-testid={`prep-shop-${id}`}>
                  <strong className="grow">{doc.items?.find((i) => i.id === id)?.name ?? id}</strong>
                  <span className="help">{Object.entries(doc.items?.find((i) => i.id === id)?.price ?? {}).map(([r, n]) => `${n} ${doc.game?.resources?.find((x) => x.id === r)?.name ?? r}`).join(', ')}</span>
                  <button type="button" className="btn ghost sm icon" aria-label={P.remove(doc.items?.find((i) => i.id === id)?.name ?? id)} onClick={() => apply((m) => setShop(m, prep.shop!.sells.filter((x) => x !== id)))}><Icon name="trash" /></button>
                </div>
              ))}
              <Select aria-label={P.shopAdd} value="" placeholder={P.shopAdd}
                options={(doc.items ?? []).filter((i) => i.price && Object.keys(i.price).length > 0 && !prep.shop!.sells.includes(i.id)).map((i) => ({ value: i.id, label: i.name ?? i.id }))}
                onChange={(v) => apply((m) => setShop(m, [...prep.shop!.sells, v]))} />
              {musicIds(doc).length > 0 && (
                <Field label={P.shopMusic} help={P.shopMusicHelp}>
                  <AudioChoice kind="music" mode="nothing" value={prep.shop.music} aria-label={P.shopMusic} onChange={(v) => apply((m) => setSingleShopMusic(m, v || undefined))} />
                </Field>
              )}
              <div><button type="button" className="btn ghost sm" onClick={() => apply(toSeveralShops)}>{P.shopsUse}</button></div>
            </>
          )}

          <ChecksList doc={doc} mapId={mapId} />
        </>
      )}
    </div>
  )
}

function ChecksList({ doc, mapId }: { doc: ProjectDoc; mapId: string }) {
  const checks = prepChecks(doc, mapId)
  return (
    <section aria-label={P.checksTitle} data-testid="prep-checks">
      <h4>{P.checksTitle}</h4>
      {checks.length === 0 ? <p className="help">{P.noChecks}</p> : <ul className="usage-list">{checks.map((c, i) => <li key={i}>{c.text}</li>)}</ul>}
    </section>
  )
}
