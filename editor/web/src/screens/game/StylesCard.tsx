import { useState } from 'react'
import { addStyle, moveStyle, newStyleId, removeStyle, setStyleStructureColor, setStyleTerrainColor, updateStyle } from '../../model/game'
import type { Structure, Style, Terrain } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { ColorInput } from '../../ui/ColorInput'
import { ConfirmDialog } from '../../ui/Dialog'
import { Field, FieldGroup } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NameDialog } from '../../ui/NameDialog'
import { TextInput } from '../../ui/TextInput'
import { Toggle } from '../../ui/Toggle'
import { T } from './text'

export function StylesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const styles = doc.game!.styles ?? []
  const [adding, setAdding] = useState(false)
  const [deleting, setDeleting] = useState<Style | null>(null)

  return (
    <Card title={T.looks} help={T.looksHelp}
      actions={<button type="button" className="btn sm" onClick={() => setAdding(true)}><Icon name="plus" />{T.addLook}</button>}>
      {styles.length === 0 && <p className="help">{T.looksEmpty}</p>}
      <div className="stack">
        {styles.map((s, i) => (
          <StyleBox key={s.id} style={s} index={i} count={styles.length} terrain={doc.terrain} structures={doc.structures} onDelete={() => setDeleting(s)} />
        ))}
      </div>
      <NameDialog open={adding} title={T.newLookTitle} label={T.newLookLabel} onCancel={() => setAdding(false)}
        onSubmit={(name) => { const id = newStyleId(doc, name); edit((d) => addStyle(d, name, id)); setAdding(false) }} />
      <ConfirmDialog open={deleting !== null} title={deleting ? T.deleteLookTitle(deleting.name) : ''} confirmLabel={T.removeLook} danger
        onCancel={() => setDeleting(null)} onConfirm={() => { if (deleting) edit((d) => removeStyle(d, deleting.id)); setDeleting(null) }}>
        <p>{T.deleteLookBody}</p>
      </ConfirmDialog>
    </Card>
  )
}

function StyleBox({ style, index, count, terrain, structures, onDelete }: { style: Style; index: number; count: number; terrain: Terrain[] | null; structures: Structure[] | null; onDelete: () => void }) {
  const edit = useEdit()
  const key = `style:${style.id}`
  return (
    <div className="subcard">
      <div className="subcard-head">
        <h3>{style.name || 'Unnamed style'}</h3>
        <div className="card-actions">
          <button type="button" className="btn ghost sm icon" aria-label={`${T.moveLookUp}: ${style.name}`} disabled={index === 0} onClick={() => edit((d) => moveStyle(d, index, index - 1))}><Icon name="up" /></button>
          <button type="button" className="btn ghost sm icon" aria-label={`${T.moveLookDown}: ${style.name}`} disabled={index === count - 1} onClick={() => edit((d) => moveStyle(d, index, index + 1))}><Icon name="down" /></button>
          <button type="button" className="btn ghost sm danger" onClick={onDelete}><Icon name="trash" />{T.removeLook}</button>
        </div>
      </div>
      <div className="stack">
        <Field label={T.lookName}>
          <TextInput value={style.name} onChange={(v) => edit((d) => updateStyle(d, style.id, { name: v }), `${key}:name`)} />
        </Field>
        <Field label={T.background} help={T.backgroundHelp}>
          <div className="row">
            <ColorInput value={style.background} aria-label={`${style.name} ${T.background}`} onChange={(v) => edit((d) => updateStyle(d, style.id, { background: v }), `${key}:bg`)} />
            {style.background !== undefined && (
              <button type="button" className="btn sm" onClick={() => edit((d) => updateStyle(d, style.id, { background: undefined }))}>{T.backgroundReset}</button>
            )}
          </div>
        </Field>
        <Toggle label={T.showGlyphs} help={T.showGlyphsHelp} checked={style.showGlyphs !== false}
          onChange={(v) => edit((d) => updateStyle(d, style.id, { showGlyphs: v ? undefined : false }))} />
        <FieldGroup label={T.terrainColors} help={T.terrainColorsHelp}>
          {terrain === null ? <p className="help">{T.needsTerrain}</p> : (
            <ul className="color-rows">
              {terrain.map((t) => {
                const over = style.terrainColors?.[t.id]
                return (
                  <li key={t.id}>
                    <span className="color-row-name">{t.name}</span>
                    <ColorInput value={over ?? t.color} aria-label={`${style.name}: ${t.name} colour`}
                      onChange={(v) => edit((d) => setStyleTerrainColor(d, style.id, t.id, v), `${key}:tc:${t.id}`)} />
                    {over !== undefined
                      ? <button type="button" className="btn sm" onClick={() => edit((d) => setStyleTerrainColor(d, style.id, t.id, undefined))}>{T.terrainReset}</button>
                      : <span className="help">{T.terrainDefault}</span>}
                  </li>
                )
              })}
            </ul>
          )}
        </FieldGroup>
        {(structures?.length ?? 0) > 0 && (
          <FieldGroup label={T.structureColors} help={T.structureColorsHelp}>
            <ul className="color-rows">
              {structures!.map((s) => {
                const over = style.structureColors?.[s.id]
                return (
                  <li key={s.id}>
                    <span className="color-row-name">{s.name}</span>
                    <ColorInput value={over ?? s.color} aria-label={`${style.name}: ${s.name} colour`}
                      onChange={(v) => edit((d) => setStyleStructureColor(d, style.id, s.id, v), `${key}:sc:${s.id}`)} />
                    {over !== undefined
                      ? <button type="button" className="btn sm" onClick={() => edit((d) => setStyleStructureColor(d, style.id, s.id, undefined))}>{T.terrainReset}</button>
                      : <span className="help">{T.terrainDefault}</span>}
                  </li>
                )
              })}
            </ul>
          </FieldGroup>
        )}
      </div>
    </div>
  )
}
