// "Unit badges": a small picture drawn on the map sprite of every unit that carries a tag (a boss marker). Each badge names its image (the whole
// image or one cell), the tag, whose units wear it, the corner of the unit's square it sits in and a small offset (docs/GRAPHICS.md "Unit badges").
import { BADGE_ANCHORS, BADGE_OFFSET_MAX, addBadge, badgeList, badgeProblems, badgeTags, moveBadge, patchBadge, removeBadge, type BadgeAnchor } from '../../model/badges'
import { sheetsOf } from '../../model/graphics'
import type { Badge } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Icon } from '../../ui/Icon'
import { NumberInput } from '../../ui/NumberInput'
import { RowButtons } from '../../ui/RowButtons'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { Toggle } from '../../ui/Toggle'
import { T } from './text'

const sidesWord = (b: Badge): string => (Array.isArray(b.sides) ? 'list' : b.sides ?? 'all')

export function BadgesCard() {
  const doc = useDoc()
  const edit = useEdit()
  const list = badgeList(doc)
  const sheets = sheetsOf(doc)
  const tags = badgeTags(doc)
  const problems = badgeProblems(doc)
  return (
    <Card title={T.badgesTitle} help={T.badgesHelp} id="badges">
      <div className="stack tight" data-testid="badges">
        {sheets.length === 0 && <p className="help">{T.badgesNeedImage}</p>}
        {sheets.length > 0 && list.length === 0 && <p className="help">{T.badgesNone}</p>}
        {list.map((b, i) => (
          <div key={i} className="battle-fields" data-testid={`badge-${i + 1}`}>
            <Field label={T.badgeName}>
              <TextInput aria-label={T.badgeNameN(i + 1)} value={b.id} onChange={(v) => edit((d) => patchBadge(d, i, { id: v }), `badge:${i}:id`)} />
            </Field>
            <Field label={T.badgeImage}>
              <Select aria-label={T.badgeImageN(i + 1)} value={b.sheet} options={sheets.map((s) => ({ value: s.id, label: s.name || s.id }))}
                onChange={(v) => edit((d) => patchBadge(d, i, { sheet: v }))} />
            </Field>
            <Toggle label={T.badgeOneCell} checked={b.cell !== undefined} onChange={(on) => edit((d) => patchBadge(d, i, { cell: on ? [0, 0] : undefined }))} />
            {b.cell !== undefined && (
              <>
                <Field label={T.badgeCol}><NumberInput aria-label={T.badgeColN(i + 1)} min={0} max={30000} value={b.cell[0]} onChange={(v) => v !== undefined && edit((d) => patchBadge(d, i, { cell: [v, b.cell![1]] }), `badge:${i}:col`)} /></Field>
                <Field label={T.badgeRow}><NumberInput aria-label={T.badgeRowN(i + 1)} min={0} max={30000} value={b.cell[1]} onChange={(v) => v !== undefined && edit((d) => patchBadge(d, i, { cell: [b.cell![0], v] }), `badge:${i}:row`)} /></Field>
              </>
            )}
            <Field label={T.badgeTag} help={tags.length ? tags.join(', ') : T.badgeNoTags}>
              <TextInput aria-label={T.badgeTagN(i + 1)} value={b.tag} placeholder="boss" onChange={(v) => edit((d) => patchBadge(d, i, { tag: v }), `badge:${i}:tag`)} />
            </Field>
            <Field label={T.badgeSides}>
              <Select aria-label={T.badgeSidesN(i + 1)} value={sidesWord(b)}
                options={[{ value: 'all', label: T.badgeSidesAll }, { value: 'computer', label: T.badgeSidesComputer }, { value: 'player', label: T.badgeSidesPlayer }, ...(Array.isArray(b.sides) ? [{ value: 'list', label: T.badgeSidesList(b.sides.join(', ')) }] : [])]}
                onChange={(v) => v !== 'list' && edit((d) => patchBadge(d, i, { sides: v as 'all' | 'computer' | 'player' }))} />
            </Field>
            <Field label={T.badgeAnchor}>
              <Select aria-label={T.badgeAnchorN(i + 1)} value={b.anchor ?? 'bottomRight'} options={BADGE_ANCHORS.map((a) => ({ value: a, label: T.badgeAnchors[a] }))}
                onChange={(v) => edit((d) => patchBadge(d, i, { anchor: v as BadgeAnchor }))} />
            </Field>
            <Field label={T.badgeOffsetX}><NumberInput aria-label={T.badgeOffsetXN(i + 1)} min={-BADGE_OFFSET_MAX} max={BADGE_OFFSET_MAX} value={b.offset?.[0] ?? 0} onChange={(v) => v !== undefined && edit((d) => patchBadge(d, i, { offset: [v, b.offset?.[1] ?? 0] }), `badge:${i}:ox`)} /></Field>
            <Field label={T.badgeOffsetY}><NumberInput aria-label={T.badgeOffsetYN(i + 1)} min={-BADGE_OFFSET_MAX} max={BADGE_OFFSET_MAX} value={b.offset?.[1] ?? 0} onChange={(v) => v !== undefined && edit((d) => patchBadge(d, i, { offset: [b.offset?.[0] ?? 0, v] }), `badge:${i}:oy`)} /></Field>
            <RowButtons index={i} count={list.length} up={T.badgeUp(i + 1)} down={T.badgeDown(i + 1)} del={T.badgeDelete(i + 1)}
              onUp={() => edit((d) => moveBadge(d, i, i - 1))} onDown={() => edit((d) => moveBadge(d, i, i + 1))} onDelete={() => edit((d) => removeBadge(d, i))} />
          </div>
        ))}
        {sheets.length > 0 && (
          <div className="add-row">
            <button type="button" className="btn sm" onClick={() => edit((d) => addBadge(d))}><Icon name="plus" />{T.badgeAdd}</button>
          </div>
        )}
        {problems.map((p) => <p key={p} className="error-text" role="alert">{p}</p>)}
      </div>
    </Card>
  )
}
