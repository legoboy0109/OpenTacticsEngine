// Helpers for the art tests: PNGs made on the fly (no files in the repo), driving the Art screen and the picker like a
// person would, and reading pixels from the canvases.
import { deflateSync } from 'node:zlib'
import type { APIRequestContext, Locator, Page } from '@playwright/test'
import { editorUrl, expect, saved } from './fixtures'

export type Rgba = [number, number, number, number]

const crcTable = (() => {
  const t: number[] = []
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t.push(c >>> 0) }
  return t
})()
const crc = (b: Buffer) => { let c = 0xffffffff; for (const x of b) c = crcTable[(c ^ x) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0 }
function chunk(type: string, data: Buffer): Buffer {
  const out = Buffer.alloc(12 + data.length)
  out.writeUInt32BE(data.length, 0); out.write(type, 4, 'latin1'); data.copy(out, 8)
  out.writeUInt32BE(crc(out.subarray(4, 8 + data.length)), 8 + data.length)
  return out
}

/** An 8-bit RGBA PNG; `px(x, y)` gives each pixel. */
export function makePng(width: number, height: number, px: (x: number, y: number) => Rgba): Buffer {
  const raw = Buffer.alloc((width * 4 + 1) * height)
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) raw.set(px(x, y), y * (width * 4 + 1) + 1 + x * 4)
  }
  const ihdr = Buffer.alloc(13)
  ihdr.writeUInt32BE(width, 0); ihdr.writeUInt32BE(height, 4); ihdr[8] = 8; ihdr[9] = 6
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))])
}

/** Distinct, easy-to-tell colour for piece (col, row). */
export const pieceColor = (col: number, row: number): Rgba => [(col * 53 + 40) % 256, (row * 71 + 90) % 256, ((col + row) * 37 + 20) % 256, 255]

/** A sheet of `cols` x `rows` pieces of `cell` pixels, each piece one flat colour (pieceColor). */
export const sheetPng = (cols: number, rows: number, cell = 24): Buffer =>
  makePng(cols * cell, rows * cell, (x, y) => pieceColor(Math.floor(x / cell), Math.floor(y / cell)))

export const solidPng = (w: number, h: number, c: Rgba): Buffer => makePng(w, h, () => c)

export const png = (name: string, buffer: Buffer) => ({ name, mimeType: 'image/png', buffer })

export async function openArt(page: Page, game: string, item?: string): Promise<void> {
  await page.goto(editorUrl(game, 'art', item))
  await expect(page.getByRole('heading', { name: 'Art', level: 1 })).toBeVisible()
}

/** Adds images through the file chooser of the Art screen. */
export async function addImages(page: Page, files: Array<{ name: string; mimeType: string; buffer: Buffer }>): Promise<void> {
  await page.getByTestId('add-image-input').setInputFiles(files)
}

/** Adds one image and waits until it is in the list and saved. */
export async function addImage(page: Page, name: string, buffer: Buffer, listName?: string): Promise<void> {
  await addImages(page, [png(name, buffer)])
  await expect(page.getByRole('list', { name: 'Art' }).getByRole('button', { name: listName ?? name.replace(/\.png$/, '') })).toBeVisible()
  await saved(page)
}

export const imageList = (page: Page) => page.getByRole('list', { name: 'Art' })

/** Page coordinates of the middle of piece (col, row) on a piece grid (the picker's or the Art screen's). */
export async function piecePoint(grid: Locator, col: number, row: number): Promise<{ x: number; y: number }> {
  await grid.scrollIntoViewIfNeeded()
  const box = (await grid.boundingBox())!
  const zoom = await steadyZoom(grid)
  const [cw, ch] = (await grid.getAttribute('data-cell'))!.split(',').map(Number)
  return { x: box.x + (col + 0.5) * cw * zoom, y: box.y + (row + 0.5) * ch * zoom }
}

/** The grid fits itself to its box a moment after a dialog opens: wait until the zoom has stopped changing. */
async function steadyZoom(grid: Locator): Promise<number> {
  let last = -1
  for (let i = 0; i < 30; i++) {
    const z = Number(await grid.getAttribute('data-zoom'))
    if (z === last) return z
    last = z
    await grid.page().waitForTimeout(120)
  }
  return last
}

export async function clickPiece(_page: Page, grid: Locator, col: number, row: number): Promise<void> {
  const zoom = await steadyZoom(grid)
  const [cw, ch] = (await grid.getAttribute('data-cell'))!.split(',').map(Number)
  // Locator.click scrolls the point into view (the picker's grid scrolls inside the dialog).
  await grid.click({ position: { x: (col + 0.5) * cw * zoom, y: (row + 0.5) * ch * zoom } })
}

export const picker = (page: Page) => page.getByRole('dialog', { name: /^Choose a picture for/ })
export const pickerGrid = (page: Page) => picker(page).getByTestId('piece-grid')

/** Reads one device pixel of a canvas (page coordinates are not needed: canvas coordinates). */
export async function canvasPixel(canvas: Locator, x: number, y: number): Promise<Rgba> {
  return canvas.evaluate((el, [px, py]) => {
    const c = el as HTMLCanvasElement
    const d = c.getContext('2d')!.getImageData(px, py, 1, 1).data
    return [d[0], d[1], d[2], d[3]] as [number, number, number, number]
  }, [x, y] as [number, number])
}

export const near = (a: Rgba | number[], b: Rgba | number[], tol = 6) => a.slice(0, 3).every((v, i) => Math.abs(v - b[i]) <= tol)

// ---- seeding through the API (fast set-up for tests that are not about adding images) ----

/** Puts PNG files into art/ and the given graphics section into the project, like another program would. The starter art
 *  that templates now carry is wiped first, so a test sees exactly the art it seeds (nothing else in art/, no stray images). */
export async function seedArt(request: APIRequestContext, id: string, files: Record<string, Buffer>, graphics: object): Promise<void> {
  let doc = await (await request.get(`/api/projects/${id}`)).json()
  if (doc.graphics) {
    expect((await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics: { sheets: [] } } })).ok()).toBeTruthy()
  }
  for (const f of (await (await request.get(`/api/projects/${id}/art`)).json()).art) {
    expect((await request.delete(`/api/projects/${id}/${f.file}`)).ok(), f.file).toBeTruthy()
  }
  for (const [name, buffer] of Object.entries(files)) {
    const res = await request.put(`/api/projects/${id}/art/${name}.png`, { data: buffer, headers: { 'content-type': 'image/png' } })
    expect(res.status(), await res.text()).toBe(201)
  }
  doc = await (await request.get(`/api/projects/${id}`)).json()
  const res = await request.put(`/api/projects/${id}`, { data: { baseRevision: doc.revision, graphics } })
  expect(res.ok(), await res.text()).toBeTruthy()
}

/** A game from a template with no art at all (art section empty, nothing in art/), for tests that start from scratch. */
export async function bareGame(request: APIRequestContext, game: { create(template: string, name?: string): Promise<string> }, template: string, name: string): Promise<string> {
  const id = await game.create(template, name)
  await seedArt(request, id, {}, { sheets: [] })
  return id
}

/** One image "tiles" (art/tiles.png, square pieces of `cell` pixels) plus optional extra graphics keys. */
export const tilesGraphics = (extra: object = {}, cell = 24) => ({
  sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: cell, cellHeight: cell, name: 'Tiles' }], ...extra,
})

// ---- the map canvas ----

/** Reads the colour of the map canvas at a point inside square (cx, cy); fx, fy are fractions of the square (0..1). */
export async function mapPixel(page: Page, cx: number, cy: number, fx = 0.5, fy = 0.5): Promise<Rgba> {
  const c = page.getByTestId('map-canvas')
  await page.evaluate(() => new Promise<void>((r) => requestAnimationFrame(() => requestAnimationFrame(() => r()))))
  const [scale, vx, vy] = (await c.getAttribute('data-view'))!.split(',').map(Number)
  return canvasPixel(c, Math.round(vx + (cx + fx) * scale), Math.round(vy + (cy + fy) * scale))
}

export const hex = (c: Rgba | number[]): string => `#${c.slice(0, 3).map((v) => v.toString(16).padStart(2, '0')).join('')}`
