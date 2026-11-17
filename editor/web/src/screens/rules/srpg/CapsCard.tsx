// Rules card "Stat caps" (docs/SRPG.md section 7): one optional number per stat of the game; empty = no cap. Shown while the feature is on.
import { useMemo } from 'react'
import { computerCap, computerCapProblems, setComputerCap } from '../../../model/fidelityCaps'
import { capChecks } from '../../../model/rankChecks'
import { MAX_CAP, gameCap, setGameCap } from '../../../model/ranks'
import { useDoc, useEdit } from '../../../store/context'
import { Banner } from '../../../ui/Banner'
import { Card } from '../../../ui/Card'
import { NumberInput } from '../../../ui/NumberInput'
import { R } from '../../ranks/text'
import '../../ranks/ranks.css'

export function CapsCard() {
  const doc = useDoc()
  const edit = useEdit()
  const checks = useMemo(() => capChecks(doc).filter((c) => c.section === 'rules'), [doc])
  if (doc.caps === null) return null
  const stats = doc.game?.stats ?? []
  return (
    <Card title={R.capsTitle} help={R.capsIntro} id="stat-caps" learn="rules-caps">
      <div className="stack" data-testid="caps-card">
        {stats.length === 0 && <p className="help">{R.capsNoStats}</p>}
        {stats.length > 0 && (
          <table className="kv three-col cap-rows">
            <thead><tr><th scope="col"><span className="sr-only">Stat</span></th><th scope="col">{R.capCol}</th></tr></thead>
            <tbody>
              {stats.map((s) => (
                <tr key={s.id}>
                  <th scope="row">{s.name}</th>
                  <td>
                    <NumberInput value={gameCap(doc, s.id)} allowEmpty min={1} max={MAX_CAP} placeholder={R.capNone} aria-label={R.capLabel(s.name)}
                      onChange={(v) => edit((d) => setGameCap(d, s.id, v), `cap:${s.id}`)} />
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
        {stats.length > 0 && (
          <div data-testid="computer-caps">
            <h4>{R.computerCapsTitle}</h4>
            <p className="help">{R.computerCapsHelp}</p>
            <table className="kv three-col cap-rows">
              <tbody>
                {stats.map((s) => (
                  <tr key={s.id}>
                    <th scope="row">{s.name}</th>
                    <td>
                      <NumberInput value={computerCap(doc, s.id)} allowEmpty min={1} max={MAX_CAP} placeholder={R.computerCapSame} aria-label={R.computerCapLabel(s.name)}
                        onChange={(v) => edit((d) => setComputerCap(d, s.id, v), `ccap:${s.id}`)} />
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
            {computerCapProblems(doc).map((p) => <Banner key={p} tone="warn" title={p} />)}
          </div>
        )}
        {checks.length > 0 && <Banner tone="warn" title={R.worthALook}><ul className="usage-list">{checks.map((c, i) => <li key={i}>{c.text}</li>)}</ul></Banner>}
      </div>
    </Card>
  )
}
