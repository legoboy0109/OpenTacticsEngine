// "Open this event": the campaign timeline sends the person to a map and asks the map editor to show one event in its Events tab.
// The request waits here (a few seconds at most) until the editor for that map has mounted and used it.
interface Request { mapId: string; eventId: string; at: number }
let pending: Request | null = null
const FRESH_MS = 10_000

export function requestOpenEvent(mapId: string, eventId: string): void {
  pending = { mapId, eventId, at: Date.now() }
}

/** The event id asked for on this map, if a fresh request is waiting. Reading does not use it up (a component may render twice). */
export function peekOpenEvent(mapId: string): string | null {
  return pending && pending.mapId === mapId && Date.now() - pending.at < FRESH_MS ? pending.eventId : null
}

export function clearOpenEvent(): void {
  pending = null
}
