// Up / down / delete buttons of one row in a story list. `name` makes the buttons' names unique ("Move command 2 up").
import { Icon } from './Icon'

interface Props {
  index: number
  count: number
  onUp: () => void
  onDown: () => void
  onDelete: () => void
  up: string
  down: string
  del: string
}

export function RowButtons({ index, count, onUp, onDown, onDelete, up, down, del }: Props) {
  return (
    <span className="item-row-buttons">
      <button type="button" className="btn ghost sm icon" aria-label={up} disabled={index === 0} onClick={onUp}><Icon name="up" /></button>
      <button type="button" className="btn ghost sm icon" aria-label={down} disabled={index === count - 1} onClick={onDown}><Icon name="down" /></button>
      <button type="button" className="btn ghost sm icon" aria-label={del} onClick={onDelete}><Icon name="trash" /></button>
    </span>
  )
}
