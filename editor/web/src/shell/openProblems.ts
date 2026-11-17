// Lets a screen ask the shell to open the Problems list (the same list the top bar's badge opens).
const EVENT = 'ote:open-problems'

export const openProblemsList = (): void => { window.dispatchEvent(new Event(EVENT)) }

export function onOpenProblems(fn: () => void): () => void {
  window.addEventListener(EVENT, fn)
  return () => window.removeEventListener(EVENT, fn)
}
