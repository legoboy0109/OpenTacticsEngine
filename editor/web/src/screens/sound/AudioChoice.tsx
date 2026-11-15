// The pickers for "which track / which sound": a drop-down of what the game has, with the plain choices that go with where
// it is used. The stored value of "no choice" is a missing key; "" (silence on purpose) is its own choice where the game
// accepts it. A name that no longer exists stays visible as "(not there any more)" so it can be seen and changed.
import { musicIds, soundIds } from '../../model/audio'
import { useDoc } from '../../store/context'
import { Select, type Option } from '../../ui/Select'
import { T } from './text'

/** Where the choice is made, which sets the wording of the plain choices:
 *  nothing: the game's standard set (“Nothing”); game: weapons, units, items (“Same as the game” / “Silent”);
 *  map: a map's music (“Same as the game” / “No music”); scene: a scene's music (“Keep what is playing”). */
export type ChoiceMode = 'nothing' | 'game' | 'map' | 'scene'

const NO_CHOICE = '__none__'

interface Props {
  kind: 'music' | 'sound'
  mode: ChoiceMode
  /** undefined = no choice (key missing), "" = silent / no music, otherwise a name. */
  value: string | undefined
  onChange: (v: string | undefined) => void
  'aria-label'?: string
  'data-learn'?: string
}

export function AudioChoice({ kind, mode, value, onChange, ...rest }: Props) {
  const doc = useDoc()
  const ids = kind === 'music' ? musicIds(doc) : soundIds(doc)
  const none = mode === 'nothing' ? T.nothing : mode === 'scene' ? T.keepPlaying : T.sameAsGame
  const silent = mode === 'game' ? T.silent : mode === 'map' ? T.noMusicOption : null
  // "Nothing" stands for both a missing key and "" in the standard set.
  const shown = value === undefined || (value === '' && silent === null) ? NO_CHOICE : value
  const options: Option[] = [
    { value: NO_CHOICE, label: none },
    ...(silent !== null ? [{ value: '', label: silent }] : []),
    ...(shown !== NO_CHOICE && shown !== '' && !ids.includes(shown) ? [{ value: shown, label: T.missing(shown) }] : []),
    ...ids.map((id) => ({ value: id, label: id })),
  ]
  return <Select value={shown} options={options} onChange={(v) => onChange(v === NO_CHOICE ? undefined : v)} {...rest} />
}
