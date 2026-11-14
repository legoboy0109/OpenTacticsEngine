// The one-click offer to bring a project made before structures existed up to date (model/structuresConvert.ts).
// It explains in plain words what will change before anything does; converting is one undo step.
import { useMemo, useState } from 'react'
import { convertOldProject, convertPlan, type ConvertPlan } from '../model/structuresConvert'
import { useProject, useStore } from '../store/context'
import { Banner } from '../ui/Banner'
import { Dialog } from '../ui/Dialog'
import { C } from './convertText'

export function ConvertOffer() {
  const store = useStore()
  const { doc, problems } = useProject()
  const [open, setOpen] = useState(false)
  const plan = useMemo(() => convertPlan(doc), [doc])
  // A damaged structures file loads as "none": converting would overwrite it, so the Structures screen asks for a fix first.
  if (!plan || (doc.structures === null && problems.some((p) => p.file === 'data/structures.json'))) return null
  const convert = () => {
    store.edit(convertOldProject)
    store.setNotice(C.done)
    setOpen(false)
  }
  return (
    <>
      <Banner tone="warn" title={C.bannerTitle}
        actions={<button type="button" className="btn primary sm" onClick={() => setOpen(true)}>{C.review}</button>}>
        <p>{C.bannerBody}</p>
      </Banner>
      {open && (
        <Dialog open title={C.dialogTitle} onClose={() => setOpen(false)} wide
          footer={(
            <>
              <button type="button" className="btn" onClick={() => setOpen(false)}>{C.cancel}</button>
              <button type="button" className="btn primary" onClick={convert}>{C.convert}</button>
            </>
          )}>
          <Explanation plan={plan} />
        </Dialog>
      )}
    </>
  )
}

function Explanation({ plan }: { plan: ConvertPlan }) {
  return (
    <div className="stack tight" data-testid="convert-explanation">
      <p>{C.intro}</p>
      {plan.kinds.length > 0 && (
        <>
          <h3>{C.kindsTitle}</h3>
          <ul className="usage-list">
            {plan.kinds.map((k) => <li key={k.terrainId}>{C.kind(k.name, k.settings, k.squares)}</li>)}
          </ul>
          <p>{C.groundStays}</p>
        </>
      )}
      {(plan.ownersKept > 0 || plan.ownersDropped > 0) && (
        <>
          <h3>{C.ownersTitle}</h3>
          <ul className="usage-list">
            {plan.ownersKept > 0 && <li>{C.ownersKept(plan.ownersKept)}</li>}
            {plan.ownersDropped > 0 && <li>{C.ownersDropped(plan.ownersDropped)}</li>}
          </ul>
        </>
      )}
      {plan.kinds.length > 0 && <p>{C.looks}</p>}
      <p><strong>{C.safe}</strong></p>
    </div>
  )
}
