// Editor signals: things that happen in the editor but leave no trace in the game document (docs/TUTORIAL.md).
// A lesson's done test can ask `signals.has('played')`.
//
// To add a signal: (1) add its name to SignalName below, (2) call `emitSignal('name')` where it happens.
// Nothing else; the runner records every signal that arrives while a lesson runs.
export type SignalName =
  | 'played' // the Play panel reported that the embedded player is ready
  | 'saved' // the game was written to disk (autosave or Play)
  | 'problemsChecked' // the Problems list was opened
  | 'pictureChooser' // the "Choose a picture" dialog was opened (Terrain, Units or Structures screen)

type Listener = (name: SignalName) => void

/** A tiny publish/subscribe. The editor uses the shared `signalBus`; tests make their own. */
export class SignalBus {
  private listeners = new Set<Listener>()
  subscribe = (fn: Listener): (() => void) => { this.listeners.add(fn); return () => { this.listeners.delete(fn) } }
  emit = (name: SignalName): void => { for (const fn of [...this.listeners]) fn(name) }
}

export const signalBus = new SignalBus()
/** Say that something happened. Cheap and harmless when no lesson is running. */
export const emitSignal = (name: SignalName): void => signalBus.emit(name)
