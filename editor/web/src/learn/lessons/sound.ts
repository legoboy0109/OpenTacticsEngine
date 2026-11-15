// Lesson: "Sound". Starts from the blank template, which already comes with six music tracks (title, battle, enemy, victory,
// defeat, levelup) and a set of sound effects, all chosen for the standard events. So the lesson never needs a file of the
// maker's own: it previews, adjusts and re-chooses what the game already has. Nothing below is already true at the start.
import { firstMapId } from '../helpers'
import { eventSoundsReplaced, mapHasOwnMusic, tracksQuieter, weaponsWithOwnSound } from '../helpers/sound'
import type { Lesson } from '../types'

/** The blank template's standard choices for "when things happen" (data/audio.json, use.sounds): event -> sound. */
export const START_SOUNDS: Record<string, string> = {
  cursor: 'cursor', confirm: 'confirm', cancel: 'cancel', error: 'error', step: 'step', hit: 'hit', miss: 'miss', critical: 'critical',
  heal: 'heal', defeated: 'defeated', capture: 'capture', build: 'build', itemUse: 'item_use', gold: 'gold', dialogue: 'dialogue',
  turnStart: 'turn_start', upgrade: 'upgrade', research: 'research', repair: 'repair', supply: 'supply',
}

export const sound: Lesson = {
  id: 'sound',
  title: 'Sound',
  description: 'Listen to the music, make it quieter, pick music for a map, and change what a hit sounds like.',
  minutes: 6,
  template: 'blank',
  playable: true,
  summary: [
    'A music track you turned down.',
    'A map with music of its own.',
    'A different sound for something that happens in battle, and a weapon with a sound of its own.',
    'A game that has no problems and that you played.',
  ],
  steps: [
    {
      id: 'listen',
      title: 'Listen to the music',
      text: 'This game already has music and sounds. On the Sound screen, click the play button next to a music track to hear it. Click it again to stop.',
      where: { section: 'sound' },
      point: 'sound-play',
      done: (_doc, _screen, signals) => signals.has('soundPreviewed'),
    },
    {
      id: 'volume',
      title: 'Turn a track down',
      text: 'Music can be loud next to sound effects. Pick a track and lower its Volume (0 is silent, 100 is full).',
      where: { section: 'sound' },
      point: 'track-volume',
      // Typing "50" passes through "5": the tick waits for a short pause.
      settleMs: 1200,
      done: (doc) => tracksQuieter(doc) >= 1,
    },
    {
      id: 'map-music',
      title: 'Pick music for a map',
      text: 'Each map can have its own music. On the Maps screen, open the Map tab, and under Music choose a track instead of Same as the game.',
      where: { section: 'maps', item: firstMapId },
      point: 'map-music|map-tab-map|list-item',
      done: (doc) => mapHasOwnMusic(doc),
    },
    {
      id: 'event-sound',
      title: 'Change what a hit sounds like',
      text: 'Back on the Sound screen, find When things happen. Next to An attack hits, choose a different sound, then press its play button to hear it.',
      where: { section: 'sound' },
      point: 'sound-event-hit',
      done: (doc) => eventSoundsReplaced(doc, START_SOUNDS) >= 1,
    },
    {
      id: 'weapon-sound',
      title: 'Give a weapon its own sound',
      text: 'On the Weapons screen, open the Sword. Under Sounds, next to When it hits, choose a sound. Only this weapon will use it.',
      where: { section: 'weapons', item: (doc) => doc.weapons?.[0]?.id },
      point: 'own-sound-hit',
      done: (doc) => weaponsWithOwnSound(doc) >= 1,
    },
    {
      id: 'play',
      title: 'Play it and listen',
      text: 'Click Play. You will hear your map’s music, and the sword’s sound when a unit attacks.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
  ],
}
