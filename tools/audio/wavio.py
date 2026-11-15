"""Minimal 16-bit PCM WAV reading and writing (standard library only)."""
import struct
import wave


def write_wav(path, samples, rate):
    """Writes mono float samples (-1..1, already limited) as 16-bit PCM."""
    pcm = struct.pack('<%dh' % len(samples),
                      *[max(-32767, min(32767, int(round(s * 32767)))) for s in samples])
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)


def read_wav(path):
    """Returns (rate, channels, [int samples, interleaved])."""
    with wave.open(path, 'rb') as w:
        n = w.getnframes() * w.getnchannels()
        data = struct.unpack('<%dh' % n, w.readframes(w.getnframes()))
        return w.getframerate(), w.getnchannels(), list(data)
