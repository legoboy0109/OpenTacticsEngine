// The three questions the timeline asks: the name of a new chapter, "remove this chapter?" (with where it is used) and
// "remove this scene?". They are the editor's usual dialogs.
import type { SceneKey } from '../../../model/campaignTypes'
import type { Usage } from '../../../model/references'
import { ConfirmDialog } from '../../../ui/Dialog'
import { DeleteConfirm } from '../../../ui/DeleteConfirm'
import { NameDialog } from '../../../ui/NameDialog'
import { C } from '../text'
import { TL } from './text'

export type Asking =
  | { kind: 'new'; index: number; between?: [string, string] }
  | { kind: 'delete'; chapterId: string }
  | { kind: 'scene'; chapterId: string; which: SceneKey; commands: number }

interface Props {
  asking: Asking | null
  onClose: () => void
  onCreate: (name: string, index: number) => void
  usages: (chapterId: string) => Usage[]
  onDelete: (chapterId: string) => void
  onRemoveScene: (chapterId: string, which: SceneKey) => void
  nameOf: (chapterId: string) => string
}

export function TimelineDialogs({ asking, onClose, onCreate, usages, onDelete, onRemoveScene, nameOf }: Props) {
  return (
    <>
      <NameDialog open={asking?.kind === 'new'} title={TL.newChapterTitle} label={TL.newChapterLabel} help={TL.newChapterHelp} onCancel={onClose}
        onSubmit={(name) => { if (asking?.kind === 'new') onCreate(name, asking.index) }} />
      <DeleteConfirm open={asking?.kind === 'delete'} what={asking?.kind === 'delete' ? C.deleteChapterWhat(nameOf(asking.chapterId)) : ''}
        usages={asking?.kind === 'delete' ? usages(asking.chapterId) : []} onCancel={onClose}
        onConfirm={() => { if (asking?.kind === 'delete') onDelete(asking.chapterId) }} />
      <ConfirmDialog open={asking?.kind === 'scene'} danger confirmLabel={TL.removeSceneConfirm}
        title={asking?.kind === 'scene' ? TL.removeSceneTitle(asking.which === 'before' ? TL.sceneBefore : TL.sceneAfter, nameOf(asking.chapterId)) : ''}
        onCancel={onClose} onConfirm={() => { if (asking?.kind === 'scene') onRemoveScene(asking.chapterId, asking.which) }}>
        <p>{asking?.kind === 'scene' ? TL.removeSceneBody(asking.commands) : ''}</p>
        <p className="help">{TL.removeSceneUndo}</p>
      </ConfirmDialog>
    </>
  )
}
