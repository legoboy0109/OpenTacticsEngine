// The "Standing part" section of a terrain's Picture card, for a style whose kind of view is 3D (docs/GRAPHICS.md "Standing
// terrain"): nothing, an upright picture (the usual piece picker, with a random mix for variants) or 3D models (the model files
// of the game, or an upload). Edits go through model/stand.ts. The map canvas stays top-down; Play shows the real thing.
import { useRef, useState, useSyncExternalStore } from 'react'
import { sheetsOf } from '../../model/graphics'
import type { Slot } from '../../model/graphicsLooks'
import { MAX_MODEL_BYTES, modelNameFor, readGlbFacts } from '../../model/models3d'
import {
  addStandModel, removeStandModel, setStand, setStandHeight, setStandPicture, setStandScatter, setStandTurn, setStandWidth, standFields,
  standKind, standModels, standPictureLook, STAND_HEIGHT, STAND_TURN, STAND_WIDTH, type StandKind,
} from '../../model/stand'
import type { Look } from '../../model/types'
import { useDoc, useEdit, useStore } from '../../store/context'
import { Banner } from '../Banner'
import { Field } from '../Field'
import { NumberInput } from '../NumberInput'
import { Select, type Option } from '../Select'
import { Check } from '../Toggle'
import { ArtCanvas } from './ArtCanvas'
import { M } from './modelText'
import { PickerDialog } from './PickerDialog'
import { S } from './standText'
import { useStandSample, type LookChoice } from './usePreview'

interface Props { slot: Slot; entry: Look | null | undefined; id: string; name: string; choice: LookChoice }

export function StandEditor({ slot, entry, id, name, choice }: Props) {
  const doc = useDoc()
  const edit = useEdit()
  const stand = entry ? entry.stand : undefined
  const kind = standKind(stand)
  // "A picture" / "A 3D model" chosen but nothing picked yet: nothing is written until a picture or a file is chosen.
  const [pending, setPending] = useState<{ key: string; kind: StandKind } | null>(null)
  const key = `${slot.style ?? ''}:${id}`
  const shownKind: StandKind = kind !== 'none' ? kind : pending?.key === key ? pending.kind : 'none'
  const [picking, setPicking] = useState(false)
  const f = standFields(stand)

  if (!entry) {
    return (
      <section className="stand-part stack tight" aria-label={S.title}>
        <h3>{S.title}</h3>
        <p className="help">{S.help}</p>
        <p className="help">{S.needGround}</p>
      </section>
    )
  }
  const choose = (k: StandKind) => {
    if (k === shownKind) return
    if (k === 'none') { setPending(null); edit((d) => setStand(d, slot, undefined)); return }
    // Changing between a picture and models drops what the other one had; nothing is written until something is chosen.
    setPending({ key, kind: k })
    if (kind !== 'none') edit((d) => setStand(d, slot, undefined))
  }
  const hasImages = sheetsOf(doc).length > 0

  return (
    <section className="stand-part stack tight" aria-label={S.title} data-testid="stand-part">
      <h3>{S.title}</h3>
      <p className="help">{S.help}</p>
      <fieldset className="fieldset">
        <legend>{S.group}</legend>
        <div className="kind-row" role="radiogroup" aria-label={S.group}>
          <Radio on={shownKind === 'none'} label={S.none} onPick={() => choose('none')} />
          <Radio on={shownKind === 'picture'} label={S.picture} onPick={() => choose('picture')} />
          <Radio on={shownKind === 'model'} label={S.model} onPick={() => choose('model')} />
        </div>
      </fieldset>

      {shownKind === 'picture' && (
        <div className="stack tight">
          <p className="help">{S.pictureHelp}</p>
          {!hasImages && <p className="help">{S.noPicture}</p>}
          <div className="picture-row">
            <PictureSample id={id} choice={choice} />
            <div className="stack tight picture-controls">
              {kind !== 'picture' && <p className="help">{S.noPicture}</p>}
              {hasImages && <div className="row"><button type="button" className="btn primary" data-testid="stand-choose-picture" onClick={() => setPicking(true)}>{kind === 'picture' ? S.changePicture : S.choosePicture}</button></div>}
            </div>
          </div>
          {kind === 'picture' && (
            <Field label={`${S.width} (${S.widthUnit})`} help={S.widthHelp}>
              <NumberInput allowEmpty min={STAND_WIDTH.min} max={STAND_WIDTH.max} value={f.width} onChange={(v) => edit((d) => setStandWidth(d, slot, v), `stand-width:${id}`)} />
            </Field>
          )}
        </div>
      )}
      {shownKind === 'model' && <ModelPart slot={slot} id={id} files={standModels(stand)} fields={f} />}
      {kind !== 'none' && <Check label={S.scatter} help={S.scatterHelp} checked={f.scatter} onChange={(v) => edit((d) => setStandScatter(d, slot, v))} />}

      {picking && (
        <PickerDialog kind="terrain" standing thingId={id} look={choice} what={S.pickerWhat(name)} current={standPictureLook(stand)}
          apply={(d, l) => setStandPicture(d, slot, l)}
          onClose={() => setPicking(false)}
          onChoose={(l) => { edit((d) => setStandPicture(d, slot, l)); setPicking(false); setPending(null) }} />
      )}
    </section>
  )
}

function Radio({ on, label, onPick }: { on: boolean; label: string; onPick: () => void }) {
  return <button type="button" role="radio" aria-checked={on} className="pick pick-inline" onClick={onPick}><span className="pick-name">{label}</span></button>
}

function PictureSample({ id, choice }: { id: string; choice: LookChoice }) {
  const spec = useStandSample(id, choice)
  return <div className="picture-preview">{spec && <ArtCanvas spec={spec} label={S.picture} />}</div>
}

/** Model files (one or several), add / upload, height and turn. */
function ModelPart({ slot, id, files, fields }: { slot: Slot; id: string; files: string[]; fields: ReturnType<typeof standFields> }) {
  const store = useStore()
  const edit = useEdit()
  const lib = useSyncExternalStore(store.models.subscribe, store.models.getState)
  const picker = useRef<HTMLInputElement>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const known = new Set(lib.files.map((m) => m.file))

  async function upload(picked: File) {
    setError(null)
    if (picked.size > MAX_MODEL_BYTES) { setError(M.tooBig(picked.name)); return }
    const bytes = new Uint8Array(await picked.arrayBuffer())
    if (!readGlbFacts(bytes)) { setError(M.notGlb(picked.name)); return }
    setBusy(true)
    try {
      const blob = new Blob([bytes], { type: 'model/gltf-binary' })
      const taken = () => store.models.getState().files.map((x) => x.file)
      let res = await store.models.upload(modelNameFor(picked.name, taken()), blob, false)
      for (let i = 0; !res.ok && res.status === 409 && i < 3; i++) {
        await store.models.refresh()
        res = await store.models.upload(modelNameFor(picked.name, taken()), blob, false)
      }
      if (!res.ok) { setError(M.uploadFailed(picked.name, res.error)); return }
      const file = res.value.file
      edit((d) => addStandModel(d, slot, file))
    } finally {
      setBusy(false)
    }
  }
  const addable: Option[] = lib.files.filter((m) => !files.includes(m.file)).map((m) => ({ value: m.file, label: m.file.replace(/^models\//, '') }))

  return (
    <div className="stack tight">
      <p className="help">{M.noPreview}</p>
      <Field label={S.models} help={S.modelsHelp}>
        <div className="stack tight">
          {files.length === 0 && <p className="help">{S.noModels}</p>}
          {files.length > 0 && (
            <ul className="side-list" aria-label={S.models}>
              {files.map((file) => (
                <li key={file}>
                  <span>{known.has(file) ? file.replace(/^models\//, '') : S.fileMissing(file)}</span>
                  <button type="button" className="btn sm" aria-label={S.removeModel(file.replace(/^models\//, ''))} onClick={() => edit((d) => removeStandModel(d, slot, file))}>{S.remove}</button>
                </li>
              ))}
            </ul>
          )}
          {addable.length > 0 && (
            <Select value="" placeholder={S.addModel} aria-label={S.addModel} options={addable} onChange={(v) => { if (v) edit((d) => addStandModel(d, slot, v)) }} />
          )}
        </div>
      </Field>
      <div className="row">
        <button type="button" className="btn" disabled={busy} onClick={() => picker.current?.click()}>{busy ? S.uploading : S.upload}</button>
      </div>
      <input ref={picker} type="file" accept=".glb,model/gltf-binary" hidden aria-label={S.upload} data-testid="upload-stand-model-input"
        onChange={(e) => { const p = e.target.files?.[0]; e.target.value = ''; if (p) void upload(p) }} />
      {error && <Banner tone="error" onDismiss={() => setError(null)}><p>{error}</p></Banner>}
      {files.length > 0 && (
        <>
          <Field label={`${S.height} (${S.heightUnit})`} help={S.heightHelp}>
            <NumberInput integer={false} allowEmpty min={STAND_HEIGHT.min} max={STAND_HEIGHT.max} value={fields.height} onChange={(v) => edit((d) => setStandHeight(d, slot, v), `stand-height:${id}`)} />
          </Field>
          <Field label={`${S.turn} (${S.turnUnit})`} help={S.turnHelp}>
            <NumberInput integer={false} allowEmpty min={STAND_TURN.min} max={STAND_TURN.max} value={fields.turn} onChange={(v) => edit((d) => setStandTurn(d, slot, v), `stand-turn:${id}`)} />
          </Field>
        </>
      )}
    </div>
  )
}
