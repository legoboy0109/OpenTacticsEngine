// Wording of the "Standing part" section of a terrain's Picture card (see StandEditor.tsx).
export const S = {
  title: 'Standing part',
  help: 'In 3D styles a terrain can have something standing on its ground, like trees or a peak. The ground picture stays underneath.',
  needGround: 'First give this terrain a picture of its own in this style. The standing part stands on it.',

  group: 'What stands here',
  none: 'Nothing',
  picture: 'An upright picture',
  model: 'A 3D model',

  pictureHelp: 'The picture always faces the camera. Choose several pieces as a random mix so a forest is not a row of identical trees; each square always shows the same one.',
  choosePicture: 'Choose picture…',
  changePicture: 'Change picture…',
  noPicture: 'No picture chosen yet.',
  pickerWhat: (n: string) => `${n}, standing part`,
  width: 'Picture width',
  widthUnit: 'pixels',
  widthHelp: 'How wide it is drawn, in pixels of the game’s tile size. Empty is one square wide.',

  models: 'Model files',
  modelsHelp: 'Each square always uses the same one of these, picked by its position. Add several so a forest is not a row of identical trees.',
  noModels: 'No model chosen yet.',
  addModel: 'Add a model file…',
  remove: 'Remove',
  removeModel: (n: string) => `Remove ${n}`,
  fileMissing: (name: string) => `${name} (file not found)`,
  upload: 'Upload a model…',
  uploading: 'Uploading…',
  height: 'Model height',
  heightUnit: 'squares',
  heightHelp: 'How tall it stands, in squares. Empty means 1.',
  turn: 'Model turn',
  turnUnit: 'degrees',
  turnHelp: 'Added to the way it faces. Empty means 0.',

  scatter: 'Vary position and turn',
  scatterHelp: 'Every square gets a small fixed shift, and a model its own turn, so a forest looks less regular.',
}
