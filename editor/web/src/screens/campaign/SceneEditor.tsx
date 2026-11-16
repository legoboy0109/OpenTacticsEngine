// One scene (Before / After the battle) of a chapter, edited with the SAME command list and forms as a map's events
// (SceneCommands.tsx holds the shared part; here only what is about a chapter: its title, music and speakers help).
import { useMemo } from 'react'
import { sceneAsMap, sceneOf, setSceneMusic, withScene } from '../../model/campaign'
import { sceneMusic } from '../../model/campaignScene'
import { hasMusic } from '../../model/audio'
import { sceneEventId } from '../../model/campaignRoster'
import { SCENE_TITLE } from '../../model/campaignWalk'
import type { SceneKey } from '../../model/campaignTypes'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { AudioChoice } from '../sound/AudioChoice'
import { T as SoundT } from '../sound/text'
import { Field } from '../../ui/Field'
import { SceneCommands, type SceneSource } from './SceneCommands'
import { C } from './text'

export function SceneEditor({ chapterId, which }: { chapterId: string; which: SceneKey }) {
  const doc = useDoc()
  const edit = useEdit()
  const source = useMemo<SceneSource>(() => ({
    eventId: sceneEventId(chapterId, which),
    read: (d) => sceneOf(d, chapterId, which),
    // `withScene` takes the scene map's edited event back out; the made-up map is rebuilt around the new list.
    write: (d, list) => withScene(d, chapterId, which, { ...sceneAsMap(d, chapterId, which), events: [{ id: sceneEventId(chapterId, which), when: { battle: 'start' }, do: list }] }),
    goto: which === 'after',
  }), [chapterId, which])
  const music = sceneMusic(doc.campaign?.chapters.find((c) => c.id === chapterId)?.[which])
  return (
    <Card title={SCENE_TITLE[which]} help={which === 'before' ? C.beforeHelp : C.afterHelp} className="scene-card">
      <div data-testid={`scene-${which}`}>
        {(hasMusic(doc) || music !== undefined) && (
          <Field label={SoundT.sceneMusic} help={SoundT.sceneMusicHelp}>
            <AudioChoice kind="music" mode="scene" value={music}
              aria-label={`${SCENE_TITLE[which]}: ${SoundT.sceneMusic}`} onChange={(v) => edit((d) => setSceneMusic(d, chapterId, which, v))} />
          </Field>
        )}
        <SceneCommands source={source} name={SCENE_TITLE[which]} />
      </div>
      <p className="help">{C.speakersHelp}</p>
    </Card>
  )
}
