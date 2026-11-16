// The Supports screen (docs/SRPG.md "Editor", step E4): the settings card, then the pairs as "list on the left, form on the
// right". It sits inside the shared frame of the strategy-RPG screens (off / unreadable file / the screen).
import { useMemo, useState } from 'react'
import { deletePairClean, pairUsageLines } from '../../model/srpgRefs'
import { supportChecks } from '../../model/supportChecks'
import { addPair, movePair, nextFreePair, pairIndex, pairKey, pairLabel, pairsOf } from '../../model/supports'
import { Link, navigate, projectPath } from '../../router'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { MasterDetail } from '../../ui/MasterDetail'
import { SrpgScreen } from '../srpg/SrpgScreen'
import { PairForm } from './PairForm'
import { SupportRulesCard } from './SupportRulesCard'
import { T } from './text'
import './supports.css'

export function SupportsScreen({ itemId }: { itemId?: string }) {
  return <SrpgScreen feature="supports"><SupportsBody itemId={itemId} /></SrpgScreen>
}

function SupportsBody({ itemId }: { itemId?: string }) {
  const doc = useDoc()
  const edit = useEdit()
  const store = useStore()
  const pairs = pairsOf(doc)
  const [deleting, setDeleting] = useState<string | null>(null)
  const go = (id?: string) => navigate(projectPath(store.id, 'supports', id))
  const checks = useMemo(() => supportChecks(doc), [doc])
  const free = nextFreePair(doc)
  const note = (doc.characters?.length ?? 0) < 2 ? T.needTwo : free === null ? T.allPaired : undefined
  const target = pairs.find((p) => pairKey(p) === deleting)

  return (
    <>
      {checks.length > 0 && (
        <Banner tone="info" title={T.checksTitle}>
          <ul className="usage-list" data-testid="support-checks">
            {checks.map((c, n) => {
              const p = pairs[pairIndex(doc, c.pair)]
              return <li key={n}>{p && <><Link to={projectPath(store.id, 'supports', c.pair)}>{pairLabel(doc, p)}</Link>: </>}{c.text}</li>
            })}
          </ul>
        </Banner>
      )}
      <SupportRulesCard />
      <MasterDetail heading="h2" title={T.title} noun={T.noun} emptyText={T.empty} selectedId={itemId} newLabel={T.newLabel} note={note}
        items={pairs.map((p) => ({ id: pairKey(p), label: pairLabel(doc, p), sub: T.levelsCount(p.levels?.length ?? 0) }))}
        onSelect={go} onBack={() => go()}
        onNew={() => { const next = nextFreePair(doc); if (next) { edit((d) => addPair(d)); go(pairKey({ a: next[0], b: next[1] })) } }}
        onDelete={setDeleting}
        onMove={(id, delta) => { const i = pairIndex(doc, id); edit((d) => movePair(d, i, i + delta)) }}>
        {(id) => <PairForm pairId={id} />}
      </MasterDetail>
      <DeleteConfirm open={!!target} what={target ? T.deleteWhat(pairLabel(doc, target)) : ''} usages={target ? pairUsageLines(doc, target.a, target.b) : []}
        onCancel={() => setDeleting(null)}
        onConfirm={() => { if (target) { edit((d) => deletePairClean(d, pairIndex(d, pairKey(target)))); go() } setDeleting(null) }} />
    </>
  )
}
