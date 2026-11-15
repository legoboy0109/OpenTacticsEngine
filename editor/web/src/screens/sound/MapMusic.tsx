// The map settings' music part: this map's music and its two turn musics, each "Same as the game" unless chosen. Shown only
// when the game has music (or this map already names some, which must stay visible).
import { hasMusic, setMapMusic } from '../../model/audio'
import { MAP_MUSIC_KEYS, MAP_MUSIC_LABEL } from '../../model/audioLabels'
import type { MapFile, ProjectDoc } from '../../model/types'
import { useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { AudioChoice } from './AudioChoice'
import { T } from './text'

export function MapMusic({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  if (!hasMusic(doc) && !MAP_MUSIC_KEYS.some((k) => map[k] !== undefined)) return null
  return (
    <div className="stack tight" data-testid="map-music">
      <h4>{T.mapMusicGroup}</h4>
      <p className="help">{T.mapMusicHelp}</p>
      {MAP_MUSIC_KEYS.map((key) => (
        <Field key={key} label={MAP_MUSIC_LABEL[key]}>
          <AudioChoice kind="music" mode="map" value={map[key]} aria-label={MAP_MUSIC_LABEL[key]}
            onChange={(v) => edit((d) => setMapMusic(d, mapId, key, v))} />
        </Field>
      ))}
    </div>
  )
}
