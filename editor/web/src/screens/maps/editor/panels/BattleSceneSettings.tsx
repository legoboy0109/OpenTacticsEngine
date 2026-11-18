// The map settings' battle scene part (docs/BATTLE_SCENE.md "Which background and platform", docs/ECONOMY.md "Weather and movement cost"):
// the map's own backdrop and platform for each kind of terrain (they win over the game's), and the weather the battle starts with.
import { backgroundList } from '../../../../model/battleArt'
import { TABLE_DEFAULT, mapTable, mapTableProblems, mapWeather, platformList, setMapTableEntry, setMapWeather, type TableKind, type Weather } from '../../../../model/battleTables'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { Field } from '../../../../ui/Field'
import { Select } from '../../../../ui/Select'
import { BS } from './battleSceneText'

function Table({ doc, map, mapId, kind }: { doc: ProjectDoc; map: MapFile; mapId: string; kind: TableKind }) {
  const edit = useEdit()
  const platform = kind === 'battlePlatforms'
  const ids = (platform ? platformList(doc) : backgroundList(doc)).map((x) => x.id)
  const table = mapTable(map, kind)
  const keys = [...(doc.terrain ?? []).map((t) => ({ key: t.id, label: t.name })), { key: TABLE_DEFAULT, label: BS.everythingElse }]
  if (ids.length === 0 && Object.keys(table).length === 0) return <p className="help">{platform ? BS.noPlatforms : BS.noBackgrounds}</p>
  return (
    <Field label={platform ? BS.platformTable : BS.backgroundTable} help={platform ? BS.platformTableHelp : BS.backgroundTableHelp}>
      <table className="kv" data-testid={platform ? 'map-platform-table' : 'map-background-table'}>
        <tbody>
          {keys.map(({ key, label }) => {
            const value = table[key] ?? ''
            const known = value === '' || ids.includes(value)
            return (
              <tr key={key}>
                <th scope="row">{label}</th>
                <td>
                  <Select aria-label={`${platform ? BS.platformFor : BS.backgroundFor} ${label}`} value={value}
                    options={[{ value: '', label: key === TABLE_DEFAULT ? BS.gameTable : BS.fromDefault }, ...(known ? [] : [{ value, label: `${value} (not there)` }]), ...ids.map((id) => ({ value: id, label: id }))]}
                    onChange={(v) => edit((d) => setMapTableEntry(d, mapId, kind, key, v))} />
                </td>
              </tr>
            )
          })}
        </tbody>
      </table>
    </Field>
  )
}

export function BattleSceneSettings({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const problems = mapTableProblems(doc, map)
  return (
    <div className="stack tight" data-testid="battle-scene-settings">
      <h4>{BS.title}</h4>
      <Table doc={doc} map={map} mapId={mapId} kind="battleBackgrounds" />
      <Table doc={doc} map={map} mapId={mapId} kind="battlePlatforms" />
      <Field label={BS.weather} help={BS.weatherHelp}>
        <Select aria-label={BS.weather} value={mapWeather(map)} options={[{ value: '', label: BS.clear }, { value: 'rain', label: BS.rain }, { value: 'snow', label: BS.snow }]}
          onChange={(v) => edit((d) => setMapWeather(d, mapId, v as Weather | ''))} />
      </Field>
      {problems.map((p) => <p key={p} className="error-text" role="alert">{p}</p>)}
    </div>
  )
}
