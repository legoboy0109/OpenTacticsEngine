import { hasPrice } from '../../model/items'
import { suggestedTerrainProps } from '../../model/formulas'
import { defeatConditions, featureEnabled } from '../../model/rules'
import {
  SIZE_MAX, SIZE_MIN, isHeadquarters, setCapturePoints, setHeadquarters, setProduces, setSize, setStructureIncome, setStructureNumber,
  setSells, setStructureTags, sizeOf, updateStructure,
} from '../../model/structures'
import { Link, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Advanced, Card, FormGrid } from '../../ui/Card'
import { ChipsInput } from '../../ui/ChipsInput'
import { ColorInput } from '../../ui/ColorInput'
import { Field } from '../../ui/Field'
import { KeyNumberTable } from '../../ui/KeyNumberTable'
import { MultiSelect } from '../../ui/MultiSelect'
import { NumberInput } from '../../ui/NumberInput'
import { TextInput } from '../../ui/TextInput'
import { PictureCard } from '../../ui/picture/PictureCard'
import { Check } from '../../ui/Toggle'
import { repairOn } from '../../model/supplyRules'
import { Footprint } from './Footprint'
import { MovingThrough } from './MovingThrough'
import { RepairsCard, ResearchHereCard } from './RepairsCard'
import { UpgradesCard } from './UpgradesCard'
import { T } from './text'

const titleCase = (s: string) => s.charAt(0).toUpperCase() + s.slice(1)

export function StructureForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const game = doc.game!
  const s = doc.structures?.find((x) => x.id === id)
  if (!s) return null
  const key = `structure:${id}`
  const { w, h } = sizeOf(s)
  const suggested = suggestedTerrainProps(game)
  const labelOf = new Map(suggested.map((x) => [x.id, x.label]))
  const propRows = [...new Set([...suggested.map((x) => x.id), ...Object.keys(s.props ?? {})])].map((k) => ({ key: k, label: labelOf.get(k) ?? titleCase(k) }))
  const resources = game.resources ?? []
  const capturable = (s.capturePoints ?? 0) > 0
  const hq = isHeadquarters(s)
  // Only items with a price can be sold; one already listed without a price stays visible (and is flagged) so it can be removed.
  const items = doc.items ?? []
  const sells = s.sells ?? []
  const sellable = items.filter((x) => hasPrice(x) || sells.includes(x.id)).map((x) => ({ value: x.id, label: hasPrice(x) ? x.name : T.noPrice(x.name) }))
  const showSells = items.length > 0 || sells.length > 0
  const otherTags = (s.tags ?? []).filter((x) => x !== 'hq')

  // A setting shows when its rule is on or the structure already has it (then with a hint); otherwise one line says it is hidden.
  const captureOn = featureEnabled(doc, 'capture'), economyOn = featureEnabled(doc, 'economy'), productionOn = featureEnabled(doc, 'production')
  const hqOn = defeatConditions(doc).includes('lostHq')
  const hasIncome = Object.values(s.income ?? {}).some((v) => v)
  const hasProduces = (s.produces?.length ?? 0) > 0
  const showCapture = captureOn || capturable
  const showIncome = (economyOn || hasIncome) && resources.length > 0
  const showProduces = productionOn || hasProduces
  const showHq = hqOn || hq
  const showRepairs = repairOn(doc) || s.repairs !== undefined
  const hidden = [!showCapture && T.hiddenCapture, !(economyOn || hasIncome) && T.hiddenIncome, !showProduces && T.hiddenProduces, !showHq && T.hiddenHq, !showRepairs && T.hiddenRepairs].filter(Boolean)

  return (
    <div className="stack">
      <Card>
        <FormGrid>
          <Field label={T.name} help={T.nameHelp}>
            <TextInput value={s.name} onChange={(v) => edit((d) => updateStructure(d, id, { name: v }), `${key}:name`)} />
          </Field>
          <Field label={T.glyph} help={T.glyphHelp}>
            <TextInput value={s.glyph ?? ''} maxLength={1} className="glyph-input"
              onChange={(v) => edit((d) => updateStructure(d, id, { glyph: Array.from(v)[0] }), `${key}:glyph`)} />
          </Field>
          <Field label={T.color} help={T.colorHelp}>
            <ColorInput value={s.color} aria-label={`${s.name} ${T.color.toLowerCase()}`} onChange={(v) => edit((d) => updateStructure(d, id, { color: v }), `${key}:color`)} />
          </Field>
        </FormGrid>
      </Card>

      <Card title={T.size} help={T.sizeHelp} id={`${key}:size`}>
        <div className="size-row">
          <FormGrid>
            <Field label={T.width}>
              <NumberInput value={w} min={SIZE_MIN} max={SIZE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setSize(d, id, v, h), `${key}:w`) }} />
            </Field>
            <Field label={T.height}>
              <NumberInput value={h} min={SIZE_MIN} max={SIZE_MAX} onChange={(v) => { if (v !== undefined) edit((d) => setSize(d, id, w, v), `${key}:h`) }} />
            </Field>
          </FormGrid>
          <div className="stack tight">
            <Footprint w={w} h={h} color={s.color} />
            <p className="help">{T.footprint(w, h)}</p>
          </div>
        </div>
        <p className="help">{T.sizeChanged}</p>
      </Card>

      <PictureCard kind="structures" id={id} name={s.name} />

      <Card><MovingThrough id={id} /></Card>

      <Card>
        <KeyNumberTable label={T.numbers} help={T.numbersHelp} rows={propRows}
          columns={[{ title: T.numberCol, values: s.props, integer: false, placeholder: T.numberPlaceholder, onChange: (k, v) => edit((d) => setStructureNumber(d, id, k, v), `${key}:prop:${k}`) }]}
          addOptions={suggested.map((x) => ({ key: x.id, label: x.label }))} onAdd={(k) => edit((d) => setStructureNumber(d, id, k, 0))} addLabel={T.addNumber} />
      </Card>

      {(showCapture || showHq) && (
        <Card>
          <div className="stack">
            {showCapture && (
              <>
                <Check label={T.capture} help={T.captureHelp} checked={capturable} onChange={(on) => edit((d) => setCapturePoints(d, id, on ? 20 : undefined))} />
                {capturable && (
                  <Field label={T.capturePoints} help={T.capturePointsHelp}>
                    <NumberInput value={s.capturePoints} min={1} onChange={(v) => { if (v !== undefined) edit((d) => setCapturePoints(d, id, v), `${key}:capture`) }} />
                  </Field>
                )}
                {capturable && !captureOn && <p className="help">{T.captureOff}</p>}
              </>
            )}
            {showHq && (
              <>
                <Check label={T.hq} help={T.hqHelp} checked={hq} onChange={(on) => edit((d) => setHeadquarters(d, id, on))} />
                {hq && !hqOn && <p className="help">{T.hqOff}</p>}
              </>
            )}
          </div>
        </Card>
      )}

      {(economyOn || hasIncome) && (
        <Card>
          {resources.length === 0
            ? <p className="help">{T.incomeNone}</p>
            : (
              <div className="stack">
                <KeyNumberTable label={T.income} help={T.incomeHelp} rows={resources.map((r) => ({ key: r.id, label: r.name }))}
                  columns={[{ title: T.incomeCol, values: s.income, min: 0, placeholder: '0', onChange: (k, v) => edit((d) => setStructureIncome(d, id, k, v), `${key}:income:${k}`) }]} />
                {showIncome && !economyOn && <p className="help">{T.incomeOff}</p>}
              </div>
            )}
        </Card>
      )}

      {showProduces && (
        <Card>
          <MultiSelect label={T.produces} help={T.producesHelp} value={s.produces ?? []} options={(doc.units ?? []).map((u) => ({ value: u.id, label: u.name }))}
            onChange={(v) => edit((d) => setProduces(d, id, v))} />
          {hasProduces && !productionOn && <p className="help">{T.producesOff}</p>}
        </Card>
      )}

      {showSells && (
        <Card>
          <MultiSelect label={T.sells} help={T.sellsHelp} emptyText={T.sellsNone} value={sells} options={sellable}
            onChange={(v) => edit((d) => setSells(d, id, v))} />
          <p className="help">{T.sellsRule}</p>
        </Card>
      )}

      <ResearchHereCard id={id} />
      <UpgradesCard id={id} />
      {showRepairs && <RepairsCard id={id} />}

      {hidden.length > 0 && (
        <p className="help" data-testid="hidden-settings">{T.hiddenBody(hidden.join(', '))} <Link to={projectPath(store.id, 'rules')}>{T.rulesLink}</Link></p>
      )}

      <Advanced label={T.advanced}>
        <div className="stack">
          <Field label={T.tags} help={T.tagsHelp}>
            <ChipsInput value={otherTags} onChange={(v) => edit((d) => setStructureTags(d, id, [...(hq ? ['hq'] : []), ...v]))} />
          </Field>
          <Field label={T.id} help={T.idHelp}><TextInput value={s.id} readOnly mono /></Field>
        </div>
      </Advanced>
    </div>
  )
}
