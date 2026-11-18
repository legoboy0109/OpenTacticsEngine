// "Stage" on the Art screen: the optional fixed stage of the battle scene (docs/BATTLE_SCENE.md "Fixed stage"): its size, where each
// fighter stands (close and at range), where the platforms are, which part stays visible, how it fits, and the range line.
import { useState } from 'react'
import {
  defaultBackdropSize, enableStage, removeStage, setStageAlign, setStageFit, setStageGroup, setStagePoint, setStageRangeLine,
  setStageSize, stageAlign, stageFit, stageOf, stageProblems, stageRangeLine, type PointGroup, type Side, type StageAlign, type StageFit,
} from '../../../model/battleStage'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { Toggle } from '../../../ui/Toggle'
import { StagePreview } from './StagePreview'
import { ST } from './stageText'

const GROUP_TITLE: Record<PointGroup, string> = { feet: ST.feet, feetFar: ST.feetFar, platform: ST.platform, platformFar: ST.platformFar }
const GROUP_HELP: Partial<Record<PointGroup, string>> = { feet: ST.feetHelp, feetFar: ST.feetFarHelp, platform: ST.platformHelp }

export function StageCard() {
  const doc = useDoc()
  const edit = useEdit()
  const s = stageOf(doc)
  const [far, setFar] = useState(false)
  const problems = stageProblems(doc)
  const backdrop = defaultBackdropSize(doc)
  const groupOn = (g: PointGroup) => !!s?.[g]
  const row = (g: PointGroup) => {
    if (!s || !s[g]) return null
    return (
      <div className="stack tight" key={g} data-testid={`stage-${g}`}>
        <strong>{GROUP_TITLE[g]}</strong>
        {GROUP_HELP[g] && <p className="help">{GROUP_HELP[g]}</p>}
        {(['left', 'right'] as Side[]).map((side) => (
          <div className="row wrap" key={side}>
            <span style={{ minWidth: '4rem' }}>{side === 'left' ? ST.left : ST.right}</span>
            {([0, 1] as const).map((axis) => (
              <NumberInput key={axis} aria-label={ST.point(GROUP_TITLE[g], side, axis === 0 ? ST.across : ST.down)} value={s[g]![side][axis]} min={0} max={1023}
                suffix={axis === 0 ? ST.across : ST.down} onChange={(v) => v !== undefined && edit((d) => setStagePoint(d, g, side, axis, v))} />
            ))}
          </div>
        ))}
      </div>
    )
  }
  return (
    <Card title={ST.title} help={ST.help} id="battle-stage">
      <div className="stack" data-testid="battle-stage">
        <Toggle label={ST.use} checked={!!s} onChange={(on) => edit((d) => (on ? enableStage(d) : removeStage(d)))} />
        {!s && <p className="help">{ST.needStage}</p>}
        {s && (
          <>
            <div className="row wrap">
              <Field label={ST.width}>
                <NumberInput aria-label={ST.width} value={s.width} min={16} max={1024} suffix={ST.pixels} onChange={(v) => v !== undefined && edit((d) => setStageSize(d, 'width', v))} />
              </Field>
              <Field label={ST.height}>
                <NumberInput aria-label={ST.height} value={s.height} min={16} max={1024} suffix={ST.pixels} onChange={(v) => v !== undefined && edit((d) => setStageSize(d, 'height', v))} />
              </Field>
            </div>
            {backdrop && <p className="help" data-testid="stage-backdrop-note">{backdrop.width === s.width && backdrop.height === s.height ? ST.backdropSame(backdrop.width, backdrop.height) : ST.backdropDiff(backdrop.width, backdrop.height)}</p>}
            {row('feet')}
            <Toggle label={ST.separateFar} checked={groupOn('feetFar')} onChange={(on) => edit((d) => setStageGroup(d, 'feetFar', on))} />
            {row('feetFar')}
            <Toggle label={ST.separatePlatform} checked={groupOn('platform')} onChange={(on) => edit((d) => setStageGroup(d, 'platform', on))} />
            {row('platform')}
            {groupOn('platform') && <Toggle label={ST.platformFar} checked={groupOn('platformFar')} onChange={(on) => edit((d) => setStageGroup(d, 'platformFar', on))} />}
            {row('platformFar')}
            <Field label={ST.align} help={ST.alignHelp}>
              <Select aria-label={ST.align} value={stageAlign(s)} onChange={(v) => edit((d) => setStageAlign(d, v as StageAlign))}
                options={[{ value: 'top', label: ST.alignTop }, { value: 'center', label: ST.alignCenter }, { value: 'bottom', label: ST.alignBottom }]} />
            </Field>
            <Field label={ST.fit}>
              <Select aria-label={ST.fit} value={stageFit(s)} onChange={(v) => edit((d) => setStageFit(d, v as StageFit))}
                options={[{ value: 'cover', label: ST.fitCover }, { value: 'contain', label: ST.fitContain }]} />
            </Field>
            <Toggle label={ST.rangeLine} checked={stageRangeLine(s)} onChange={(on) => edit((d) => setStageRangeLine(d, on))} />
            <div className="stack tight">
              <strong>{ST.preview}</strong>
              {(s.feetFar || s.platformFar) && (
                <div role="radiogroup" aria-label={ST.preview} className="row">
                  {[false, true].map((f) => (
                    <button key={String(f)} type="button" role="radio" aria-checked={far === f} className={`btn sm${far === f ? ' primary' : ''}`} onClick={() => setFar(f)}>
                      {f ? 'At range' : 'Close'}
                    </button>
                  ))}
                </div>
              )}
              <StagePreview far={far && !!(s.feetFar || s.platformFar)} />
              <p className="help">{ST.previewNote}</p>
            </div>
          </>
        )}
        {problems.map((p) => <p key={p} className="error-text" role="alert">{p}</p>)}
      </div>
    </Card>
  )
}
