// What a play button plays for a track or a sound, and the buttons themselves (they need the open game's files).
import { useSyncExternalStore } from 'react'
import { tracksRepeats, filesOfSound } from '../../model/audio'
import type { MusicTrack, SoundEffect } from '../../model/audioTypes'
import { useStore } from '../../store/context'
import { PlayButton } from './PlayButton'
import { T } from './text'

/** The sound files of the open game and a version that changes when the list does. */
export function useAudioFileList() {
  const lib = useStore().audioFiles
  return useSyncExternalStore(lib.subscribe, lib.getState)
}

/** Plays a music track the way the game does: it repeats (from its "Repeat from" time) unless it is a jingle. */
export function TrackPlay({ id, track, playKey = `music:${id}` }: { id: string; track: MusicTrack; playKey?: string }) {
  const lib = useStore().audioFiles
  const { files, loaded } = useAudioFileList()
  const there = typeof track.file === 'string' && track.file !== '' && (!loaded || files.some((f) => f.file === track.file))
  return (
    <PlayButton playKey={playKey} what={id} playable={there} why={track.file ? T.fileMissing : T.noFileToPlay}
      source={() => ({ url: lib.url(track.file), opts: { volume: track.volume ?? 100, loop: tracksRepeats(track), loopStart: track.loopStart } })} />
  )
}

/** Plays a sound effect; with several variants it plays a random one each time, like the game does. */
export function SoundPlay({ id, sound, playKey = `sound:${id}` }: { id: string; sound: SoundEffect; playKey?: string }) {
  const lib = useStore().audioFiles
  const { files, loaded } = useAudioFileList()
  const list = filesOfSound(sound).filter((f) => !loaded || files.some((x) => x.file === f))
  return (
    <PlayButton playKey={playKey} what={id} playable={list.length > 0} why={filesOfSound(sound).length ? T.fileMissing : T.noFileToPlay}
      source={() => (list.length ? { url: lib.url(list[Math.floor(Math.random() * list.length)]), opts: { volume: sound.volume ?? 100 } } : null)} />
  )
}

/** One file of the audio folder, played on its own (a variant, a file in the list). */
export function FilePlay({ playKey, what, file, volume = 100 }: { playKey: string; what: string; file: string; volume?: number }) {
  const lib = useStore().audioFiles
  const { files, loaded } = useAudioFileList()
  const there = file !== '' && (!loaded || files.some((f) => f.file === file))
  return <PlayButton playKey={playKey} what={what} playable={there} why={file ? T.fileMissing : T.noFileToPlay} source={() => ({ url: lib.url(file), opts: { volume } })} />
}
