// The "Doors, chests and places" card of a structure kind (docs/SRPG.md section 10): three parts, each switched on like the other parts of a form.
// What the loader refuses is greyed out with the reason (model/places.ts); the checks (model/placeChecks.ts) say the rest above the list.
import {
  blocksSomeone, fittingKeys, fromOf, lockedByDefault, partBlock, placeKindOf, setBlocksSight, setDoorHp, setDoorStat, setLockedByDefault, setOpensKind, setPlaceFrom,
  setPlaceKeys, setPlacePart, setVisitBecomes, setVisitRepeat, setVisitRuin, setWays, twinKinds, waysOf, DOOR_HP_MAX, WAYS, type PlaceKind,
} from '../../model/places'
import type { PlaceFrom, Structure } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field, FieldGroup } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { D } from './placeText'

const PARTS: Array<{ part: PlaceKind; label: string; help: string }> = [
  { part: 'door', label: D.door, help: D.doorHelp },
  { part: 'chest', label: D.chest, help: D.chestHelp },
  { part: 'visit', label: D.visit, help: D.visitHelp },
]

export function PlacePartsCard({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return null
  const on = placeKindOf(s)
  const why = (part: PlaceKind): string | undefined => {
    const b = partBlock(doc, s, part)
    return b === 'other' ? D.blockedOther : b === 'twin' ? D.blockedTwin : undefined
  }
  return (
    <Card title={D.title} help={D.help} id={`structure:${id}:places`}>
      <div className="stack" data-testid="place-parts">
        {PARTS.map(({ part, label, help }) => (
          <div key={part} className="stack tight" data-testid={`part-${part}`}>
            <Check label={label} help={on === part ? help : why(part) ?? help} checked={on === part} disabled={on !== part && !!why(part)}
              onChange={(v) => edit((d) => setPlacePart(d, id, part, v))} />
            {on === part && <div className="indent stack">{part === 'visit' ? <VisitPart s={s} /> : <OpeningPart s={s} part={part} />}</div>}
          </div>
        ))}
      </div>
    </Card>
  )
}

/** One of the "from" choices. */
function FromSelect({ s }: { s: Structure }) {
  const edit = useEdit()
  return (
    <Field label={D.from} help={placeKindOf(s) === 'visit' ? undefined : D.fromHelp}>
      <Select value={fromOf(s)} options={(Object.keys(D.fromKinds) as PlaceFrom[]).map((f) => ({ value: f, label: D.fromKinds[f] }))}
        onChange={(v) => edit((d) => setPlaceFrom(d, s.id, v as PlaceFrom))} />
    </Field>
  )
}

/** The part of a door and of a chest: what it becomes, locked or not, what can open it, which keys, hit points. */
function OpeningPart({ s, part }: { s: Structure; part: 'door' | 'chest' }) {
  const doc = useDoc()
  const edit = useEdit()
  const door = part === 'door'
  const block = s[part]!
  const ways = waysOf(s)
  const twins = twinKinds(doc, s)
  const keyOptions = fittingKeys(doc, s).map((i) => ({ value: i.id, label: i.name }))
  const listed = block.keys ?? []
  for (const k of listed) if (!keyOptions.some((o) => o.value === k)) keyOptions.push({ value: k, label: D.keyNotFitting(doc.items?.find((i) => i.id === k)?.name ?? k) })
  const anyKeyItem = (doc.items ?? []).some((i) => Array.isArray(i.opens) && i.opens.length > 0)
  const stats = (doc.game?.stats ?? []).map((x) => ({ key: x.id, label: x.name }))
  const known = twins.some((t) => t.id === block.opens)
  const opens = [...(known ? [] : [{ value: block.opens, label: `${block.opens} (not usable: pick another)` }]), ...twins.map((t) => ({ value: t.id, label: t.name }))]
  return (
    <>
      <Field label={D.opens} help={door ? D.opensHelpDoor : D.opensHelpChest}>
        <Select value={block.opens} options={opens} onChange={(v) => edit((d) => setOpensKind(d, s.id, v))} />
      </Field>
      {door && !blocksSomeone(s) && <p className="help" data-testid="door-blocks-nobody">{D.doorBlocksNobody}</p>}
      <Check label={D.locked} help={D.lockedHelp} checked={lockedByDefault(s)} onChange={(v) => edit((d) => setLockedByDefault(d, s.id, v))} />
      <FieldGroup label={D.ways} help={door ? D.waysHelpDoor : D.waysHelpChest}>
        <div className="check-list">
          {WAYS.filter((w) => door || w !== 'break').map((w) => (
            <Check key={w} label={D.ways_[w]} help={D.waysHelpEach[w]} checked={ways.includes(w)}
              onChange={(v) => edit((d) => setWays(d, s.id, v ? [...ways, w] : ways.filter((x) => x !== w)))} />
          ))}
        </div>
        {ways.length === 0 && <p className="help" data-testid="story-only">{D.storyOnly}</p>}
      </FieldGroup>
      {ways.includes('key') && (
        <>
          <MultiSelect label={D.keys} help={D.keysHelp} emptyText={D.keysNone} options={keyOptions} value={listed} onChange={(v) => edit((d) => setPlaceKeys(d, s.id, v))} />
          {!anyKeyItem && <p className="help">{D.noKeysInGame}</p>}
        </>
      )}
      {door && ways.includes('break') && typeof s.door?.hp === 'number' && (
        <>
          <Field label={D.hp} help={D.hpHelp}>
            <NumberInput value={s.door.hp} min={1} max={DOOR_HP_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setDoorHp(d, s.id, v), `structure:${s.id}:doorHp`) }} />
          </Field>
          {stats.length > 0 && (
            <KeyNumberTable label={D.stats} help={D.statsHelp} rows={stats}
              columns={[{ title: D.statsCol, values: s.door.stats, placeholder: '0', onChange: (k, v) => edit((d) => setDoorStat(d, s.id, k, v), `structure:${s.id}:doorStat:${k}`) }]} />
          )}
        </>
      )}
      <FromSelect s={s} />
      {door && <Check label={D.blocksSight} help={D.blocksSightHelp} checked={s.blocksSight === true} onChange={(v) => edit((d) => setBlocksSight(d, s.id, v))} />}
    </>
  )
}

function VisitPart({ s }: { s: Structure }) {
  const doc = useDoc()
  const edit = useEdit()
  const b = s.visit!
  const twins = twinKinds(doc, s)
  const kinds = (current: string | undefined, none: string) => [
    { value: '', label: none },
    ...(current !== undefined && !twins.some((t) => t.id === current) ? [{ value: current, label: `${current} (not usable: pick another)` }] : []),
    ...twins.map((t) => ({ value: t.id, label: t.name })),
  ]
  const repeat = b.repeat === true
  return (
    <>
      <Field label={D.becomes} help={repeat ? D.repeatBlocked : D.becomesHelp}>
        <Select value={b.becomes ?? ''} options={kinds(b.becomes, D.becomesNone)} disabled={repeat} onChange={(v) => edit((d) => setVisitBecomes(d, s.id, v))} />
      </Field>
      <Field label={D.ruin} help={D.ruinHelp}>
        <Select value={b.ruin ?? ''} options={kinds(b.ruin, D.ruinNone)} onChange={(v) => edit((d) => setVisitRuin(d, s.id, v))} />
      </Field>
      {twins.length === 0 && <p className="help">{D.noTwinsVisit}</p>}
      <Check label={D.repeat} help={b.becomes !== undefined ? D.repeatBlocked : D.repeatHelp} checked={repeat} disabled={!repeat && b.becomes !== undefined}
        onChange={(v) => edit((d) => setVisitRepeat(d, s.id, v))} />
      <FromSelect s={s} />
    </>
  )
}
