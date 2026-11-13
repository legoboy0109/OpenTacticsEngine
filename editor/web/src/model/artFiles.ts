// File-level helpers for images: names on disk, PNG header reading, limits. Pure (no browser APIs).
import { slugify, uniqueId } from './ids'

export const MAX_IMAGE_SIZE = 2048
export const MAX_IMAGE_BYTES = 8 * 1024 * 1024

/** "art/tiles.png" -> "tiles". */
export const artBase = (file: string): string => file.replace(/^art\//, '').replace(/\.png$/i, '')
export const artPath = (base: string): string => `art/${base}.png`

const stemOf = (originalName: string): string => originalName.replace(/^.*[\\/]/, '').replace(/\.[A-Za-z0-9]{1,5}$/, '')

/** Name on disk for an uploaded file: slug of the original name (without extension), unique among `existing` file paths. */
export function artNameFor(originalName: string, existing: Iterable<string>): string {
  return uniqueId(slugify(stemOf(originalName), { sep: '_', fallback: 'image', max: 40 }), [...existing].map(artBase), '_')
}

/** The name people see for a new image: the original file name without its extension. */
export const displayStem = (originalName: string): string => stemOf(originalName).trim()

const SIG = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]
/** Width and height from a PNG header, or null when the bytes are not a PNG. */
export function pngSize(bytes: Uint8Array): { width: number; height: number } | null {
  if (bytes.length < 24 || SIG.some((b, i) => bytes[i] !== b)) return null
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
  if (v.getUint32(12) !== 0x49484452) return null // "IHDR"
  const width = v.getUint32(16), height = v.getUint32(20)
  return width > 0 && height > 0 ? { width, height } : null
}

export const tooBig = (width: number, height: number): boolean => width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE
