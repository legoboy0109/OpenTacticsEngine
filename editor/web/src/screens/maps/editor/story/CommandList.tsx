// The list of commands ("Then"): one plain sentence each; selecting one opens its form under it; add, reorder, delete.
// An "If" shows its own "Then" and "Otherwise" lists indented under it, so nesting reads as an outline.
import { newCommand } from '../../../../model/storyDefaults'
import { addCommand, moveCommand, removeCommand, type Branch, type ListPath } from '../../../../model/storyEdit'
import { commandText } from '../../../../model/storySummary'
import { commandKind, type Command, type CommandKind } from '../../../../model/storyTypes'
import { Select, type OptionGroup } from '../../../../ui/Select'
import { CommandForm } from './CommandForm'
import { RowButtons } from '../../../../ui/RowButtons'
import { useStory, type Story } from './StoryContext'
import { S } from './text'

/** Which row is open: the path to its list plus its index. */
export const cmdKey = (path: ListPath, i: number): string => `${path.map(([n, b]) => `${n}${b[0]}`).join('/')}#${i}`

interface Props {
  eventId: string
  path: ListPath
  commands: Command[]
  /** Only a "Two units talk" event can end the talking unit's turn. */
  talk: boolean
  selected: string | null
  onSelect: (key: string | null) => void
  /** Names the list for assistive technology and tests ("Then", "If 2: Then"). */
  name: string
}

const GROUPS: Array<{ group: string; kinds: CommandKind[] }> = [
  { group: S.commandGroups.dialogue, kinds: ['say'] },
  { group: S.commandGroups.units, kinds: ['addUnit', 'removeUnit', 'moveUnit', 'changeSide', 'promote', 'teachSkill', 'forgetSkill'] },
  { group: S.commandGroups.give, kinds: ['giveItem', 'giveResource', 'grantResearch'] },
  { group: S.commandGroups.story, kinds: ['setSwitch', 'setVariable', 'addVariable', 'objective', 'win', 'lose'] },
  { group: S.commandGroups.flow, kinds: ['if', 'endTurn', 'focus', 'goto'] },
  { group: S.commandGroups.sound, kinds: ['playMusic', 'stopMusic', 'playSound'] },
]

/** A campaign scene knows no map, so it can only use these (`goto` only in an "after" scene). */
const SCENE_KINDS: CommandKind[] = ['say', 'giveResource', 'setSwitch', 'setVariable', 'addVariable', 'if', 'goto', 'playMusic', 'stopMusic', 'playSound']

/** The menu of commands, grouped. A kind that cannot be used yet says why in its name and cannot be chosen. */
function menu(story: Story, talk: boolean): OptionGroup[] {
  const { doc } = story
  const why: Partial<Record<CommandKind, string | null>> = {
    promote: (doc.units?.length ?? 0) > 1 ? null : 'needs two unit types',
    teachSkill: doc.skills?.length ? null : S.noSkills.toLowerCase(),
    forgetSkill: doc.skills?.length ? null : S.noSkills.toLowerCase(),
    giveItem: doc.items?.length ? null : 'no items yet',
    giveResource: doc.game?.resources?.length ? null : 'no resources yet',
    grantResearch: doc.research?.length ? null : 'no technologies yet',
    setSwitch: doc.game?.switches?.length ? null : 'no switches yet',
    setVariable: doc.game?.variables?.length ? null : 'no variables yet',
    addVariable: doc.game?.variables?.length ? null : 'no variables yet',
    endTurn: talk ? null : S.endTurnTalkOnly,
    goto: doc.campaign && doc.campaign.chapters.length > 0 ? null : 'no chapters yet',
    playMusic: doc.audio?.music && Object.keys(doc.audio.music).length ? null : 'no music yet',
    playSound: doc.audio?.sounds && Object.keys(doc.audio.sounds).length ? null : 'no sounds yet',
  }
  const allowed = (k: CommandKind) => (story.scene ? SCENE_KINDS.includes(k) && (k !== 'goto' || story.scene.goto) : k !== 'goto')
  return GROUPS.map((g) => ({
    group: g.group,
    options: g.kinds.filter(allowed).map((k) => ({ value: k, label: why[k] ? `${S.commandKinds[k]} (${why[k]})` : S.commandKinds[k], disabled: !!why[k] })),
  })).filter((g) => g.options.length > 0)
}

export function CommandList({ eventId, path, commands, talk, selected, onSelect, name }: Props) {
  const story = useStory()
  const { apply } = story
  const add = (kind: string) => {
    apply((m) => addCommand(m, eventId, path, newCommand(story, kind as CommandKind)))
    onSelect(cmdKey(path, commands.length))
  }
  const move = (from: number, to: number) => {
    apply((m) => moveCommand(m, eventId, path, from, to))
    if (selected === cmdKey(path, from)) onSelect(cmdKey(path, to))
    else if (selected === cmdKey(path, to)) onSelect(cmdKey(path, from))
  }
  const remove = (i: number) => {
    apply((m) => removeCommand(m, eventId, path, i))
    onSelect(null)
  }
  return (
    <div className="story-list commands" role="group" aria-label={name} data-testid={`commands-${name}`}>
      {commands.length === 0 && <p className="help">{S.noCommands}</p>}
      <ol className="command-list">
        {commands.map((c, i) => {
          const key = cmdKey(path, i)
          const open = selected === key
          const kind = commandKind(c)
          const label = S.commandN(i + 1)
          return (
            <li key={i} className="command-row" data-open={open || undefined} data-kind={kind ?? 'unknown'}>
              <div className="story-row-head">
                <button type="button" className="command-sentence" aria-expanded={open} data-testid={`command-${kind}`} onClick={() => onSelect(open ? null : key)}>
                  {S.openCommand(i + 1, commandText(story, c))}
                </button>
                <RowButtons index={i} count={commands.length} up={S.moveCommandUp(`${name} ${label}`)} down={S.moveCommandDown(`${name} ${label}`)} del={S.deleteCommand(`${name} ${label}`)}
                  onUp={() => move(i, i - 1)} onDown={() => move(i, i + 1)} onDelete={() => remove(i)} />
              </div>
              {open && <div className="command-form"><CommandForm cmd={c} at={{ eventId, path, index: i }} /></div>}
              {kind === 'if' && (
                <div className="command-nested">
                  {(['then', 'else'] as Branch[]).map((b) => (
                    <div key={b} className="nested-part">
                      <h4>{b === 'then' ? S.thenPart : S.elsePart}</h4>
                      <CommandList eventId={eventId} path={[...path, [i, b]]} commands={c[b] ?? []} talk={talk} selected={selected} onSelect={onSelect}
                        name={`${name === S.then ? '' : `${name}, `}If ${i + 1}: ${b === 'then' ? S.thenPart : 'Otherwise'}`} />
                    </div>
                  ))}
                </div>
              )}
            </li>
          )
        })}
      </ol>
      <Select data-learn="add-command" aria-label={`${name}: ${S.addCommand}`} value="" placeholder={S.addCommand} options={menu(story, talk)} onChange={add} />
    </div>
  )
}
