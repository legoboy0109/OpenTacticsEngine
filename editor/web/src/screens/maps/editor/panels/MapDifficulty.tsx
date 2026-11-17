// A map's own difficulty numbers (docs/DIFFICULTY.md): for each difficulty of the game the map may replace the enemy levels, added stats,
// stats percent, experience and resources. A number set here REPLACES the game's for this map (it does not add); an empty box keeps the
// game's number. "Hard adds 3 enemy levels here" is levels = 3 on Hard.
import { clearMapOverride, difficultyList, mapOverride, setMapOverrideNumber, setMapOverrideStat } from '../../../../model/difficulty'
import { editMap } from '../../../../model/mapCtx'
import type { MapFile, ProjectDoc } from '../../../../model/types'
import { useEdit } from '../../../../store/context'
import { D } from '../../../rules/difficultyText'
import { EnemyFields } from '../../../rules/EnemyFields'

export function MapDifficulty({ doc, map, mapId }: { doc: ProjectDoc; map: MapFile; mapId: string }) {
  const edit = useEdit()
  const list = difficultyList(doc)
  const stats = doc.game?.stats ?? []
  if (list.length === 0 && !map.difficulty) return null  // the game has none (the Rules screen has the card to add them)
  return (
    <details className="fieldset difficulty-map" data-testid="map-difficulty" open={!!map.difficulty}>
      <summary>{D.mapTitle}</summary>
      <div className="stack tight difficulty-body">
        <p className="help">{D.mapHelp}</p>
        {list.map((d) => {
          const o = mapOverride(map, d.id)
          const patch = (fn: (m: MapFile) => MapFile, key: string) => edit((x) => editMap(x, mapId, (m) => fn(m)), `map:${mapId}:difficulty:${d.id}:${key}`)
          return (
            <details key={d.id} open={!!o} data-testid={`map-difficulty-${d.id}`}>
              <summary>{d.name}{o ? ' •' : ''}</summary>
              <div className="stack tight difficulty-body">
                <EnemyFields name={`${d.name} (this map)`} stats={stats} enemy={o?.enemy} xpPercent={o?.xpPercent} resourcePercent={o?.resourcePercent}
                  hint={{
                    levels: d.enemy?.levels !== undefined ? D.mapGames(String(d.enemy.levels)) : D.mapGames('0'),
                    statsPercent: D.mapGames(`${d.enemy?.statsPercent ?? 100}%`),
                    xp: D.mapGames(`${d.xpPercent ?? 100}%`),
                    resource: D.mapGames(`${d.resourcePercent ?? 100}%`),
                    stat: (s) => D.mapGames(String(d.enemy?.stats?.[s] ?? 0)),
                  }}
                  onLevels={(v) => patch((m) => setMapOverrideNumber(m, d.id, 'levels', v), 'levels')}
                  onStatsPercent={(v) => patch((m) => setMapOverrideNumber(m, d.id, 'statsPercent', v), 'pct')}
                  onXp={(v) => patch((m) => setMapOverrideNumber(m, d.id, 'xpPercent', v), 'xp')}
                  onResource={(v) => patch((m) => setMapOverrideNumber(m, d.id, 'resourcePercent', v), 'res')}
                  onStat={(s, v) => patch((m) => setMapOverrideStat(m, d.id, s, v), `stat:${s}`)} />
                {o && <button type="button" className="btn ghost sm" onClick={() => patch((m) => clearMapOverride(m, d.id), 'clear')}>{D.mapClear}</button>}
              </div>
            </details>
          )
        })}
      </div>
    </details>
  )
}
