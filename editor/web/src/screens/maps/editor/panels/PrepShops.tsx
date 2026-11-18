// Several preparation shops of a map (docs/EVENTS.md "Preparation"): each has a name, what it sells, whether it buys, and music. The single
// shop of the older form is still the plain block in PrepPanel; "Use several shops" turns it into the first of a list.
import { musicIds } from '../../../../model/audio'
import { addShop, MAX_SHOPS, patchShop, removeShop, shopsOf, toSingleShop } from '../../../../model/prepShops'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { AudioChoice } from '../../../sound/AudioChoice'
import { Field } from '../../../../ui/Field'
import { Icon } from '../../../../ui/Icon'
import { Select } from '../../../../ui/Select'
import { TextInput } from '../../../../ui/TextInput'
import { Toggle } from '../../../../ui/Toggle'
import { P } from './prepText'

interface Props { doc: ProjectDoc; map: MapFile; apply: (fn: (m: MapFile) => MapFile) => void }

export function PrepShops({ doc, map, apply }: Props) {
  const shops = shopsOf(map)
  const priced = (doc.items ?? []).filter((i) => i.price && Object.keys(i.price).length > 0)
  return (
    <div className="stack" data-testid="prep-shops">
      <p className="help">{P.shopsHelp}</p>
      {shops.map((s, i) => (
        <section key={s.id} className="card-lite" aria-label={s.name ?? s.id} data-testid={`prep-shops-${s.id}`}>
          <div className="row">
            <TextInput aria-label={P.shopName(i + 1)} value={s.name ?? ''} onChange={(v) => apply((m) => patchShop(m, i, { name: v }))} />
            <button type="button" className="btn ghost sm icon" aria-label={P.remove(s.name ?? s.id)} onClick={() => apply((m) => removeShop(m, i))}><Icon name="trash" /></button>
          </div>
          <Toggle label={P.shopBuys} help={P.shopBuysHelp} checked={s.buys !== false} onChange={(v) => apply((m) => patchShop(m, i, { buys: v }))} />
          {s.sells.length === 0 && <p className="help">{P.shopNone}</p>}
          {s.sells.map((id) => (
            <div className="row" key={id}>
              <strong className="grow">{doc.items?.find((x) => x.id === id)?.name ?? id}</strong>
              <button type="button" className="btn ghost sm icon" aria-label={P.remove(doc.items?.find((x) => x.id === id)?.name ?? id)} onClick={() => apply((m) => patchShop(m, i, { sells: s.sells.filter((x) => x !== id) }))}><Icon name="trash" /></button>
            </div>
          ))}
          <Select aria-label={`${P.shopAdd} (${s.name ?? s.id})`} value="" placeholder={P.shopAdd}
            options={priced.filter((it) => !s.sells.includes(it.id)).map((it) => ({ value: it.id, label: it.name ?? it.id }))}
            onChange={(v) => apply((m) => patchShop(m, i, { sells: [...s.sells, v] }))} />
          {musicIds(doc).length > 0 && (
            <Field label={P.shopMusic} help={P.shopMusicHelp}>
              <AudioChoice kind="music" mode="nothing" value={s.music} aria-label={`${P.shopMusic} (${s.name ?? s.id})`}
                onChange={(v) => apply((m) => patchShop(m, i, { music: v || undefined }))} />
            </Field>
          )}
        </section>
      ))}
      <div className="row">
        <button type="button" className="btn sm" disabled={shops.length >= MAX_SHOPS} onClick={() => apply(addShop)}>{P.shopsAdd}</button>
        {shops.length === 1 && <button type="button" className="btn ghost sm" onClick={() => apply(toSingleShop)}>{P.shopsBack}</button>}
      </div>
    </div>
  )
}
