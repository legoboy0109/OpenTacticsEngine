// The command list of one scene, edited with the SAME forms as a map's events. Those forms work on "a map with an event": here the
// map is a made-up scene map (model/campaignRoster.ts), its one event is the scene, and `source.write` puts the result back into
// the document as one undo step per edit. A chapter's Before / After scenes are one kind of source (SceneEditor.tsx); a support
// level's conversation is another (screens/supports), limited to lines of dialogue through `only`.
import { useMemo, useState } from 'react'
import { sceneMap } from '../../model/campaignRoster'
import type { Command, CommandKind } from '../../model/storyTypes'
import { namedUnits } from '../../model/storyIds'
import type { MapFile, ProjectDoc } from '../../model/types'
import { useDoc, useEdit } from '../../store/context'
import { CommandList } from '../maps/editor/story/CommandList'
import { StoryScope, type Story } from '../maps/editor/story/StoryContext'
import { MapViewState } from '../maps/editor/viewState'

export interface SceneSource {
  /** Names the one event of the scene map. */
  eventId: string
  /** The scene's commands in the document. */
  read: (doc: ProjectDoc) => Command[]
  /** The document with `list` as the scene's commands (the same document when nothing changed). */
  write: (doc: ProjectDoc, list: Command[]) => ProjectDoc
  /** "Go to a chapter" is offered in a chapter's scenes, not in a support conversation. */
  goto: boolean
  /** Only these kinds of command are offered. */
  only?: CommandKind[]
  /** The map the story forms see (who can speak, which sides exist); default: the campaign's roster. */
  base?: (doc: ProjectDoc) => MapFile
  /** What the empty speaker choice is called (default "Nobody (a narrator)"): in a character's quotes a line with no speaker is that character's. */
  emptySpeaker?: string
}

export function SceneCommands({ source, name }: { source: SceneSource; name: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const [selected, setSelected] = useState<string | null>(null)
  // Never used by scene commands (no squares to pick); the story forms just expect the field.
  const vs = useMemo(() => new MapViewState(), [])
  const story = useMemo<Story>(() => {
    const asMap = (d: ProjectDoc): MapFile => ({ ...(source.base ? source.base(d) : sceneMap(d)), events: [{ id: source.eventId, when: { battle: 'start' }, do: source.read(d) }] })
    const map = asMap(doc)
    return {
      doc, map, mapId: '', vs, units: namedUnits(doc, map), teams: [], tags: [],
      scene: { goto: source.goto, only: source.only, emptySpeaker: source.emptySpeaker },
      apply: (fn, key) => edit((d) => {
        const m = asMap(d)
        const next = fn(m)
        return next === m ? d : source.write(d, next.events?.[0]?.do ?? [])
      }, key),
      applyDoc: (fn, key) => edit(fn, key),
    }
  }, [doc, source, edit, vs])
  return (
    <StoryScope value={story}>
      <CommandList eventId={source.eventId} path={[]} commands={story.map.events![0].do} talk={false} selected={selected} onSelect={setSelected} name={name} />
    </StoryScope>
  )
}
