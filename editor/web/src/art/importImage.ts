// Getting an image ready for upload: any picture the browser can open becomes a PNG (via a canvas); PNGs go up
// untouched. The 2048 x 2048 and 8 MB limits are checked here first so the person gets a friendly message
// before anything is sent (the helper checks again).
import { MAX_IMAGE_BYTES, pngSize, tooBig } from '../model/artFiles'
import { T } from '../screens/art/text'

export type Prepared =
  | { ok: true; blob: Blob; width: number; height: number; converted: boolean }
  | { ok: false; message: string }

const mb = (n: number) => (n / (1024 * 1024)).toFixed(1)

async function decode(file: Blob): Promise<{ source: CanvasImageSource; width: number; height: number; close: () => void } | null> {
  try {
    const bmp = await createImageBitmap(file)
    return { source: bmp, width: bmp.width, height: bmp.height, close: () => bmp.close() }
  } catch { /* some formats (SVG) only open through an <img> */ }
  const url = URL.createObjectURL(file)
  try {
    const img = new Image()
    img.src = url
    await img.decode()
    if (!img.naturalWidth || !img.naturalHeight) return null
    return { source: img, width: img.naturalWidth, height: img.naturalHeight, close: () => URL.revokeObjectURL(url) }
  } catch {
    URL.revokeObjectURL(url)
    return null
  }
}

export async function prepareImage(file: File): Promise<Prepared> {
  const head = new Uint8Array(await file.slice(0, 32).arrayBuffer())
  const png = pngSize(head)
  if (png) {
    if (tooBig(png.width, png.height)) return { ok: false, message: T.tooLarge(png.width, png.height) }
    if (file.size > MAX_IMAGE_BYTES) return { ok: false, message: T.tooHeavy(mb(file.size)) }
    return { ok: true, blob: file, width: png.width, height: png.height, converted: false }
  }
  const img = await decode(file)
  if (!img) return { ok: false, message: T.cannotOpen(file.name) }
  try {
    if (tooBig(img.width, img.height)) return { ok: false, message: T.tooLarge(img.width, img.height) }
    const c = document.createElement('canvas')
    c.width = img.width; c.height = img.height
    const g = c.getContext('2d')
    if (!g) return { ok: false, message: T.cannotConvert(file.name) }
    g.imageSmoothingEnabled = false
    g.drawImage(img.source, 0, 0)
    const blob = await new Promise<Blob | null>((r) => c.toBlob(r, 'image/png'))
    if (!blob) return { ok: false, message: T.cannotConvert(file.name) }
    if (blob.size > MAX_IMAGE_BYTES) return { ok: false, message: T.tooHeavy(mb(blob.size)) }
    return { ok: true, blob, width: img.width, height: img.height, converted: true }
  } finally {
    img.close()
  }
}
