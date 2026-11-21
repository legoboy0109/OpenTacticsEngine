// The Play panel frame's whole-number fit (src/play/wholeFit.ts): the arithmetic, in device pixels.
import { describe, expect, it } from 'vitest'
import { cssFor, dialogFit, fitStyle, viewSize, wholeFit } from '../../src/play/wholeFit'

const V = { width: 480, height: 270 }

describe('wholeFit', () => {
  it('takes the largest whole multiple that fits and centres it', () => {
    expect(wholeFit(V, 1177, 662)).toEqual({ k: 2, w: 960, h: 540, x: 108, y: 61 })
    expect(wholeFit(V, 2560, 1401)).toEqual({ k: 5, w: 2400, h: 1350, x: 80, y: 25 })
    expect(wholeFit(V, 1920, 1080)).toEqual({ k: 4, w: 1920, h: 1080, x: 0, y: 0 })
  })
  it('is null when even 1x does not fit (the frame then shrinks to fit, as before)', () => {
    expect(wholeFit(V, 479, 270)).toBeNull()
    expect(wholeFit(V, 2000, 269)).toBeNull()
    expect(wholeFit(V, 0, 0)).toBeNull()
    expect(wholeFit({ width: 0, height: 270 }, 1000, 1000)).toBeNull()
  })
  it('lets the tighter axis decide and floors an odd offset', () => {
    expect(wholeFit(V, 1000, 3000)).toMatchObject({ k: 2, x: 20, y: 1230 })
    expect(wholeFit({ width: 320, height: 240 }, 1000, 700)).toMatchObject({ k: 2, w: 640, h: 480, x: 180, y: 110 })
    expect(wholeFit(V, 481, 271)).toMatchObject({ k: 1, x: 0, y: 0 })
  })
  it('sizes the frame in css pixels so one canvas pixel is one device pixel', () => {
    const fit = wholeFit(V, 1250, 750) // 1000 x 600 css px at 1.25
    expect(fit).toMatchObject({ k: 2, w: 960, h: 540 })
    expect(fitStyle(fit, 1.25)).toEqual({ inset: 'auto', left: `${145 / 1.25}px`, top: `${105 / 1.25}px`, width: '768px', height: '432px' })
    expect(fitStyle(null, 1)).toBeUndefined()
  })
  it('uses whole css pixels, exact in device pixels where some whole size is (a frame is laid out in whole css pixels)', () => {
    expect(cssFor(1920, 1.75)).toBe(1097) // 1097.14 ideal: 1097 x 1.75 = 1919.75 rounds to 1920
    expect(cssFor(960, 1.1)).toBe(873)    // 872.7 ideal
    expect(cssFor(960, 1)).toBe(960)
    expect(cssFor(960, 2)).toBe(480)
    expect(Math.round(cssFor(1080, 1.75) * 1.75)).toBe(1080)
    // never short: a canvas below k x view would show the picture at (k-1)x
    for (const dpr of [1, 1.1, 1.25, 1.3, 1.5, 1.75, 2, 2.5, 3]) for (const dev of [480, 960, 1440, 1920, 2400, 270, 540, 1350])
      expect(Math.round(cssFor(dev, dpr) * dpr)).toBeGreaterThanOrEqual(dev)
  })
})

describe('dialogFit', () => {
  const chrome = { borderW: 2, borderH: 2, head: 90, foot: 34 }
  it('sizes the dialog from the window: frame = the largest whole multiple, dialog = frame + chrome', () => {
    const l = dialogFit(V, { w: 2560, h: 1401 }, chrome, 1)!
    expect(l.frame).toMatchObject({ width: '1920px', height: '1080px' })
    expect(l.dialog).toMatchObject({ width: '1922px', height: 'fit-content' })
    expect(l.stage).toMatchObject({ height: '1080px' })
    expect(dialogFit(V, { w: 1920, h: 1080 }, chrome, 1)!.frame).toMatchObject({ width: '1440px' })
    expect(dialogFit(V, { w: 1280, h: 900 }, chrome, 1)!.frame).toMatchObject({ width: '960px' })
  })
  it('gives 3x on a 1080p screen\'s browser window (about 900 px tall) with the slim one-row chrome', () => {
    const slim = { borderW: 2, borderH: 2, head: 46, foot: 22 }
    expect(dialogFit(V, { w: 1920, h: 900 }, slim, 1)!.frame).toMatchObject({ width: '1440px', height: '810px' })
    expect(dialogFit(V, { w: 1536, h: 730 }, slim, 1.25)!.frame).toMatchObject({ width: '1152px', height: '648px' })  // 3x of 480x270 in device pixels
  })
  it('keeps a narrow frame on a dialog wide enough for the header, and gives up below 1x', () => {
    expect(dialogFit(V, { w: 760, h: 700 }, chrome, 1)!.dialog).toMatchObject({ width: '602px' })
    expect(dialogFit(V, { w: 800, h: 300 }, chrome, 1)).toBeNull()
  })
})

describe('viewSize', () => {
  it('takes game.json, falling back to the engine default per part', () => {
    expect(viewSize({ width: 320, height: 240 })).toEqual({ width: 320, height: 240 })
    expect(viewSize({ width: 640 })).toEqual({ width: 640, height: 270 })
    expect(viewSize(undefined)).toEqual({ width: 480, height: 270 })
    expect(viewSize({ width: -5, height: 0 })).toEqual({ width: 480, height: 270 })
  })
})
