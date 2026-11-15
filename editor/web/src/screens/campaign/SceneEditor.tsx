// One scene (Before / After the battle) of a chapter, edited with the SAME command list and forms as a map's events.
// Those forms work on "a map with an event": here the map is the made-up scene map (model/campaignRoster.ts), its one
// event is the scene, and `apply` writes the result back into the chapter, as one undo step per edit.
import { useMemo, useState } from 'react'
import { sceneAsMap, setSceneMusic, withScene } from '../../model/campaign'
import { sceneMusic } from '../../model/campaignScene'
import { hasMusic } from '../../model/audio'
import { sceneEventId } from '../../model/campaignRoster'
import { SCENE_TITLE } from '../../model/campaignWalk'
import type { SceneKey } from '../../model/campaignTypes'
import { namedUnits } from '../../model/storyIds'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { AudioChoice } from '../sound/AudioChoice'
import { T as SoundT } from '../sound/text'
import { Field } from '../../ui/Field'
import { CommandList } from '../maps/editor/story/CommandList'
import { StoryScope, type Story } from '../maps/editor/story/StoryContext'
import { MapViewState } from '../maps/editor/viewState'
import { C } from './text'

export function SceneEditor({ chapterId, which }: { chapterId: string; which: SceneKey }) {
  const doc = useDoc()
  const edit = useEdit()
  const [selected, setSelected] = useState<string | null>(null)
  // Never used by scene commands (no squares to pick); the story forms just expect the field.
  const vs = useMemo(() => new MapViewState(), [])
  const eventId = sceneEventId(chapterId, which)
  const story = useMemo<Story>(() => {
    const map = sceneAsMap(doc, chapterId, which)
    return {
      doc, map, mapId: '', vs, units: namedUnits(doc, map), teams: [], tags: [],
      scene: { goto: which === 'after' },
      apply: (fn, key) => edit((d) => {
        const m = sceneAsMap(d, chapterId, which)
        const next = fn(m)
        return next === m ? d : withScene(d, chapterId, which, next)
      }, key),
      applyDoc: (fn, key) => edit(fn, key),
    }
  }, [doc, chapterId, which, edit, vs])
  const commands = story.map.events![0].do
  return (
    <Card title={SCENE_TITLE[which]} help={which === 'before' ? C.beforeHelp : C.afterHelp} className="scene-card">
      <div data-testid={`scene-${which}`}>
        {(hasMusic(doc) || sceneMusic(doc.campaign?.chapters.find((c) => c.id === chapterId)?.[which]) !== undefined) && (
          <Field label={SoundT.sceneMusic} help={SoundT.sceneMusicHelp}>
            <AudioChoice kind="music" mode="scene" value={sceneMusic(doc.campaign?.chapters.find((c) => c.id === chapterId)?.[which])}
              aria-label={`${SCENE_TITLE[which]}: ${SoundT.sceneMusic}`} onChange={(v) => edit((d) => setSceneMusic(d, chapterId, which, v))} />
          </Field>
        )}
        <StoryScope value={story}>
          <CommandList eventId={eventId} path={[]} commands={commands} talk={false} selected={selected} onSelect={setSelected} name={SCENE_TITLE[which]} />
        </StoryScope>
      </div>
      <p className="help">{C.speakersHelp}</p>
    </Card>
  )
}
