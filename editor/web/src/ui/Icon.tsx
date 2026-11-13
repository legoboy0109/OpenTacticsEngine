// Small stroke icons (no emoji, no icon library).
const PATHS = {
  undo: 'M9 14L4 9l5-5M4 9h10.5a5.5 5.5 0 0 1 0 11H11',
  redo: 'M15 14l5-5-5-5M20 9H9.5a5.5 5.5 0 0 0 0 11H13',
  plus: 'M12 5v14M5 12h14',
  trash: 'M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3',
  copy: 'M9 9h11v11H9zM5 15V4h11',
  up: 'M6 15l6-6 6 6',
  down: 'M6 9l6 6 6-6',
  back: 'M15 5l-7 7 7 7',
  warn: 'M12 3l10 18H2zM12 10v5M12 18v.01',
  play: 'M7 4l13 8-13 8z',
  check: 'M5 12l5 5 9-10',
  close: 'M6 6l12 12M18 6L6 18',
  info: 'M12 11v6M12 7.5v.01M3 12a9 9 0 1 0 18 0 9 9 0 1 0-18 0',
} as const

export type IconName = keyof typeof PATHS

export function Icon({ name, className }: { name: IconName; className?: string }) {
  return (
    <svg className={`icon-svg ${className ?? ''}`} viewBox="0 0 24 24" fill="none" stroke="currentColor"
      strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" focusable="false">
      <path d={PATHS[name]} />
    </svg>
  )
}
