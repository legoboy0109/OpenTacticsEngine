// All wording of the Sound screen and of the small "Sounds" / "Music" groups on the Weapons, Units, Items, Maps and Campaign
// screens. Placeholder wording: the owner decides. (The names of the standard events and music slots are in
// model/audioLabels.ts, next to the keys they name.)
import type { AudioExt } from '../../model/audioFiles'

const plural = (n: number, one: string, many: string): string => (n === 1 ? `1 ${one}` : `${n} ${many}`)

export const T = {
  title: 'Sound',
  intro: 'Music and sound effects. Add sound files (.wav or .ogg), give them names, then say when each one plays.',

  // --- blocked ---
  blockedTitle: 'Sound can’t be opened: data/audio.json could not be read.',
  blockedBody: 'Nothing here will be changed or saved. Fix or restore that file (it is in the game folder), then reload this page.',

  // --- empty ---
  emptyTitle: 'This game has no sound yet',
  emptyBody: 'Music plays under the title screen, the battles and the story scenes. Sound effects play for menus, steps, hits and other things that happen. Start by adding a file: you can add more later.',
  emptyFormats: 'Files must be .wav or .ogg (up to 8 MB).',
  addMusic: 'Add music',
  addSound: 'Add a sound',

  // --- adding files ---
  addInputLabel: 'Add a sound file',
  chooseFile: 'Choose a file…',
  uploadAnother: 'Add another file…',
  dropHere: 'Drop sound files here',
  nameMusicTitle: 'Name this music',
  nameSoundTitle: 'Name this sound',
  nameLabel: 'Name',
  nameHelp: 'Letters, digits, _ and - (spaces become _). It is only for you, to find it again.',
  nameCreate: 'Add',
  renameTitle: (kind: 'music' | 'sound') => (kind === 'music' ? 'Rename this music' : 'Rename this sound'),
  renameConfirm: 'Rename',
  notSoundFile: (name: string) => `“${name}” is not a sound file. Use a .wav or .ogg file (some programs name other files like that: save it again as WAV or Ogg Vorbis).`,
  tooBig: (name: string) => `“${name}” is larger than 8 MB. Shorten it, or save it as .ogg (much smaller than .wav).`,
  wrongKind: (name: string, is: AudioExt, says: AudioExt) => `“${name}” is really a ${is.toUpperCase()} file but its name ends in .${says}. Rename it to end in .${is}.`,
  uploadFailed: (name: string, why: string) => `“${name}” was not added: ${why}`,
  helperDown: (name: string, why: string) => `“${name}” was not added. ${why}`,
  addedFiles: (n: number) => `${plural(n, 'file', 'files')} added.`,

  // --- music ---
  musicTitle: 'Music',
  musicHelp: 'Tracks that play in the background. The game plays one at a time.',
  noMusic: 'No music yet.',
  tracksCount: (n: number) => plural(n, 'track', 'tracks'),
  trackFile: 'Sound file',
  trackVolume: 'Volume',
  trackVolumeHelp: '0 to 100. Players also have their own music volume.',
  repeats: 'Repeats',
  repeatsHelp: 'On: starts again when it ends. Off: plays once (for a short tune such as “you win”).',
  repeatFrom: 'Repeat from',
  repeatFromHelp: 'Seconds. When it starts again it jumps back here, so an intro is not heard twice. 0 = the very start.',
  repeatFromNow: 'Use the playing position',
  repeatFromNowHelp: 'Play the track, then press this where the repeat should begin.',
  rename: 'Rename',
  delete: 'Delete',
  renameLabel: (name: string) => `Rename ${name}`,
  deleteLabel: (name: string) => `Delete ${name}`,
  deleteMusicWhat: (name: string) => `the music “${name}”`,
  deleteSoundWhat: (name: string) => `the sound “${name}”`,

  // --- sounds ---
  soundsTitle: 'Sounds',
  soundsHelp: 'Short effects. A sound can have several variants: one of them is picked each time, so a step does not sound the same every time.',
  noSounds: 'No sounds yet.',
  soundsCount: (n: number) => plural(n, 'sound', 'sounds'),
  variants: 'Variants',
  variantsHelp: 'One is picked each time the sound plays.',
  variantN: (n: number) => `Variant ${n}`,
  addVariant: 'Add a variant',
  removeVariant: (n: number) => `Remove variant ${n}`,
  variantsFull: 'A sound can have up to 16 variants.',
  soundFile: 'Sound file',
  soundVolume: 'Volume',
  soundVolumeHelp: '0 to 100. Players also have their own sound volume.',
  longEffect: (seconds: number) => `This file is ${seconds} seconds long; a sound effect can be at most 10 seconds.`,

  // --- play button ---
  play: 'Play',
  stop: 'Stop',
  playLabel: (what: string) => `Play ${what}`,
  stopLabel: (what: string) => `Stop ${what}`,
  noFileToPlay: 'There is no file to play yet.',
  fileMissing: 'The file is not in the game folder.',

  // --- when things happen ---
  eventsTitle: 'When things happen',
  eventsHelp: 'Pick what plays for each moment. “Nothing” means silence. These are the game’s standard choices; a weapon, a unit type, an item or a map can pick its own.',
  musicGroup: 'Music',
  nothing: 'Nothing',
  oncePlays: 'plays once',
  jingleNote: 'This plays once, so pick a track with “Repeats” switched off.',
  eventsPickNote: 'Pick a sound and press play to hear it.',

  // --- files ---
  filesTitle: 'Sound files',
  filesHelp: 'Everything in this game’s audio folder.',
  filesNone: 'No sound files yet.',
  fileUsedBy: (names: string[]) => `Used by ${names.join(', ')}`,
  fileUnused: 'Not used yet',
  fileLength: (text: string) => text,
  fileRate: (rate: number, channels: number) => `${rate} Hz, ${channels === 1 ? 'mono' : channels === 2 ? 'stereo' : `${channels} channels`}`,
  unreadable: 'Unreadable',
  useAsMusic: 'Use as music',
  useAsSound: 'Use as a sound',
  deleteFile: 'Delete file',
  deleteFileLabel: (file: string) => `Delete the file ${file}`,
  deleteFileTitle: (file: string) => `Delete ${file}?`,
  deleteFileUnused: 'The file is deleted from the game folder for good. Undo does not bring it back.',
  deleteFileUsed: 'It is used here. Deleting it also takes these out of the game (Undo brings them back, but not the file):',
  fileDeleted: (file: string) => `${file} was deleted.`,
  fileStillUsed: (file: string, by: string) => `${file} is still used by ${by} in the saved game. Take it out of there first.`,
  fileDeleteFailed: (file: string, why: string) => `${file} was not deleted: ${why}`,
  deleteConfirm: 'Delete',

  // --- remove everything ---
  removeAllTitle: 'Remove all sound',
  removeAllHelp: 'Takes every track, sound and setting out of this game. The files stay in the audio folder.',
  removeAllButton: 'Remove all sound…',
  removeAllWhat: 'all the sound in this game',

  // --- checks ---
  checksTitle: 'Things to check in the sound',
  checksHelp: 'The game still saves; fix these before you play.',
  hintsTitle: 'Worth a look',

  // --- the small groups on other screens ---
  soundsGroup: 'Sounds',
  soundsGroupHelp: 'Leave on “Same as the game” to use the standard sound. “Silent” plays nothing here.',
  sameAsGame: 'Same as the game',
  silent: 'Silent',
  mapMusicGroup: 'Music',
  mapMusicHelp: 'Leave on “Same as the game” to use the game’s music. “No music” keeps this map quiet.',
  noMusicOption: 'No music',
  sceneMusic: 'Music for this scene',
  sceneMusicHelp: 'Starts when the scene starts and goes on afterwards.',
  keepPlaying: 'Keep what is playing',
  missing: (id: string) => `${id} (not there any more)`,
} as const
