// "Platforms": the picture each fighter of the battle scene stands on, the default one, and a `far` picture for fights at range 2+
// (docs/BATTLE_SCENE.md "Platforms"). Per terrain they are chosen on the Terrain screen and per map in the map's settings.
import { useState } from 'react'
import { addPlatform, clearPlatformFar, defaultPlatform, newPlatformId, platformList, platformProblems, removePlatform, setDefaultPlatform, setPlatformPicture, setPlatformRight } from '../../../model/battleTables'
import { sheetsOf } from '../../../model/graphics'
import { tidyId } from '../../../model/storyIds'
import { useDoc, useEdit } from '../../../store/context'
import { Card } from '../../../ui/Card'
import { Field } from '../../../ui/Field'
import { Icon } from '../../../ui/Icon'
import { Select } from '../../../ui/Select'
import { PortraitPicker } from '../PortraitPicker'
import { PortraitThumb } from '../PortraitThumb'
import { PT } from './platformText'

export function PlatformsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = platformList(doc)
  const [choosing, setChoosing] = useState<{ id: string; far: boolean; right?: boolean } | null>(null)
  const [text, setText] = useState('')
  const hasImages = sheetsOf(doc).length > 0
  const id = newPlatformId(doc, text || 'ground')
  const add = () => { if (text) { edit((d) => addPlatform(d, id)); setText('') } }
  const target = choosing ? list.find((p) => p.id === choosing.id) : undefined
  const problems = platformProblems(doc)
  return (
    <Card title={PT.title} help={PT.help} id="battle-platforms">
      <div className="stack tight" data-testid="battle-platforms">
        {!hasImages && <p className="help">{PT.needImage}</p>}
        {hasImages && list.length === 0 && <p className="help">{PT.none}</p>}
        <ul className="portrait-list" aria-label={PT.title}>
          {list.map((p) => (
            <li key={p.id} className="portrait-row" data-testid={`platform-${p.id}`}>
              <PortraitThumb sheet={p.sheet} cell={p.cell} label={PT.name(p.id)} height={32} />
              <strong>{p.id}</strong>
              <button type="button" className="btn sm" onClick={() => setChoosing({ id: p.id, far: false })}>{PT.choose}</button>
              {p.far
                ? <>
                    <PortraitThumb sheet={p.sheet} cell={p.far} label={PT.farName(p.id)} height={32} />
                    <button type="button" className="btn sm" onClick={() => setChoosing({ id: p.id, far: true })}>{PT.chooseFar}</button>
                    <button type="button" className="btn sm" aria-label={PT.clearFar(p.id)} onClick={() => edit((d) => clearPlatformFar(d, p.id))}>{PT.clearFarText}</button>
                  </>
                : <button type="button" className="btn sm" onClick={() => setChoosing({ id: p.id, far: true })}>{PT.addFar}</button>}
              {p.right && <PortraitThumb sheet={p.sheet} cell={p.right} label={PT.rightName(p.id)} height={32} />}
              {p.farRight && <PortraitThumb sheet={p.sheet} cell={p.farRight} label={PT.farRightName(p.id)} height={32} />}
              <button type="button" className="btn sm" onClick={() => setChoosing({ id: p.id, far: false, right: true })}>{p.right ? PT.chooseRight : PT.addRight}</button>
              {p.right && <button type="button" className="btn sm" aria-label={PT.clearRight(p.id)} onClick={() => edit((d) => setPlatformRight(d, p.id, undefined))}>{PT.clearRightText}</button>}
              {p.far && <button type="button" className="btn sm" onClick={() => setChoosing({ id: p.id, far: true, right: true })}>{p.farRight ? PT.chooseFarRight : PT.addFarRight}</button>}
              {p.farRight && <button type="button" className="btn sm" aria-label={PT.clearFarRight(p.id)} onClick={() => edit((d) => setPlatformRight(d, p.id, undefined, true))}>{PT.clearFarRightText}</button>}
              <button type="button" className="btn sm" aria-label={PT.delete(p.id)} onClick={() => edit((d) => removePlatform(d, p.id))}><Icon name="trash" />{PT.delete(p.id)}</button>
            </li>
          ))}
        </ul>
        {hasImages && (
          <div className="add-row">
            <input className="input" aria-label={PT.newName} placeholder={PT.newName} value={text} autoComplete="off" spellCheck={false}
              onChange={(e) => setText(tidyId(e.target.value))}
              onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); add() } }} />
            <button type="button" className="btn sm" disabled={!text} onClick={add}><Icon name="plus" />{PT.add}</button>
          </div>
        )}
        {list.length > 0 && (
          <Field label={PT.default}>
            <Select aria-label={PT.default} value={defaultPlatform(doc)} options={[{ value: '', label: PT.noDefault }, ...list.map((p) => ({ value: p.id, label: p.id }))]}
              onChange={(v) => edit((d) => setDefaultPlatform(d, v))} />
          </Field>
        )}
        {problems.map((p) => <p key={p} className="error-text" role="alert">{p}</p>)}
      </div>
      {target && choosing && (
        <PortraitPicker id={target.id}
          current={{ sheet: target.sheet, cell: (choosing.right ? (choosing.far ? target.farRight : target.right) : undefined) ?? (choosing.far && target.far ? target.far : target.cell) }}
          onClose={() => setChoosing(null)}
          onChoose={(sheet, cell) => {
            // (a picture for the right-hand fighter comes from the platform's own image: only the piece is chosen)
            edit((d) => (choosing.right ? setPlatformRight(d, target.id, cell, choosing.far) : setPlatformPicture(d, target.id, sheet, cell, choosing.far)))
            setChoosing(null)
          }} />
      )}
    </Card>
  )
}
