// The amber banner of sound mistakes: what the player's sound loader would refuse (model/audioChecks.ts), on the Sound
// screen and on the Weapons, Units and Items screens for their own part. Files the game names but does not have are only a
// "worth a look": the player starts without that sound.
import { useMemo } from 'react'
import { audioChecks } from '../../model/audioChecks'
import type { Section } from '../../model/types'
import { useDoc } from '../../store/context'
import { Banner } from '../../ui/Banner'
import { useAudioFileList } from './sources'
import { T } from './text'

export function AudioChecks({ section }: { section: Section }) {
  const doc = useDoc()
  const { files, loaded } = useAudioFileList()
  const list = useMemo(() => audioChecks(doc, loaded ? new Map(files.map((f) => [f.file, f])) : null).filter((c) => c.section === section), [doc, files, loaded, section])
  const errors = list.filter((c) => c.level === 'error')
  const hints = list.filter((c) => c.level === 'hint')
  if (list.length === 0) return null
  return (
    <>
      {errors.length > 0 && (
        <Banner tone="warn" title={T.checksTitle}>
          <ul className="usage-list" data-testid="audio-checks">{errors.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
          <p className="help">{T.checksHelp}</p>
        </Banner>
      )}
      {hints.length > 0 && (
        <Banner tone="warn" title={T.hintsTitle}>
          <ul className="usage-list" data-testid="audio-hints">{hints.map((c, i) => <li key={i}>{c.text}</li>)}</ul>
        </Banner>
      )}
    </>
  )
}
