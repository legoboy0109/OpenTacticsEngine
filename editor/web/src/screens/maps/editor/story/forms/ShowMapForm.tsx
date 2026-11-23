// "Show another map" (docs/EVENTS.md "Showing another map"): which map, and where the view starts. And its end, "Back to the battle’s map".
import { sectionsOf } from '../../../../../model/storyShowMap'
import { shownScope } from '../../../../../model/storyShownScope'
import { showMapOptions } from '../../../../../model/storyShowMapEdit'
import { rectOf, type Command } from '../../../../../model/storyTypes'
import { AreaField } from '../AreaField'
import { PickSelect, UnitPick } from '../pickers'
import { StoryScope, useStory } from '../StoryContext'
import { useCommandEdit, type CommandRef } from './useCommandEdit'
import { SM } from './showMapText'

type Camera = 'none' | 'square' | 'unit'
const cameraOf = (c: Command): Camera => (typeof c.focus === 'string' ? 'unit' : c.focus && typeof c.focus === 'object' ? 'square' : 'none')

export function ShowMapForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const story = useStory()
  const { patch } = useCommandEdit(at)
  const options = showMapOptions(story.doc, story.mapId)
  const event = story.map.events?.find((e) => e.id === at.eventId)
  const section = event ? sectionsOf(story.doc, event.do).of.get(cmd) : undefined
  const scope = section ? shownScope(story.doc, section) : null
  const camera = cameraOf(cmd)
  return (
    <div className="stack tight" data-testid="showmap-form">
      <PickSelect label={SM.map} help={SM.mapHelp} value={typeof cmd.showMap === 'string' ? cmd.showMap : ''} options={options} empty={SM.noOtherMap}
        onChange={(v) => patch({ showMap: v, focus: undefined })} />
      {scope && (
        <StoryScope value={{ ...story, ...scope }}>
          <PickSelect label={SM.camera} help={SM.cameraHelp} value={camera}
            options={[{ value: 'none', label: SM.cameraNone }, { value: 'square', label: SM.cameraSquare }, { value: 'unit', label: SM.cameraUnit }]}
            onChange={(v) => patch({ focus: v === 'unit' ? '' : v === 'square' ? { x: 0, y: 0 } : undefined })} />
          {camera === 'unit' && <UnitPick label={SM.unit} value={cmd.focus as string} onChange={(v) => patch({ focus: v })} />}
          {camera === 'square' && <AreaField label={SM.square} mode="square" value={{ ...rectOf(cmd.focus), w: 1, h: 1 }} onChange={(r) => patch({ focus: { x: r.x, y: r.y } })} />}
        </StoryScope>
      )}
      <p className="help">{SM.help}</p>
    </div>
  )
}

export function ReturnMapForm() {
  return <p className="help">{SM.backHelp}</p>
}
