// Wording of the notice, loop, name-entry and portrait-face controls (docs/EVENTS.md "Speech bubbles, notices, names, faces and loops").
// ALL of it is placeholder wording for the owner to review.
export const DX = {
  kinds: {
    notice: 'Show a notice',
    loop: 'Repeat these commands',
    break: 'Leave the repeat',
    askName: 'Ask for a character’s name',
  },
  // A line's and a question's portrait face.
  face: 'Face (expression)',
  faceHelp: 'Which of the speaker’s portrait faces to show (1 = the first of its “Expressions” in Art, Portraits). Empty = the portrait’s own picture.',
  facePlaceholder: 'The portrait’s own picture',

  // A notice (a system message in its own window).
  noticeText: 'What the notice says',
  noticeTextHelp: 'A message in a window of its own in the middle of the screen: no name, no portrait. {name:id} shows the name a player typed for that character.',

  // A question's notice / cursor.
  choiceNotice: 'Show it as a notice',
  choiceNoticeHelp: 'It is asked, with its answers, in the notice window instead of beside a speaker. A notice question has no speaker and no portrait.',
  choiceCursor: 'Starts on answer',
  choiceCursorHelp: 'Which answer is highlighted first (1 = the first answer as written). Empty = the first one. When nobody is asked (the simulator, Play on autopilot) this is the answer that is taken.',
  choiceCursorPlaceholder: 'The first answer',

  // A loop.
  loopHelp: 'The commands inside run again and again. A “Leave the repeat” command inside (also inside an If or an answer) ends it at once; without one the commands run the most rounds below and then the story goes on. Add the commands in the list under this one.',
  loopMax: 'Most rounds',
  loopMaxHelp: 'A safety limit so a story can never hang (1 to 100, usually 10).',
  loopEmpty: 'Nothing is inside this repeat yet: add commands to the list under it.',
  loopPart: 'Repeated',
  breakHelp: 'Ends the repeat this command is in, and the story goes on after it. It only works inside a “Repeat these commands”.',

  // Asking for a name (campaign scenes only).
  nameWho: 'Whose name',
  nameWhoHelp: 'The character the player names. The typed name is used everywhere that character’s name is shown, and {name:id} in any text shows it.',
  nameDefault: 'Starting name',
  nameDefaultHelp: 'The name the entry starts with, and the one kept when nobody types (the simulator). Empty = the character’s own name.',
  nameMax: 'Most letters',
  nameMaxHelp: '1 to 24 (usually 12).',
  namePrompt: 'Words above the name',
  namePromptHelp: 'What the name entry says, such as “Enter your name.” Empty = the game’s usual words.',
  namePromptPlaceholder: 'The usual words',
}
