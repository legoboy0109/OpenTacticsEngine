// "Backdrop for the scenes" (the campaign's) and "Backdrop for this scene" (one Before / After scene's own): docs/EVENTS.md "Scene backdrops".
import { backgroundIds } from '../../model/backgrounds'
import { campaignBackdrop, setCampaignBackdrop, setSceneBackdrop } from '../../model/campaignBackdrop'
import { sceneBackdrop } from '../../model/campaignScene'
import type { SceneKey } from '../../model/campaignTypes'
import { SCENE_TITLE } from '../../model/campaignWalk'
import { useDoc, useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { BD } from './backdropText'

const NONE = '__none__'
const SAME = '__same__'

/** The campaign's backdrop. With no backgrounds in the game (and none named) it only says where to add them. */
export function CampaignBackdropField() {
  const doc = useDoc()
  const edit = useEdit()
  const ids = backgroundIds(doc)
  const cur = campaignBackdrop(doc)
  if (ids.length === 0 && cur === undefined) return <Field label={BD.campaign} help={BD.campaignHelp}><p className="help">{BD.noBackgrounds}</p></Field>
  const options = [{ value: NONE, label: BD.campaignNone }, ...(cur !== undefined && !ids.includes(cur) ? [{ value: cur, label: `${cur} (not found)` }] : []), ...ids.map((id) => ({ value: id, label: id }))]
  return (
    <Field label={BD.campaign} help={BD.campaignHelp}>
      <Select aria-label={BD.campaign} value={cur ?? NONE} options={options} onChange={(v) => edit((d) => setCampaignBackdrop(d, v === NONE ? undefined : v))} />
    </Field>
  )
}

/** One scene's own backdrop: the campaign's (nothing written), a picture, or the plain colour (null). */
export function SceneBackdropField({ chapterId, which }: { chapterId: string; which: SceneKey }) {
  const doc = useDoc()
  const edit = useEdit()
  const ids = backgroundIds(doc)
  const cur = sceneBackdrop(doc.campaign?.chapters.find((c) => c.id === chapterId)?.[which])
  // Nothing to choose from and nothing set: leave the field out.
  if (ids.length === 0 && cur === undefined) return null
  const value = cur === undefined ? SAME : cur === null ? NONE : cur
  const options = [{ value: SAME, label: BD.sceneSame }, { value: NONE, label: BD.sceneNone }, ...(typeof cur === 'string' && !ids.includes(cur) ? [{ value: cur, label: `${cur} (not found)` }] : []), ...ids.map((id) => ({ value: id, label: id }))]
  return (
    <Field label={BD.scene} help={BD.sceneHelp}>
      <Select aria-label={`${SCENE_TITLE[which]}: ${BD.scene}`} value={value} options={options}
        onChange={(v) => edit((d) => setSceneBackdrop(d, chapterId, which, v === SAME ? undefined : v === NONE ? null : v))} />
    </Field>
  )
}
