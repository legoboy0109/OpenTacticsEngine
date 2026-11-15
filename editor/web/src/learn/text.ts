// All wording of the Learn feature except the lessons themselves (those are in lessons/*.ts).
export const T = {
  // Top bar and game list
  learnButton: 'Learn',
  learnTitle: 'Short lessons that teach the editor',

  // The lesson list
  listTitle: 'Learn',
  listIntro: 'Short lessons where you do each step yourself. Every lesson makes its own practice game, so your own games are never touched.',
  minutes: (n: number) => `About ${n} minutes`,
  statusNew: 'Not started',
  statusProgress: (step: number, total: number) => `In progress: step ${step} of ${total}`,
  statusFinished: 'Finished',
  start: 'Start',
  continue: 'Continue',
  startAgain: 'Start again',
  close: 'Close',
  starting: 'Making your practice game…',
  startFailed: (why: string) => `The practice game could not be made. ${why}`,
  goneInList: (title: string) => `The practice game for “${title}” is gone. Start again to make a new one.`,

  // First-run card on the game list
  offerTitle: 'New here? Make your first game with a short lesson.',
  offerBody: 'It takes about 10 minutes. You make a small practice game one step at a time, and the editor shows you where to click.',
  offerStart: 'Start',
  offerLater: 'Not now',
  offerHelp: 'You can find the lessons any time under Learn.',

  // The lesson panel
  panelLabel: 'Lesson',
  stepOf: (step: number, total: number) => `Step ${step} of ${total}`,
  progressLabel: (done: number, total: number) => `${done} of ${total} steps done`,
  hide: 'Hide lesson',
  show: 'Show lesson',
  barShow: 'Show',
  doneSoFar: (n: number) => `Done so far (${n})`,
  skippedMark: '(skipped)',
  happensOn: (screen: string) => `This happens on the ${screen} screen.`,
  elsewhere: 'This lesson happens in your practice game.',
  takeMeThere: 'Take me there',
  skipStep: 'Skip this step',
  stopLesson: 'Stop lesson',
  goneTitle: 'Your practice game is gone',
  goneBody: 'It looks like the practice game was deleted. You can make a new one and start the lesson again.',

  // The end of a lesson
  endTitle: 'You finished the lesson',
  endMade: 'What you made',
  endPlay: 'Play your game',
  endOpen: 'Open your practice game',
  endNext: 'Next lessons',
  endDone: 'Done',
} as const
