// Wording of the door, chest and place triggers and commands in the Events tab (docs/EVENTS.md; placeholders for the owner to review).
import type { PlaceEvent } from '../../../../../model/storyTypes'

export const P = {
  noneYetShort: 'no such place yet',
  placeEvents: { opened: 'It is opened', visited: 'It is visited', destroyed: 'It is destroyed' } satisfies Record<PlaceEvent, string>,
  placeEvent: 'What happens to it',
  placeEventHelp: 'Opened: a door or chest was opened (by a key, a unit, a staff, by breaking it, or by an event). Visited: a unit visited the place. Destroyed: an enemy of the place’s owner destroyed it.',
  place: 'Which one',
  placeHelp: 'The doors, chests and places of this map that have a story name. Give one a story name in the map’s Structures tool.',
  noPlaces: 'This map has no door, chest or place with a story name yet. Place one with the Structures tool and give it a story name in its panel.',
  noPlacesFor: (what: string) => `This map has no ${what} with a story name yet. Place one with the Structures tool and give it a story name in its panel.`,
  placeWho: 'Done by',
  placeWhoHelp: 'Optional. Leave it on “Anyone” to run the event whoever did it, an event or a unit.',
  openDoorHelp: 'Opens a shut door at once, locked or not. Its look and its movement rules become those of its open kind.',
  closeDoorHelp: 'Shuts an open door again. It is skipped while a unit stands in the doorway.',
  lockDoorHelp: 'Shuts the door and locks it, or only locks a door that is already shut.',
  unlockDoorHelp: 'Takes the lock off a door or chest. It stays shut; anyone can then open it without a key.',
  openChestHelp: 'Opens a chest at once, locked or not.',
  chestWho: 'Who gets what is inside',
  chestWhoHelp: 'Optional. Without a unit, the contents go to the first human side’s storage and funds.',
  chestNobody: 'Nobody in particular',
}
