import { useProject, useStore } from '../store/context'
import { Banner } from '../ui/Banner'

/** Conflict, save failure and one-off notices, shown above the section. */
export function SaveBanners() {
  const store = useStore()
  const { save, notice } = useProject()
  return (
    <div className="banners">
      {save.state === 'conflict' && (
        <Banner tone="error" title="This game changed on disk (another program or device edited it)."
          actions={(
            <>
              <button type="button" className="btn sm" onClick={() => void store.reload()}>Reload</button>
              <button type="button" className="btn sm danger" onClick={() => void store.keepMine()}>Keep mine</button>
            </>
          )}>
          <b>Reload</b> drops your unsaved changes and shows what is on disk. <b>Keep mine</b> overwrites the disk with what you see here.
        </Banner>
      )}
      {save.state === 'error' && (
        <Banner tone="warn" title="Could not save. Your changes are kept here and the editor keeps trying.">
          {save.message}
        </Banner>
      )}
      {notice && <Banner tone="warn" onDismiss={() => store.setNotice(null)}>{notice}</Banner>}
    </div>
  )
}
