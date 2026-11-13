// Just enough PNG decoding (8-bit RGB/RGBA, not interlaced) to look at pixels of Playwright screenshots
// without adding a dependency. The player draws with WebGL, so a screenshot is the reliable way to see it.
import { inflateSync } from 'node:zlib'

export interface Image { width: number; height: number; data: Uint8Array /* RGBA */ }

export function decodePng(buf: Buffer): Image {
  let pos = 8
  let width = 0, height = 0, channels = 4
  const idat: Buffer[] = []
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos)
    const type = buf.toString('latin1', pos + 4, pos + 8)
    const body = buf.subarray(pos + 8, pos + 8 + len)
    if (type === 'IHDR') {
      width = body.readUInt32BE(0); height = body.readUInt32BE(4)
      if (body[8] !== 8 || body[12] !== 0) throw new Error('unsupported PNG (need 8-bit, not interlaced)')
      channels = body[9] === 6 ? 4 : body[9] === 2 ? 3 : 0
      if (!channels) throw new Error(`unsupported PNG colour type ${body[9]}`)
    } else if (type === 'IDAT') idat.push(body)
    pos += 12 + len
  }
  const raw = inflateSync(Buffer.concat(idat))
  const stride = width * channels
  const out = new Uint8Array(width * height * 4)
  const prev = new Uint8Array(stride)
  const cur = new Uint8Array(stride)
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (stride + 1)]
    const line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1))
    for (let i = 0; i < stride; i++) {
      const a = i >= channels ? cur[i - channels] : 0
      const b = prev[i]
      const c = i >= channels ? prev[i - channels] : 0
      let v = line[i]
      if (filter === 1) v += a
      else if (filter === 2) v += b
      else if (filter === 3) v += (a + b) >> 1
      else if (filter === 4) {
        const p = a + b - c
        const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c)
        v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c
      }
      cur[i] = v & 255
    }
    for (let x = 0; x < width; x++) {
      const o = (y * width + x) * 4
      out[o] = cur[x * channels]; out[o + 1] = cur[x * channels + 1]; out[o + 2] = cur[x * channels + 2]
      out[o + 3] = channels === 4 ? cur[x * channels + 3] : 255
    }
    prev.set(cur)
  }
  return { width, height, data: out }
}

/** Number of pixels within `tol` (per channel) of the colour. */
export function countColor(img: Image, rgb: [number, number, number], tol = 10): number {
  let n = 0
  for (let i = 0; i < img.data.length; i += 4) {
    if (Math.abs(img.data[i] - rgb[0]) <= tol && Math.abs(img.data[i + 1] - rgb[1]) <= tol && Math.abs(img.data[i + 2] - rgb[2]) <= tol) n++
  }
  return n
}

export function distinctColors(img: Image, limit = 64): number {
  const seen = new Set<number>()
  for (let i = 0; i < img.data.length && seen.size < limit; i += 4) seen.add((img.data[i] << 16) | (img.data[i + 1] << 8) | img.data[i + 2])
  return seen.size
}

/** Pixels that differ noticeably between two same-sized images. */
export function diffCount(a: Image, b: Image, tol = 40): number {
  if (a.width !== b.width || a.height !== b.height) return a.width * a.height
  let n = 0
  for (let i = 0; i < a.data.length; i += 4) {
    if (Math.abs(a.data[i] - b.data[i]) + Math.abs(a.data[i + 1] - b.data[i + 1]) + Math.abs(a.data[i + 2] - b.data[i + 2]) > tol) n++
  }
  return n
}
