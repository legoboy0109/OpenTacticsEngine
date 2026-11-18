// "Layouts by distance" inside the Stage card (docs/BATTLE_SCENE.md "Range bands"): extra places for the fighters by fight distance, and the
// one-fighter-at-a-time camera. The model is model/stageBands.ts; the checks mirror the loader.
import { stageOf } from '../../../model/battleStage'
import {
  addBand, bandBackdrop, bandEach, bandPanMs, bandsOf, removeBand, setBandBackdrop, setBandFrom, setBandPan, setBandPanStart, setBandPlatform, setBandPoint, setBandView, type BandGroup, type BandSide,
  BAND_FROM_MAX, BAND_FROM_MIN, BAND_MAX, PAN_START_MAX, WORLD_MAX,
} from '../../../model/stageBands'
import { useDoc, useEdit } from '../../../store/context'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { NumberInput } from '../../../ui/NumberInput'
import { Select } from '../../../ui/Select'
import { Toggle } from '../../../ui/Toggle'
import { ST } from './stageText'

export function StageBands() {
  const doc = useDoc()
  const edit = useEdit()
  const s = stageOf(doc)
  const bands = bandsOf(doc)
  if (!s) return null
  const points = (i: number, group: BandGroup, title: string, maxX: number) => {
    const b = bands[i]
    const pair = b[group]
    if (!pair) return null
    return (
      <div className="stack tight" data-testid={`band-${i}-${group}`}>
        <strong>{title}</strong>
        {(['left', 'right'] as BandSide[]).map((side) => (
          <div className="row wrap" key={side}>
            <span style={{ minWidth: '4rem' }}>{side === 'left' ? ST.left : ST.right}</span>
            {([0, 1] as const).map((axis) => (
              <NumberInput key={axis} aria-label={ST.bandPoint(i + 1, title, side, axis === 0 ? ST.across : ST.down)} value={pair[side][axis]} min={0} max={axis === 0 ? maxX : 1023}
                suffix={axis === 0 ? ST.across : ST.down} onChange={(v) => v !== undefined && edit((d) => setBandPoint(d, i, group, side, axis, v))} />
            ))}
          </div>
        ))}
      </div>
    )
  }
  return (
    <div className="stack tight" data-testid="stage-bands">
      <strong>{ST.bands}</strong>
      <p className="help">{ST.bandsHelp}</p>
      {bands.length === 0 && <p className="help">{ST.bandsNone}</p>}
      {bands.map((b, i) => {
        const each = bandEach(b)
        const maxX = each ? WORLD_MAX : s.width - 1
        return (
          <div className="stack battle-rule" key={i} data-testid={`band-${i}`}>
            <div className="battle-anim-head">
              <strong>{ST.bandTitle(i + 1, b.from)}</strong>
              <button type="button" className="btn sm" aria-label={ST.bandRemove(i + 1)} onClick={() => edit((d) => removeBand(d, i))}><Icon name="trash" />Remove</button>
            </div>
            <div className="row wrap">
              <Field label={ST.bandFrom}>
                <NumberInput aria-label={`Layout ${i + 1}: ${ST.bandFrom}`} min={BAND_FROM_MIN} max={BAND_FROM_MAX} value={b.from} onChange={(v) => v !== undefined && edit((d) => setBandFrom(d, i, v), `band${i}:from`)} />
              </Field>
              <Field label={ST.bandView}>
                <Select aria-label={`Layout ${i + 1}: ${ST.bandView}`} value={each ? 'each' : 'both'} options={[{ value: 'both', label: ST.bandBoth }, { value: 'each', label: ST.bandEach }]}
                  onChange={(v) => edit((d) => setBandView(d, i, v as 'both' | 'each'))} />
              </Field>
            </div>
            {points(i, 'feet', ST.bandFeet, maxX)}
            <Toggle label={ST.bandPlatform} checked={!!b.platform} onChange={(on) => edit((d) => setBandPlatform(d, i, on))} />
            {points(i, 'platform', ST.bandPlatforms, maxX)}
            {each && b.pan && (
              <div className="stack tight" data-testid={`band-${i}-pan`}>
                <strong>{ST.bandPan}</strong>
                <p className="help">{ST.bandPanHelp}</p>
                <div className="row wrap">
                  <Field label={ST.bandPanMs}>
                    <NumberInput aria-label={ST.bandPanField(i + 1, ST.bandPanMs)} min={0} max={5000} value={bandPanMs(b)} onChange={(v) => v !== undefined && edit((d) => setBandPan(d, i, 'panMs', v), `band${i}:panms`)} />
                  </Field>
                  <Field label={ST.bandLeftView}>
                    <NumberInput aria-label={ST.bandPanField(i + 1, ST.bandLeftView)} min={0} max={WORLD_MAX} value={b.pan.leftView} onChange={(v) => v !== undefined && edit((d) => setBandPan(d, i, 'leftView', v), `band${i}:lv`)} />
                  </Field>
                  <Field label={ST.bandRightView}>
                    <NumberInput aria-label={ST.bandPanField(i + 1, ST.bandRightView)} min={0} max={WORLD_MAX} value={b.pan.rightView} onChange={(v) => v !== undefined && edit((d) => setBandPan(d, i, 'rightView', v), `band${i}:rv`)} />
                  </Field>
                </div>
                <div className="row wrap">
                  <Field label={ST.bandBackdrop}>
                    <Select aria-label={ST.bandPanField(i + 1, ST.bandBackdrop)} value={bandBackdrop(b)} options={[{ value: 'fixed', label: ST.bandBackdropFixed }, { value: 'scroll', label: ST.bandBackdropScroll }]}
                      onChange={(v) => edit((d) => setBandBackdrop(d, i, v as 'fixed' | 'scroll'))} />
                  </Field>
                  <Field label={ST.bandPanStart} help={ST.bandPanStartHelp}>
                    <NumberInput aria-label={ST.bandPanField(i + 1, ST.bandPanStart)} allowEmpty min={0} max={PAN_START_MAX} value={b.pan.startMs} onChange={(v) => edit((d) => setBandPanStart(d, i, v), `band${i}:start`)} />
                  </Field>
                </div>
                <p className="help">{ST.bandNoLine}</p>
              </div>
            )}
          </div>
        )
      })}
      {bands.length < BAND_MAX && <div><button type="button" className="btn sm" onClick={() => edit((d) => addBand(d))}><Icon name="plus" />{ST.bandAdd}</button></div>}
    </div>
  )
}
