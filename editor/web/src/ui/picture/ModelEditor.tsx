// The "3D model" part of the Picture card (3D isometric styles only): choose or upload a model file and set how it is
// used. Edits go through model/models3d.ts; the file's own facts (animation and material names) are read in the browser.
import { useRef, useState, useSyncExternalStore } from 'react'
import type { Slot } from '../../model/graphicsLooks'
import {
  ANIM_STATES, DEFAULT_ANIM, DEFAULT_SIDE_MATERIAL, HEIGHT_RANGE, MAX_MODEL_BYTES, TURN_RANGE, modelFields, modelFile, modelNameFor, readGlbFacts, setAnimation, setModelFile,
  setModelHeight, setModelTurn, setSideMaterial, type AnimState, type ModelFacts,
} from '../../model/models3d'
import type { Look } from '../../model/types'
import { useEdit, useStore } from '../../store/context'
import { Banner } from '../Banner'
import { Field, FieldGroup } from '../Field'
import { NumberInput } from '../NumberInput'
import { Select, type Option } from '../Select'
import { M } from './modelText'

/** Options for a list of names from the file, plus the stored name when the file lacks it; '' is "None". */
function nameOptions(names: string[], stored: string | undefined): Option[] {
  const extra = stored && !names.includes(stored) ? [{ value: stored, label: M.notInFile(stored) }] : []
  return [{ value: '', label: M.none }, ...names.map((n) => ({ value: n, label: n })), ...extra]
}
/** The select's value: the stored name, but a default the file does not have shows as None (nothing is written for it). */
const shown = (value: string, names: string[], dflt: string): string => (names.includes(value) || value !== dflt ? value : '')

export function ModelEditor({ slot, entry, structure }: { slot: Slot; entry: Look | null | undefined; structure: boolean }) {
  const store = useStore()
  const edit = useEdit()
  const lib = useSyncExternalStore(store.models.subscribe, store.models.getState)
  const picker = useRef<HTMLInputElement>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const file = modelFile(entry)
  const known = lib.files.some((f) => f.file === file)
  const state = file ? store.models.factsOf(file) : null
  const facts: ModelFacts | null = state && state !== 'loading' ? state : null
  const f = modelFields(entry)
  const names = facts?.animations ?? []
  const materials = facts?.materials ?? []

  async function upload(picked: File) {
    setError(null)
    if (picked.size > MAX_MODEL_BYTES) { setError(M.tooBig(picked.name)); return }
    const bytes = new Uint8Array(await picked.arrayBuffer())
    if (!readGlbFacts(bytes)) { setError(M.notGlb(picked.name)); return }
    setBusy(true)
    try {
      const blob = new Blob([bytes], { type: 'model/gltf-binary' })
      const taken = () => store.models.getState().files.map((x) => x.file)
      // The name is unique among the files that exist now; if another upload took it meanwhile, look again.
      let res = await store.models.upload(modelNameFor(picked.name, taken()), blob, false)
      for (let i = 0; !res.ok && res.status === 409 && i < 3; i++) {
        await store.models.refresh()
        res = await store.models.upload(modelNameFor(picked.name, taken()), blob, false)
      }
      if (!res.ok) { setError(M.uploadFailed(picked.name, res.error)); return }
      edit((d) => setModelFile(d, slot, res.ok ? res.value.file : ''))
    } finally {
      setBusy(false)
    }
  }

  const fileOptions: Option[] = [...lib.files.map((m) => m.file), ...(file && !known ? [file] : [])]
    .map((m) => ({ value: m, label: m === file && !known ? M.fileMissing(m) : m.replace(/^models\//, '') }))

  return (
    <div className="stack tight">
      <p className="help">{M.noPreview}</p>
      <Field label={M.file} help={M.fileHelp}>
        <Select value={file ?? ''} placeholder={M.filePlaceholder} options={fileOptions} onChange={(v) => edit((d) => setModelFile(d, slot, v))} />
      </Field>
      <div className="row">
        <button type="button" className="btn" disabled={busy} onClick={() => picker.current?.click()}>{busy ? M.uploading : M.upload}</button>
      </div>
      <input ref={picker} type="file" accept=".glb,model/gltf-binary" hidden aria-label={M.upload} data-testid="upload-model-input"
        onChange={(e) => { const p = e.target.files?.[0]; e.target.value = ''; if (p) void upload(p) }} />
      {error && <Banner tone="error" onDismiss={() => setError(null)}><p>{error}</p></Banner>}
      {file && state === 'loading' && <p className="help" role="status">{M.reading}</p>}
      {file && state === null && known && <p className="help">{M.unreadable}</p>}
      {file && (
        <>
          <Field label={`${M.height} (${M.heightUnit})`} help={structure ? M.heightHelpStructure : M.heightHelpUnit}>
            <NumberInput integer={false} allowEmpty min={HEIGHT_RANGE.min} max={HEIGHT_RANGE.max} value={f.height} onChange={(v) => edit((d) => setModelHeight(d, slot, v), `model-height:${slot.id}`)} />
          </Field>
          <Field label={`${M.turn} (${M.turnUnit})`} help={M.turnHelp}>
            <NumberInput integer={false} allowEmpty min={TURN_RANGE.min} max={TURN_RANGE.max} value={f.turn} onChange={(v) => edit((d) => setModelTurn(d, slot, v), `model-turn:${slot.id}`)} />
          </Field>
          <Field label={M.sideMaterial} help={M.sideMaterialHelp}>
            <Select value={shown(f.sideMaterial, materials, DEFAULT_SIDE_MATERIAL)} options={nameOptions(materials, shown(f.sideMaterial, materials, DEFAULT_SIDE_MATERIAL))} disabled={!facts}
              onChange={(v) => edit((d) => setSideMaterial(d, slot, v))} />
          </Field>
          <FieldGroup label={M.animations} help={M.animationsHelp}>
            <div className="stack tight">
              {ANIM_STATES.map((s: AnimState) => (
                <Field key={s} label={M.states[s]}>
                  <Select value={shown(f.animations[s], names, DEFAULT_ANIM[s])} options={nameOptions(names, shown(f.animations[s], names, DEFAULT_ANIM[s]))} disabled={!facts}
                    onChange={(v) => edit((d) => setAnimation(d, slot, s, v))} />
                </Field>
              ))}
            </div>
          </FieldGroup>
        </>
      )}
    </div>
  )
}
