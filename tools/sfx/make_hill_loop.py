#!/usr/bin/env python3
"""
make_hill_loop.py - the hill's combat music, from assets/music/misanthropy.mp3

The mp3 doesn't loop cleanly on its own: it has the encoder's silence at the
front, and its last few hundred milliseconds are already the start of the
next time round, so playing it end to end gives a hiccup at every loop.

This finds where the music really repeats (the point near the end that
matches the opening most closely), cuts exactly one cycle out, and fades
the overhang back into the start over 30 ms so the seam can't be heard. It
also brings the level up to -14 LUFS, so the track sits as loud in the mix
as our own combat music does against gunfire and voices. Then it writes an
Ogg Vorbis file, which loops sample-accurately where an mp3 can't.

    python3 tools/sfx/make_hill_loop.py      # writes assets/mudcats/music_hill_loop.ogg

Needs ffmpeg on the path, with libvorbis.
"""

import os
import subprocess
import sys
import tempfile

import numpy as np
from scipy.io import wavfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
SOURCE = os.path.join(ROOT, "assets", "music", "misanthropy.mp3")
OUT = os.path.join(ROOT, "assets", "mudcats", "music_hill_loop.ogg")
TARGET_LUFS = -14.0
FADE = 0.03


def normalised_correlation(a, b):
    a = a - a.mean()
    b = b - b.mean()
    return float(np.dot(a, b) / np.sqrt(np.dot(a, a) * np.dot(b, b) + 1e-12))


def find_cycle(mono, sr):
    """The start of the music, and the length after which it repeats."""
    window = 400
    tail_start = len(mono) - int(0.25 * sr)
    tail = mono[tail_start:]
    ones = np.ones(window)
    sums = np.convolve(tail, ones, "valid")
    squares = np.convolve(tail ** 2, ones, "valid")
    variance = np.maximum(squares - sums ** 2 / window, 1e-12)
    best = (-1.0, 0, 0)
    for start in range(0, int(0.04 * sr), 10):
        a = mono[start:start + window]
        a = a - a.mean()
        if np.std(a) < 1e-4:
            continue
        c = np.correlate(tail, a, "valid") / np.sqrt(variance * np.dot(a, a))
        k = int(np.argmax(c))
        if c[k] > best[0]:
            best = (float(c[k]), start, tail_start + k - start)
    score, start, length = best
    print(f"  music starts at {start / sr * 1000:.1f} ms, repeats after {length / sr:.4f} s (match {score:.3f})")
    return start, length


def loudness(path):
    out = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", path, "-af", "ebur128", "-f", "null", "-"],
                         capture_output=True, text=True).stderr
    summary = out[out.rfind("Summary"):]
    for line in summary.splitlines():
        if line.strip().startswith("I:"):
            return float(line.split()[1])
    raise RuntimeError("ffmpeg gave no loudness")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        raw = os.path.join(tmp, "raw.wav")
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", SOURCE, "-ac", "2", "-ar", "44100", raw], check=True)
        sr, x = wavfile.read(raw)
        x = x.astype(np.float64) / 32768.0

        start, length = find_cycle(x.mean(axis=1), sr)
        fade = int(FADE * sr)
        if start + length + fade > len(x):
            sys.exit("the repeat runs off the end of the file; nothing to fade with")
        loop = x[start:start + length].copy()
        k = (np.arange(fade) / fade)[:, None]
        loop[:fade] = x[start:start + fade] * np.sin(k * np.pi / 2) + x[start + length:start + length + fade] * np.cos(k * np.pi / 2)

        gain_db = TARGET_LUFS - loudness(raw)
        loop *= 10 ** (gain_db / 20)
        peak = 20 * np.log10(np.abs(loop).max() + 1e-12)
        print(f"  gain {gain_db:+.1f} dB, peak {peak:.1f} dBFS")
        if peak > -1.0:
            loop *= 10 ** ((-1.0 - peak) / 20)

        mid = os.path.join(tmp, "loop.wav")
        wavfile.write(mid, sr, (np.clip(loop, -1, 1) * 32767).astype(np.int16))
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", mid, "-c:a", "libvorbis", "-q:a", "6", OUT], check=True)
    print(f"  wrote {os.path.relpath(OUT, ROOT)}")


if __name__ == "__main__":
    main()
