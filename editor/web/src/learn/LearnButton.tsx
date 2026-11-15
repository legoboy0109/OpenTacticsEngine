// The Learn button of the top bar and the game list.
import { Icon } from '../ui/Icon'
import { learn } from './instance'
import { T } from './text'

/** `compact`: the text is dropped on phones (the top bar has no room); the game list keeps it. */
export function LearnButton({ className = 'btn ghost', compact = true }: { className?: string; compact?: boolean }) {
  return (
    <button type="button" className={className} data-learn="learn" onClick={() => learn.openList()} aria-label={T.learnButton} title={T.learnTitle}>
      <Icon name="learn" /><span className={compact ? 'btn-text' : undefined}>{T.learnButton}</span>
    </button>
  )
}
