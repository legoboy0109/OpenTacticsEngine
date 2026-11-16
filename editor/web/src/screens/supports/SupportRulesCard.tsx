// The settings card of the Supports screen: `rules` of data/supports.json (docs/SRPG.md section 6). Every box that is empty means
// "the default", and a default is never written, so the file stays as small as what the person chose.
import { DEFAULT_RANGE, MAX_LEVELS, MAX_POINTS, mostLevels, setConversations, setLevelNames, setMaxLevels, setPointSource, setRange } from '../../model/supports'
import type { SupportTalks } from '../../model/srpgTypes'
import { useDoc, useEdit } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field, FieldGroup } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { T } from './text'

const SOURCES = [['endTurnAdjacent', T.endTurn], ['fightTogether', T.fight], ['heal', T.heal]] as const

export function SupportRulesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const rules = doc.supports?.rules ?? {}
  const count = Math.min(MAX_LEVELS, Math.max(mostLevels(doc), rules.levelNames?.length ?? 0))
  const names = Array.from({ length: count }, (_, i) => rules.levelNames?.[i] ?? '')
  return (
    <Card title={T.settings} help={T.settingsHelp} id="support-settings">
      <div className="stack" data-testid="support-rules">
        <FieldGroup label={T.levelNames} help={T.levelNamesHelp}>
          <div className="level-names">
            {names.map((n, i) => (
              <TextInput key={i} aria-label={T.levelNameN(i + 1)} placeholder={String(i + 1)} value={n}
                onChange={(v) => edit((d) => setLevelNames(d, names.map((x, k) => (k === i ? v : x))), 'support:levelNames')} />
            ))}
          </div>
        </FieldGroup>
        <FieldGroup label={T.earn} help={T.earnHelp}>
          <FormGrid cols={3}>
            {SOURCES.map(([key, label]) => (
              <Field key={key} label={label}>
                <NumberInput value={rules.points?.[key]} min={0} max={MAX_POINTS} allowEmpty placeholder={T.off} suffix={T.points}
                  onChange={(v) => edit((d) => setPointSource(d, key, v), `support:${key}`)} />
              </Field>
            ))}
          </FormGrid>
        </FieldGroup>
        <FormGrid cols={3}>
          <Field label={T.range} help={T.rangeHelp}>
            <NumberInput value={rules.range ?? DEFAULT_RANGE} min={1} max={10} suffix={T.squares} onChange={(v) => edit((d) => setRange(d, v), 'support:range')} />
          </Field>
          <Field label={T.maxLevels} help={T.maxLevelsHelp}>
            <NumberInput value={rules.maxLevels} min={1} max={99} allowEmpty placeholder={T.noLimit} onChange={(v) => edit((d) => setMaxLevels(d, v), 'support:maxLevels')} />
          </Field>
          <Field label={T.talks} help={T.talksHelp}>
            <Select value={rules.conversations ?? 'both'} options={(['battle', 'camp', 'both'] as SupportTalks[]).map((k) => ({ value: k, label: T.talksOptions[k] }))}
              onChange={(v) => edit((d) => setConversations(d, v as SupportTalks))} />
          </Field>
        </FormGrid>
      </div>
    </Card>
  )
}
