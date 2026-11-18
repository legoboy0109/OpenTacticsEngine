// The map settings' music part: this map's music, its two turn musics and its preparation screen's, each "Same as the game" unless
// chosen; the music of each side's turn and the track for when few enemies are left (docs/SOUND.md "Music by situation"). Shown only
// when the game has music (or this map already names some, which must stay visible).
import { hasMusic, setMapMusic } from '../../model/audio'
import { MAP_MUSIC_KEYS, MAP_MUSIC_LABEL } from '../../model/audioLabels'
import { FEW_COUNT_MAX, setMapFew, setMapPhaseMusic } from '../../model/audioSituations'
import type { MapFile, ProjectDoc } from '../../model/types'
import { useEdit } from '../../store/context'
import { Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { AudioChoice } from './AudioChoice'
import { T } from './text'

export function MapMusic({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const named = MAP_MUSIC_KEYS.some((k) => map[k] !== undefined) || map.phaseMusic !== undefined || map.fewEnemies !== undefined
  if (!hasMusic(doc) && !named) return null
  const few = map.fewEnemies
  return (
    <div className="stack tight" data-testid="map-music">
      <h4>{T.mapMusicGroup}</h4>
      <p className="help">{T.mapMusicHelp}</p>
      {MAP_MUSIC_KEYS.map((key) => (
        <Field key={key} label={MAP_MUSIC_LABEL[key]}>
          <AudioChoice data-learn={`map-${key}`} kind="music" mode="map" value={map[key]} aria-label={MAP_MUSIC_LABEL[key]}
            onChange={(v) => edit((d) => setMapMusic(d, mapId, key, v))} />
        </Field>
      ))}
      {(map.sides ?? []).length > 0 && (
        <div className="stack tight" data-testid="map-phase-music">
          <h4>{T.mapPhaseGroup}</h4>
          {(map.sides ?? []).map((side) => (
            <Field key={side.id} label={side.name || side.id}>
              <AudioChoice kind="music" mode="map" value={map.phaseMusic?.[side.id]} aria-label={`${side.name || side.id}: ${T.mapPhaseGroup}`}
                onChange={(v) => edit((d) => setMapPhaseMusic(d, mapId, side.id, v))} />
            </Field>
          ))}
        </div>
      )}
      <div className="stack tight" data-testid="map-few-music">
        <h4>{T.mapFewGroup}</h4>
        <p className="help">{T.mapFewHelp}</p>
        <Field label={T.fewTrack}>
          <AudioChoice kind="music" mode="map" value={few?.music} aria-label={`${T.mapFewGroup}: ${T.fewTrack}`}
            onChange={(v) => edit((d) => setMapFew(d, mapId, v === undefined ? undefined : { music: v, count: few?.count ?? doc.audio?.use?.fewEnemies?.count ?? 2 }))} />
        </Field>
        {few?.music !== undefined && few.music !== '' && (
          <Field label={T.fewCount}>
            <NumberInput aria-label={`${T.mapFewGroup}: ${T.fewCount}`} min={1} max={FEW_COUNT_MAX} value={few.count ?? 2} suffix={T.fewCountSuffix}
              onChange={(v) => v !== undefined && edit((d) => setMapFew(d, mapId, { music: few.music, count: v }), 'map-few-count')} />
          </Field>
        )}
      </div>
    </div>
  )
}
