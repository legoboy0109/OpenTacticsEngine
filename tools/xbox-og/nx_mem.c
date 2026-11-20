// Fast memcpy / memmove / memset for the original Xbox build (docs/XBOX_OG.md "Speed").
//
// nxdk's C library (PDCLib) copies and fills one BYTE per loop turn. Everything in the player goes through these: vectors,
// SDL's blits, the 2D layer's copy into the graphics chip's memory (1.2 MB a frame in the first version took 6.8 ms in xemu), the
// 3D meshes. The x86 string instructions move four bytes a step and are what the CPU is best at.
//
// This object defines the three symbols itself; the linker takes them before the library's (PDCLib keeps each function in its own
// object, which is then never pulled in). The bodies are string instructions, not loops, so the compiler cannot turn them back
// into calls to these same functions.
#include <stddef.h>

void* memcpy(void* restrict dst, const void* restrict src, size_t n) {
  void* const ret = dst;
  size_t words = n >> 2;
  __asm__ volatile("rep movsl" : "+D"(dst), "+S"(src), "+c"(words) : : "memory");
  size_t tail = n & 3;
  __asm__ volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(tail) : : "memory");
  return ret;
}

void* memmove(void* dst, const void* src, size_t n) {
  if ((char*)dst <= (const char*)src || (char*)dst >= (const char*)src + n) return memcpy(dst, src, n);
  // Overlapping with the destination above the source: copy from the end, byte by byte (rare, and short in practice; the string
  // instruction, not a loop, so that the compiler cannot turn it into a call to memmove itself).
  char* d = (char*)dst + n - 1;
  const char* s = (const char*)src + n - 1;
  size_t count = n;
  if (n) __asm__ volatile("std\n\trep movsb\n\tcld" : "+D"(d), "+S"(s), "+c"(count) : : "memory", "cc");
  return dst;
}

void* memset(void* dst, int c, size_t n) {
  void* const ret = dst;
  const unsigned int b = (unsigned char)c;
  unsigned int word = b | (b << 8) | (b << 16) | (b << 24);
  size_t words = n >> 2;
  __asm__ volatile("rep stosl" : "+D"(dst), "+c"(words) : "a"(word) : "memory");
  size_t tail = n & 3;
  __asm__ volatile("rep stosb" : "+D"(dst), "+c"(tail) : "a"(word) : "memory");
  return ret;
}
