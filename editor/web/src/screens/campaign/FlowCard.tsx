// A small picture of the story's flow: the chapters in order, an arrow to where each one leads, and a mark on the chapters
// nothing leads to. Plain text rows (works at any width); each name opens that chapter.
import { flowRows } from '../../model/campaignFlow'
import { navigate, projectPath } from '../../router'
import { useDoc, useStore } from '../../store/context'
import { Card } from '../../ui/Card'
import { C } from './text'

export function FlowCard() {
  const doc = useDoc()
  const store = useStore()
  const rows = flowRows(doc.campaign!)
  return (
    <Card title={C.flowTitle} help={C.flowHelp}>
      {rows.length === 0 && <p className="help">{C.flowEmpty}</p>}
      <ol className="flow-list" data-testid="flow">
        {rows.map((r) => (
          <li key={r.id} className="flow-row" data-testid={`flow-${r.id}`} data-unreached={r.unreached || undefined}>
            <button type="button" className="flow-chapter" aria-label={C.flowOpen(r.name)} onClick={() => navigate(projectPath(store.id, 'campaign', r.id))}>
              <span className="flow-number">{r.number}</span>{r.name}
            </button>
            {r.unreached && <span className="flow-flag">{C.flowNothing}</span>}
            <span className="flow-arrows">
              {r.leadsTo.filter((t) => t.kind === 'next').map((t) => <span key={t.id} className="flow-arrow">→ {t.name}</span>)}
              {r.leadsTo.filter((t) => t.kind === 'goto').map((t) => <span key={t.id} className="flow-arrow flow-goto">⇢ {t.name}</span>)}
              {r.ends && r.leadsTo.length === 0 && <span className="flow-arrow flow-end">→ {C.flowEnds}</span>}
            </span>
          </li>
        ))}
      </ol>
    </Card>
  )
}
