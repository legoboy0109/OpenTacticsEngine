// The list of commands ("Then"): one plain sentence each; selecting one opens its form under it; add, reorder, delete.
// An "If" shows its own "Then" and "Otherwise" lists indented under it, so nesting reads as an outline.
import { backgroundIds } from '../../../../model/backgrounds'
import { groupNames } from '../../../../model/behaviour'
import { placesForCommand } from '../../../../model/placeStory'
import { newCommand } from '../../../../model/storyDefaults'
import { addCommand, answerOf, moveCommand, removeCommand, type ListPath } from '../../../../model/storyEdit'
import { commandText } from '../../../../model/storySummary'
import { commandKind, type Command, type CommandKind } from '../../../../model/storyTypes'
import { Select, type OptionGroup } from '../../../../ui/Select'
import { CommandForm } from './CommandForm'
import { P } from './forms/placeText'
import { SM } from './forms/showMapText'
import { showMapOptions } from '../../../../model/storyShowMapEdit'
import { MC } from './forms/mapChangeText'
import { EX } from './forms/extrasText'
import { PT } from './forms/partyText'
import { storageMode } from '../../../../model/itemRules'
import { changeIds } from '../../../../model/mapChanges'
import { RowButtons } from '../../../../ui/RowButtons'
import { BATTLE_READERS, sectionsOf, type Section } from '../../../../model/storyShowMap'
import { shownScope } from '../../../../model/storyShownScope'
import { StoryScope, useStory, type Story } from './StoryContext'
import { S } from './text'

/** Which row is open: the path to its list plus its index. */
export const cmdKey = (path: ListPath, i: number): string => `${path.map(([n, b]) => `${n}${b === 'then' ? 't' : b === 'else' ? 'e' : `a${answerOf(b)}`}`).join('/')}#${i}`

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
  /** Set on a nested list (an If's branch, an answer) that sits between a "Show another map" and its "Back": its commands act on that map. */
  shown?: Section
}

const GROUPS: Array<{ group: string; kinds: CommandKind[] }> = [
  { group: S.commandGroups.dialogue, kinds: ['say', 'choice'] },
  { group: S.commandGroups.units, kinds: ['addUnit', 'removeUnit', 'moveUnit', 'changeSide', 'guestJoins', 'promote', 'teachSkill', 'forgetSkill', 'addCondition', 'removeCondition', 'addSupport', 'setBehaviour', 'wakeGroup', 'showUnit', 'fight'] },
  { group: S.commandGroups.give, kinds: ['giveItem', 'takeItem', 'giveResource', 'grantResearch'] },
  { group: S.commandGroups.story, kinds: ['setSwitch', 'setVariable', 'addVariable', 'objective', 'win', 'lose'] },
  { group: S.commandGroups.flow, kinds: ['if', 'endTurn', 'focus', 'goto'] },
  { group: S.commandGroups.sound, kinds: ['playMusic', 'stopMusic', 'playSound', 'setPhaseMusic'] },
  { group: S.commandGroups.scene, kinds: ['background', 'fade', 'wait', 'shake', 'enter', 'move', 'leave', 'clearStage', 'weather', 'narrate', 'showMap', 'returnMap'] },
  { group: S.commandGroups.places, kinds: ['openDoor', 'closeDoor', 'lockDoor', 'unlockDoor', 'openChest'] },
  { group: MC.group, kinds: ['setTerrain', 'applyChange', 'revertChange', 'setFog', 'setVision'] },
  { group: EX.group, kinds: ['storeItems'] },
  { group: PT.group, kinds: ['switchParty', 'mergeParties'] },
]

/** A campaign scene knows no map, so it can only use these (`goto` in a chapter's scenes, not in a support conversation). */
const SCENE_KINDS: CommandKind[] = ['say', 'choice', 'addSupport', 'takeItem', 'giveResource', 'setSwitch', 'setVariable', 'addVariable', 'if', 'goto', 'playMusic', 'stopMusic', 'playSound',
  'background', 'fade', 'wait', 'shake', 'enter', 'move', 'leave', 'clearStage', 'narrate', 'storeItems', 'switchParty', 'mergeParties']

/** A door / chest command needs such a place with a story name on this map. */
const placeWhy = (story: Story, kind: CommandKind): string | null => (placesForCommand(story.doc, story.map, kind).length > 0 ? null : P.noneYetShort)

/** The menu of commands, grouped. A kind that cannot be used yet says why in its name and cannot be chosen. */
/** `at`: whether the list is the event's own (showMap / returnMap only go there) and whether a shown map is still open at its end. */
function menu(story: Story, talk: boolean, at: { top: boolean; open: boolean }): OptionGroup[] {
  const { doc } = story
  const why: Partial<Record<CommandKind, string | null>> = {
    promote: (doc.units?.length ?? 0) > 1 ? null : 'needs two unit types',
    addSupport: doc.supports?.pairs?.length ? null : 'no support pairs yet',
    teachSkill: doc.skills?.length ? null : S.noSkills.toLowerCase(),
    forgetSkill: doc.skills?.length ? null : S.noSkills.toLowerCase(),
    addCondition: doc.conditions?.length ? null : S.noConditions2.toLowerCase(),
    removeCondition: doc.conditions?.length ? null : S.noConditions2.toLowerCase(),
    wakeGroup: groupNames(story.map).length ? null : 'no groups yet',
    giveItem: doc.items?.length ? null : 'no items yet',
    takeItem: doc.items?.length ? null : 'no items yet',
    giveResource: doc.game?.resources?.length ? null : 'no resources yet',
    grantResearch: doc.research?.length ? null : 'no technologies yet',
    setSwitch: doc.game?.switches?.length ? null : 'no switches yet',
    setVariable: doc.game?.variables?.length ? null : 'no variables yet',
    addVariable: doc.game?.variables?.length ? null : 'no variables yet',
    endTurn: talk ? null : S.endTurnTalkOnly,
    goto: doc.campaign && doc.campaign.chapters.length > 0 ? null : 'no chapters yet',
    playMusic: doc.audio?.music && Object.keys(doc.audio.music).length ? null : 'no music yet',
    playSound: doc.audio?.sounds && Object.keys(doc.audio.sounds).length ? null : 'no sounds yet',
    background: backgroundIds(doc).length ? null : 'no backgrounds yet',
    openDoor: placeWhy(story, 'openDoor'), closeDoor: placeWhy(story, 'closeDoor'), lockDoor: placeWhy(story, 'lockDoor'),
    unlockDoor: placeWhy(story, 'unlockDoor'), openChest: placeWhy(story, 'openChest'),
    switchParty: doc.campaign?.parties?.length ? null : PT.noParties,
    mergeParties: (doc.campaign?.parties?.length ?? 0) > 1 ? null : PT.noParties,
    storeItems: storageMode(doc) === 'off' ? EX.noStorage : doc.items?.length ? null : 'no items yet',
    fight: story.units.length >= 2 ? null : 'needs two named units',
    showMap: !at.top ? SM.menuTopOnly : at.open ? SM.menuAlready : showMapOptions(doc, story.mapId).length ? null : SM.menuNoOther,
    returnMap: !at.top ? SM.menuTopOnly : at.open ? null : SM.menuNothing,
    applyChange: changeIds(story.map).length ? null : MC.noChangesYet, revertChange: changeIds(story.map).length ? null : MC.noChangesYet,
  }
  // A support conversation (`scene.only`) takes the listed kinds and nothing else.
  const allowed = (k: CommandKind) => (story.scene?.only ? story.scene.only.includes(k) : story.scene ? SCENE_KINDS.includes(k) && (k !== 'goto' || story.scene.goto) : k !== 'goto' && k !== 'switchParty' && k !== 'mergeParties')
  return GROUPS.map((g) => ({
    group: g.group,
    options: g.kinds.filter(allowed).map((k) => ({ value: k, label: why[k] ? `${S.commandKinds[k]} (${why[k]})` : S.commandKinds[k], disabled: !!why[k] })),
  })).filter((g) => g.options.length > 0)
}

/** The story the commands of a section see: the shown map's units, sides, size and changes instead of the battle's. */
function scoped(story: Story, section: Section | undefined): Story {
  const scope = section ? shownScope(story.doc, section) : null
  return scope ? { ...story, ...scope } : story
}

export function CommandList({ eventId, path, commands, talk, selected, onSelect, name, shown }: Props) {
  const story = useStory()
  const { apply } = story
  // The event's own list knows its sections; a nested list inherits the one it sits in.
  const sections = path.length === 0 ? sectionsOf(story.doc, commands) : null
  const sectionOf = (c: Command): Section | undefined => shown ?? sections?.of.get(c)
  const openAtEnd = shown ?? sections?.list.find((x) => !x.back)  // a new command at the end of an open section acts on the shown map
  const add = (kind: string) => {
    apply((m) => addCommand(m, eventId, path, newCommand(scoped(story, openAtEnd), kind as CommandKind)))
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
          const section = sectionOf(c)
          // The "Show another map" command itself is still a command of the battle's map (it picks the shown map); the ones after it act on that map.
          const acting = section && section.show !== c && !(kind && BATTLE_READERS.includes(kind)) ? scoped(story, section) : story  // an If's conditions, a variable and setPhaseMusic read the battle's own map
          return (
            <li key={i} className="command-row" data-open={open || undefined} data-kind={kind ?? 'unknown'}>
              <div className="story-row-head">
                <button type="button" className="command-sentence" aria-expanded={open} data-testid={`command-${kind}`} onClick={() => onSelect(open ? null : key)}>
                  {S.openCommand(i + 1, commandText(acting, c))}
                </button>
                <RowButtons index={i} count={commands.length} up={S.moveCommandUp(`${name} ${label}`)} down={S.moveCommandDown(`${name} ${label}`)} del={S.deleteCommand(`${name} ${label}`)}
                  onUp={() => move(i, i - 1)} onDown={() => move(i, i + 1)} onDelete={() => remove(i)} />
              </div>
              {open && <div className="command-form"><StoryScope value={acting}><CommandForm cmd={c} at={{ eventId, path, index: i }} /></StoryScope></div>}
              {kind === 'choice' && (
                <div className="command-nested">
                  {(c.answers ?? []).map((a, k) => (
                    <div key={k} className="nested-part" data-testid={`answer-${k + 1}`}>
                      <h4>{S.answerPart(k + 1, a.text ?? '')}</h4>
                      <CommandList eventId={eventId} path={[...path, [i, `answer${k}`]]} commands={a.do ?? []} talk={talk} selected={selected} onSelect={onSelect} shown={section && section.show !== c ? section : undefined}
                        name={`${name === S.then ? '' : `${name}, `}Question ${i + 1}: Answer ${k + 1}`} />
                    </div>
                  ))}
                </div>
              )}
              {kind === 'if' && (
                <div className="command-nested">
                  {(['then', 'else'] as const).map((b) => (
                    <div key={b} className="nested-part">
                      <h4>{b === 'then' ? S.thenPart : S.elsePart}</h4>
                      <CommandList eventId={eventId} path={[...path, [i, b]]} commands={c[b] ?? []} talk={talk} selected={selected} onSelect={onSelect} shown={section && section.show !== c ? section : undefined}
                        name={`${name === S.then ? '' : `${name}, `}If ${i + 1}: ${b === 'then' ? S.thenPart : 'Otherwise'}`} />
                    </div>
                  ))}
                </div>
              )}
            </li>
          )
        })}
      </ol>
      <Select data-learn="add-command" aria-label={`${name}: ${S.addCommand}`} value="" placeholder={S.addCommand} options={menu(story, talk, { top: path.length === 0, open: !!openAtEnd && path.length === 0 })} onChange={add} />
    </div>
  )
}
