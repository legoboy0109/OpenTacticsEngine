// What a terrain costs while it rains or snows (docs/ECONOMY.md "Weather and movement cost"), and its platform in the battle scene.
import { WEATHERS, WEATHER_COST_MAX, setTerrainPlatform, setWeatherCost, terrainPlatform, weatherCostOf, weatherCostProblems, platformList } from '../../model/battleTables'
import { useDoc, useEdit } from '../../store/context'
import { Card } from '../../ui/Card'
import { FieldGroup, Field } from '../../ui/Field'
import { NumberInput } from '../../ui/NumberInput'
import { Select } from '../../ui/Select'
import { Check } from '../../ui/Toggle'
import { WX } from './weatherText'

export function WeatherCosts({ id }: { id: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const problems = weatherCostProblems(doc, id)
  return (
    <Card title={WX.title} help={WX.help}>
      <FieldGroup label={WX.costs}>
        <table className="kv" data-testid="weather-costs">
          <thead><tr><th scope="col"><span className="sr-only">{WX.moveType}</span></th>{WEATHERS.map((w) => <th key={w} scope="col">{WX.weather[w]}</th>)}</tr></thead>
          <tbody>
            {doc.game!.moveTypes.map((m) => (
              <tr key={m.id}>
                <th scope="row">{m.name}</th>
                {WEATHERS.map((w) => (
                  <td key={w}>
                    {weatherCostOf(doc, id, w, m.id) !== 'blocked' && (
                      <NumberInput value={weatherCostOf(doc, id, w, m.id) as number | undefined} min={1} max={WEATHER_COST_MAX} allowEmpty placeholder={WX.same} aria-label={`${m.name}, ${WX.weather[w].toLowerCase()}`}
                        onChange={(v) => edit((d) => setWeatherCost(d, id, w, m.id, v), `terrain:${id}:weather:${w}:${m.id}`)} />
                    )}
                    <Check label={WX.closed} checked={weatherCostOf(doc, id, w, m.id) === 'blocked'} onChange={(on) => edit((d) => setWeatherCost(d, id, w, m.id, on ? 'blocked' : undefined))} />
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </FieldGroup>
      {platformList(doc).length > 0 && (
        <Field label={WX.platform} help={WX.platformHelp}>
          <Select aria-label={WX.platform} value={terrainPlatform(doc, id)} options={[{ value: '', label: WX.platformDefault }, ...platformList(doc).map((p) => ({ value: p.id, label: p.id }))]}
            onChange={(v) => edit((d) => setTerrainPlatform(d, id, v))} />
        </Field>
      )}
      {problems.map((p) => <p key={p} className="error-text" role="alert">{p}</p>)}
    </Card>
  )
}
