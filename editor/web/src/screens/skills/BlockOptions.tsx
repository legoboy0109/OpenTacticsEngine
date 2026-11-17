// The two optional settings of a rule: "Chance" (timed points and strikes only) and "Reaches" (combat only).
import { chanceMode, setChance, setChanceMode, statChance, statChanceText, type ChanceMode } from '../../model/skillChance'
import { auraOf, combatStatIds, setAura, weaponKinds, type AuraSpec } from '../../model/skillBlocks'
import { hasChance, type When } from '../../model/skillRules'
import type { Extra, ProjectDoc } from '../../model/types'
import { Advanced } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Num } from './Num'
import { unitOptions, weaponOptions } from './options'
import { T } from './text'

interface Props { doc: ProjectDoc; block: Extra; setBlock: (fn: (b: Extra) => Extra, key?: string) => void }

export function ChanceRow({ doc, block, setBlock }: Props) {
  if (!hasChance(block.when as When)) return null
  const stats = combatStatIds(doc)
  const mode = chanceMode(block.chance, stats)
  const [stat, times] = statChance(block.chance)
  const modes = (Object.keys(T.chanceModes) as ChanceMode[]).map((m) => ({ value: m, label: T.chanceModes[m] }))
  const name = (id: string) => doc.game?.stats?.find((s) => s.id === id)?.name ?? id
  return (
    <div className="skill-option" data-testid="skill-chance">
      <Field label={T.chance} help={T.chanceHelp}>
        <Select value={mode} options={modes} onChange={(m) => setBlock((b) => setChanceMode(b, m as ChanceMode, stats))} />
      </Field>
      {mode === 'percent' && <Field label={T.chancePercent}><Num value={Number(block.chance)} min={-1000} max={1000} suffix="%" onChange={(n) => setBlock((b) => setChance(b, n), 'chance')} /></Field>}
      {mode === 'stat' && (
        <>
          <Field label={T.chanceStat}><Select value={stat} options={stats.map((s) => ({ value: s, label: name(s) }))} onChange={(s) => setBlock((b) => setChance(b, statChanceText(s, times)))} /></Field>
          <Field label={T.chanceTimes} help={T.chanceTimesHelp}><Num value={times} min={1} max={100} onChange={(n) => setBlock((b) => setChance(b, statChanceText(stat, n)), 'chanceTimes')} /></Field>
        </>
      )}
      {mode === 'custom' && (
        <Advanced label={T.advanced} defaultOpen>
          <Field label={T.chanceFormula} help={T.chanceFormulaHelp}><TextInput mono value={String(block.chance ?? '')} onChange={(v) => setBlock((b) => setChance(b, v), 'chanceFormula')} /></Field>
        </Advanced>
      )}
    </div>
  )
}

export function ReachesRow({ doc, block, setBlock }: Props) {
  if (block.when !== 'combat') return null
  const raw = block.aura as Extra | undefined
  const aura = raw ? auraOf(raw) : null
  const mode = aura ? aura.to : 'self'
  const put = (patch: Partial<AuraSpec>, key?: string) => setBlock((b) => setAura(b, { ...auraOf(b.aura as Extra | undefined), ...patch }, doc), key)
  const kinds = weaponKinds(doc)
  const kindNow = aura?.weaponKind ?? ''
  return (
    <div className="skill-option" data-testid="skill-reaches">
      <Field label={T.reaches} help={aura ? T.auraNote : T.reachesHelp}>
        <Select value={mode} options={[{ value: 'self', label: T.reachSelf }, { value: 'allies', label: T.reachAllies }, { value: 'enemies', label: T.reachEnemies }]}
          onChange={(m) => setBlock((b) => setAura(b, m === 'self' ? null : { ...auraOf(b.aura as Extra | undefined), to: m as 'allies' | 'enemies' }, doc))} />
      </Field>
      {aura && <Field label={T.reachWithin}><Num value={aura.within} min={1} max={10} onChange={(n) => put({ within: n }, 'within')} /></Field>}
      {aura && (
        <Field label={T.auraMatchPick}>
          <Select value={aura.match ?? ''} options={[{ value: '', label: T.auraMatchAny }, ...unitOptions(doc, aura.match ?? '')]} onChange={(m) => put({ match: m || undefined })} />
        </Field>
      )}
      {aura && (
        <Field label={T.auraWeaponPick} help={T.auraWeaponHelp}>
          <Select value={aura.weapon ?? ''} options={[{ value: '', label: T.auraWeaponAny }, ...weaponOptions(doc, aura.weapon ?? '')]} onChange={(m) => put({ weapon: m || undefined })} />
        </Field>
      )}
      {aura && (kinds.length > 0 || kindNow !== '') && (
        <Field label={T.auraKindPick}>
          <Select value={kindNow} options={[{ value: '', label: T.auraKindAny }, ...(kinds.includes(kindNow) || kindNow === '' ? kinds : [...kinds, kindNow]).map((k) => ({ value: k, label: kinds.includes(k) ? k : `${k} (no weapon has it)` }))]}
            onChange={(m) => put({ weaponKind: m || undefined })} />
        </Field>
      )}
      {aura && (
        <Advanced label={T.advanced} defaultOpen={!!aura.test}>
          <Field label={T.auraTest} help={T.auraTestHelp}><TextInput mono value={aura.test ?? ''} onChange={(v) => put({ test: v || undefined }, 'auraTest')} /></Field>
        </Advanced>
      )}
    </div>
  )
}
