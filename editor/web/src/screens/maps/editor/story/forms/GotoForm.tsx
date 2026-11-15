// "Go to a chapter" (campaign "after" scenes only): which chapter comes next.
import { chapterName } from '../../../../../model/campaignTypes'
import type { Command } from '../../../../../model/storyTypes'
import { useStory } from '../StoryContext'
import { PickSelect } from '../pickers'
import { S } from '../text'
import { useCommandEdit, type CommandRef } from './useCommandEdit'

export function GotoForm({ cmd, at }: { cmd: Command; at: CommandRef }) {
  const { doc } = useStory()
  const { patch } = useCommandEdit(at)
  const chapters = doc.campaign?.chapters ?? []
  return (
    <div className="stack tight">
      <PickSelect label={S.gotoChapter} value={cmd.goto ?? ''} empty={S.gotoNone} options={chapters.map((c) => ({ value: c.id, label: chapterName(c) }))}
        onChange={(v) => patch({ goto: v })} />
      <p className="help">{S.gotoHelp}</p>
    </div>
  )
}
