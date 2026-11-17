// "Music by situation" and "Steps by movement type" (docs/SOUND.md): the game's music for each side's turn, the track for when few
// enemies are left, and the step sound of each movement type. Each row is a picker; a map can set its own (MapMusic.tsx).
import { musicOf } from '../../model/audio'
import { allSideIds } from '../../model/campaignRoster'
import { FEW_COUNT_MAX, STEP_INTERVAL_MAX, STEP_INTERVAL_MIN, moveTypeStep, setMoveTypeInterval, setMoveTypeStep, setUseFew, setUsePhaseMusic } from '../../model/audioSituations'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { AudioChoice } from './AudioChoice'
import { T } from './text'

export function SituationsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const sides = [...allSideIds(doc)]
  const few = doc.audio?.use?.fewEnemies
  const moveTypes = doc.game?.moveTypes ?? []
  const hasTracks = Object.keys(musicOf(doc)).length > 0
  return (
    <Card title={T.situationsTitle} help={T.situationsHelp} id="situations" className="sound-card">
      <div className="stack" data-testid="situations-card">
        {hasTracks && (
          <section className="slot-group" aria-label={T.phaseGroup}>
            <h3>{T.phaseGroup}</h3>
            <p className="help">{T.phaseHelp}</p>
            {sides.length === 0 ? <p className="help">{T.phaseNoSides}</p> : (
              <ul className="slot-list">
                {sides.map((side) => (
                  <li className="slot-row" key={side} data-testid={`phase-${side}`}>
                    <span className="slot-label">{side}</span>
                    <AudioChoice kind="music" mode="nothing" value={doc.audio?.use?.phaseMusic?.[side]} aria-label={`${side}: ${T.phaseGroup}`}
                      onChange={(v) => edit((d) => setUsePhaseMusic(d, side, v))} />
                    <span className="play-gap" aria-hidden="true" />
                  </li>
                ))}
              </ul>
            )}
          </section>
        )}
        {hasTracks && (
          <section className="slot-group" aria-label={T.fewGroup}>
            <h3>{T.fewGroup}</h3>
            <p className="help">{T.fewHelp}</p>
            <Field label={T.fewTrack}>
              <AudioChoice kind="music" mode="nothing" value={few?.music || undefined} aria-label={T.fewTrack}
                onChange={(v) => edit((d) => setUseFew(d, v === undefined ? undefined : { music: v, count: few?.count ?? 2 }))} />
            </Field>
            {few?.music !== undefined && (
              <Field label={T.fewCount}>
                <NumberInput aria-label={T.fewCount} min={1} max={FEW_COUNT_MAX} value={few.count ?? 2} suffix={T.fewCountSuffix}
                  onChange={(v) => v !== undefined && edit((d) => setUseFew(d, { music: few.music, count: v }), 'few-count')} />
              </Field>
            )}
          </section>
        )}
        <section className="slot-group" aria-label={T.stepsTitle} data-testid="steps-by-move-type">
          <h3>{T.stepsTitle}</h3>
          <p className="help">{T.stepsHelp}</p>
          <ul className="slot-list">
            {moveTypes.map((mt) => {
              const s = moveTypeStep(doc, mt.id)
              const name = mt.name || mt.id
              return (
                <li className="steps-row" key={mt.id} data-testid={`steps-${mt.id}`}>
                  <span className="slot-label">{name}</span>
                  <AudioChoice kind="sound" mode="game" value={s.step} aria-label={T.stepsSound(name)} onChange={(v) => edit((d) => setMoveTypeStep(d, mt.id, v))} />
                  <Field label={T.stepsInterval(name)} help={T.stepsIntervalHelp}>
                    <NumberInput aria-label={T.stepsInterval(name)} allowEmpty integer={false} min={STEP_INTERVAL_MIN} max={STEP_INTERVAL_MAX} placeholder="0.1" suffix={T.stepsIntervalSuffix}
                      value={s.interval} onChange={(v) => edit((d) => setMoveTypeInterval(d, mt.id, v), `steps-${mt.id}-interval`)} />
                  </Field>
                </li>
              )
            })}
          </ul>
        </section>
      </div>
    </Card>
  )
}
