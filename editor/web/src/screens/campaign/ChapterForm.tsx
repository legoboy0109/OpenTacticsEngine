// The open chapter: its name, map, next chapter and the two scenes.
import { setChapterMap, setChapterName, setChapterNext } from '../../model/campaign'
import { chapterName } from '../../model/campaignTypes'
import { Link, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Card, FormGrid } from '../../ui/Card'
import { Field } from '../../ui/Field'
import { Select } from '../../ui/Select'
import { TextInput } from '../../ui/TextInput'
import { SceneEditor } from './SceneEditor'
import { C } from './text'

const DEFAULT_NEXT = '__default__'

export function ChapterForm({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const campaign = doc.campaign!
  const chapter = campaign.chapters.find((c) => c.id === id)
  if (!chapter) return null
  const key = `chapter:${id}`
  const maps = doc.game?.maps ?? []
  const mapKnown = maps.some((m) => m.id === chapter.map)
  const mapOptions = [
    ...(mapKnown ? [] : [{ value: chapter.map, label: chapter.map ? `${chapter.map} (not there any more)` : C.mapChoose, disabled: chapter.map === '' }]),
    ...maps.map((m) => ({ value: m.id, label: m.name || m.id })),
  ]
  // "Next chapter": the default (the following chapter), or any chapter; a value that names nothing stays visible.
  const nextKnown = !chapter.next || campaign.chapters.some((c) => c.id === chapter.next)
  const nextOptions = [
    { value: DEFAULT_NEXT, label: C.nextDefault },
    ...(nextKnown ? [] : [{ value: chapter.next!, label: `${chapter.next} (not there any more)` }]),
    ...campaign.chapters.filter((c) => c.id !== id).map((c) => ({ value: c.id, label: chapterName(c) })),
  ]
  return (
    <div className="stack" data-testid="chapter-form">
      <Card>
        <div className="stack">
          <FormGrid>
            <Field label={C.chapterName} help={C.chapterNameHelp}>
              <TextInput value={chapter.name ?? ''} onChange={(v) => edit((d) => setChapterName(d, id, v), `${key}:name`)} />
            </Field>
            <Field label={C.chapterId} help={C.chapterIdHelp}><TextInput value={chapter.id} readOnly mono /></Field>
            <Field label={C.map} help={C.mapHelp}>
              <Select value={chapter.map} options={mapOptions}
                onChange={(v) => edit((d) => setChapterMap(d, id, v))} />
              {mapKnown && <p className="help"><Link to={projectPath(store.id, 'maps', chapter.map)}>{C.openMap}</Link></p>}
            </Field>
            <Field label={C.next} help={C.nextHelp}>
              <Select value={chapter.next ? chapter.next : DEFAULT_NEXT} options={nextOptions}
                onChange={(v) => edit((d) => setChapterNext(d, id, v === DEFAULT_NEXT ? undefined : v))} />
            </Field>
          </FormGrid>
        </div>
      </Card>
      <SceneEditor chapterId={id} which="before" />
      <SceneEditor chapterId={id} which="after" />
    </div>
  )
}
