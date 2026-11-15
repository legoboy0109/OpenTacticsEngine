// The Sound section: music, sound effects and "when things happen" (the game's standard set). With no sound yet it explains
// itself and offers the two "Add" buttons. Everything is undoable and autosaved like the rest; the files themselves are not
// part of Undo (they are uploaded and deleted on disk).
import { useRef, useState } from 'react'
import { addSound, addTrack, deleteSound, deleteTrack, musicOf, newSoundId, newTrackId, removeAllAudio, soundsOf } from '../../model/audio'
import { allAudioUsageLines, audioUsageLines, renameAudioId } from '../../model/audioRefs'
import { displayStem } from '../../model/audioFiles'
import { makeId } from '../../model/ids'
import { Needs } from '../../shell/Blocked'
import { useDoc, useEdit, useProject } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { Card, PageHeader } from '../../ui/Card'
import { DeleteConfirm } from '../../ui/DeleteConfirm'
import { NameDialog } from '../../ui/NameDialog'
import { AudioChecks } from './AudioChecks'
import { EventsCard } from './EventsCard'
import { FilesCard } from './FilesCard'
import { MusicCard } from './MusicCard'
import { SoundsCard } from './SoundsCard'
import { T } from './text'
import { UploadProvider, useAudioUpload } from './useAudioUpload'

export function SoundScreen() {
  return (
    <div className="screen wide">
      <Needs parts={['game']}><SoundGate /></Needs>
    </div>
  )
}

/** A damaged data/audio.json loads as "no sound"; editing would overwrite it, so refuse until it is fixed. */
function SoundGate() {
  const { doc, problems } = useProject()
  const bad = problems.filter((p) => p.file === 'data/audio.json')
  if (doc.audio === null && bad.length > 0) {
    return (
      <Banner tone="error" title={T.blockedTitle}>
        {bad.map((p, i) => <p key={i}>{p.message}</p>)}
        <p>{T.blockedBody}</p>
      </Banner>
    )
  }
  return <SoundBody />
}

type Kind = 'music' | 'sound'
interface Naming { kind: Kind; file: string; stem: string }

function SoundBody() {
  const doc = useDoc()
  const edit = useEdit()
  const { upload, notices, say, dismiss } = useAudioUpload()
  const musicInput = useRef<HTMLInputElement>(null)
  const soundInput = useRef<HTMLInputElement>(null)
  const [naming, setNaming] = useState<Naming | null>(null)
  const [renaming, setRenaming] = useState<{ kind: Kind; id: string } | null>(null)
  const [deleting, setDeleting] = useState<{ kind: Kind; id: string } | null>(null)
  const [removingAll, setRemovingAll] = useState(false)

  const tracks = Object.entries(musicOf(doc))
  const sounds = Object.entries(soundsOf(doc))
  const empty = tracks.length === 0 && sounds.length === 0

  const choose = async (kind: Kind, f: File | undefined) => {
    if (!f) return
    const file = await upload(f)
    if (file) setNaming({ kind, file, stem: displayStem(f.name) })
  }
  const named = (name: string) => {
    if (!naming) return
    const { kind, file } = naming
    setNaming(null)
    edit((d) => (kind === 'music' ? addTrack(d, newTrackId(d, name), file) : addSound(d, newSoundId(d, name), file)))
  }
  const renamed = (name: string) => {
    if (!renaming) return
    const { kind, id } = renaming
    setRenaming(null)
    const taken = Object.keys(kind === 'music' ? musicOf(doc) : soundsOf(doc)).filter((x) => x !== id)
    const next = makeId(name, taken, { fallback: kind === 'music' ? 'track' : 'sound' })
    if (next !== id) edit((d) => renameAudioId(d, kind, id, next))
  }
  const deleted = () => {
    if (!deleting) return
    const { kind, id } = deleting
    setDeleting(null)
    edit((d) => (kind === 'music' ? deleteTrack(d, id) : deleteSound(d, id)))
  }

  return (
    <UploadProvider upload={upload}>
      <div className="stack">
        <PageHeader title={T.title} help={T.intro} />
        <input ref={musicInput} type="file" hidden accept=".wav,.ogg,audio/wav,audio/x-wav,audio/ogg" aria-label={T.addMusic} data-testid="add-music-input"
          onChange={(e) => { const f = e.target.files?.[0]; e.target.value = ''; void choose('music', f) }} />
        <input ref={soundInput} type="file" hidden accept=".wav,.ogg,audio/wav,audio/x-wav,audio/ogg" aria-label={T.addSound} data-testid="add-sound-input"
          onChange={(e) => { const f = e.target.files?.[0]; e.target.value = ''; void choose('sound', f) }} />
        {notices.length > 0 && (
          <div className="stack tight" aria-live="polite">
            {notices.map((n) => <Banner key={n.id} tone={n.tone} onDismiss={() => dismiss(n.id)}><p>{n.text}</p></Banner>)}
          </div>
        )}
        <AudioChecks section="sound" />
        {empty
          ? (
            <Card title={T.emptyTitle}>
              <div className="stack tight" data-testid="no-sound">
                <p>{T.emptyBody}</p>
                <p className="help">{T.emptyFormats}</p>
                <div className="row">
                  <button type="button" className="btn primary" onClick={() => musicInput.current?.click()}>{T.addMusic}</button>
                  <button type="button" className="btn primary" onClick={() => soundInput.current?.click()}>{T.addSound}</button>
                </div>
              </div>
            </Card>
          )
          : (
            <>
              <MusicCard tracks={tracks} onAdd={() => musicInput.current?.click()} onRename={(id) => setRenaming({ kind: 'music', id })} onDelete={(id) => setDeleting({ kind: 'music', id })} />
              <SoundsCard sounds={sounds} onAdd={() => soundInput.current?.click()} onRename={(id) => setRenaming({ kind: 'sound', id })} onDelete={(id) => setDeleting({ kind: 'sound', id })} />
              <EventsCard />
            </>
          )}
        <FilesCard say={say} onUse={(kind, file) => setNaming({ kind, file, stem: displayStem(file.replace(/^audio\//, '')) })} />
        {doc.audio && (
          <Card title={T.removeAllTitle} help={T.removeAllHelp}>
            <div><button type="button" className="btn danger" onClick={() => setRemovingAll(true)}>{T.removeAllButton}</button></div>
          </Card>
        )}
      </div>

      <NameDialog open={!!naming} title={naming?.kind === 'music' ? T.nameMusicTitle : T.nameSoundTitle} label={T.nameLabel} help={T.nameHelp} confirmLabel={T.nameCreate}
        initial={naming?.stem ?? ''} onCancel={() => setNaming(null)} onSubmit={named} />
      <NameDialog open={!!renaming} title={renaming ? T.renameTitle(renaming.kind) : ''} label={T.nameLabel} help={T.nameHelp} confirmLabel={T.renameConfirm}
        initial={renaming?.id ?? ''} onCancel={() => setRenaming(null)} onSubmit={renamed} />
      <DeleteConfirm open={!!deleting} what={deleting ? (deleting.kind === 'music' ? T.deleteMusicWhat(deleting.id) : T.deleteSoundWhat(deleting.id)) : ''}
        usages={deleting ? audioUsageLines(doc, deleting.kind, deleting.id) : []} onCancel={() => setDeleting(null)} onConfirm={deleted} />
      <DeleteConfirm open={removingAll} what={T.removeAllWhat} usages={removingAll ? allAudioUsageLines(doc) : []} onCancel={() => setRemovingAll(false)}
        onConfirm={() => { setRemovingAll(false); edit(removeAllAudio) }} />
    </UploadProvider>
  )
}
