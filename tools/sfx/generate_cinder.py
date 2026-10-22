#!/usr/bin/env python3
"""
generate_cinder.py - sounds and music for the Cinder example

Built from the same pieces as generate_sfx.py, so everything is synthesised
and yours to use.

    python3 tools/sfx/generate_cinder.py           # writes assets/cinder/*.wav

Files ending in _loop loop seamlessly.
"""

import argparse
import os
import sys

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_sfx import (SR, t_axis, env, noise, sine, square, triangle, lowpass, highpass,  # noqa: E402
                          bandpass, sweep_lowpass, crush, echo, mix, place, place_wrapped,
                          finish, periodic)

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "cinder")


def note(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def crackles(rng, seconds, per_second, loudness=(0.2, 1.0), wrap=False):
    """Fire pops: tiny bright clicks at random times."""
    n = int(seconds * SR)
    out = np.zeros(n)
    for _ in range(int(seconds * per_second)):
        size = int(rng.uniform(0.002, 0.007) * SR)
        shape = env(size / SR, 0.0002, size / SR / 4)
        pop = bandpass(noise(len(shape), rng), 1500, 9000) * shape
        at = rng.uniform(0, seconds - (0 if wrap else size / SR))
        (place_wrapped if wrap else place)(out, pop, at, gain=rng.uniform(*loudness))
    return out


# ----------------------------------------------------------------- one-shots

def ignite(seed=1):
    rng = np.random.default_rng(seed)
    length = 1.6
    t = t_axis(length)
    n = len(t)
    # Air rushing in and up, the flame catching.
    whoosh = sweep_lowpass(noise(n, rng), 300, 5000) * np.clip(t / 0.25, 0, 1) * env(length, 0.0, 0.35)
    thump = sine(90 * np.exp(-t / 0.06) + 45) * env(length, 0.005, 0.12) * 0.9
    roar = lowpass(noise(n, rng), 600) * env(length, 0.05, 0.5) * 0.6
    pops = crackles(rng, length, 30) * np.clip(1.4 - t, 0, 1)
    return finish(mix(whoosh * 0.9, thump, roar, pops * 0.6))


def ember_pickup(seed=2):
    rng = np.random.default_rng(seed)
    length = 0.6
    out = np.zeros(int(length * SR))
    for midi, at in [(84, 0.0), (91, 0.06)]:
        d = length - at
        f = np.full(int(d * SR), note(midi))
        tone = 0.7 * triangle(f) + 0.15 * sine(f * 2)
        place(out, tone * env(d, 0.002, 0.12), at)
    shimmer = highpass(noise(len(out), rng), 6000) * env(length, 0.001, 0.05) * 0.15
    return finish(crush(out + shimmer, bits=9, hold_rate=30000), peak_db=-4.0)


def dash(seed=3):
    rng = np.random.default_rng(seed)
    length = 0.4
    t = t_axis(length)
    shape = np.sin(np.pi * np.clip(t / 0.3, 0, 1)) ** 1.5
    whoosh = sweep_lowpass(highpass(noise(len(t), rng), 400), 1200, 6000) * shape
    flick = crackles(rng, length, 25, (0.2, 0.6)) * shape
    return finish(whoosh + flick * 0.5, peak_db=-3.0)


def sizzle(seed=4):
    rng = np.random.default_rng(seed)
    length = 0.5
    hiss = highpass(noise(int(length * SR), rng), 3000) * env(length, 0.005, 0.12)
    pops = crackles(rng, length, 30, (0.3, 0.8)) * env(length, 0.0, 0.15)
    return finish(hiss + pops * 0.5, peak_db=-3.0)


def extinguish(seed=5):
    rng = np.random.default_rng(seed)
    length = 2.2
    t = t_axis(length)
    n = len(t)
    fizz = sweep_lowpass(highpass(noise(n, rng), 1500), 9000, 1500) * env(length, 0.003, 0.5) * 0.7
    thup = sine(70 * np.exp(-t / 0.08) + 35) * env(length, 0.004, 0.15) * 0.9
    fall = triangle(note(64) * (0.5 ** (t / 1.2))) * env(length, 0.02, 0.6) * 0.35
    return finish(mix(fizz, thup, fall))


def level_complete():
    # A warm major chord that swells in, with a little rising sparkle on top.
    length = 4.0
    t = t_axis(length)
    n = len(t)
    swell = np.clip(t / 0.9, 0, 1) * np.exp(-np.maximum(t - 1.6, 0) / 0.9)
    pad = sum(0.5 * triangle(np.full(n, note(m))) + 0.15 * sine(np.full(n, note(m) * 2))
              for m in [57, 64, 69, 73, 76])        # A, E, A, C#, E
    out = lowpass(pad, 3000) * swell * 0.35
    for i, midi in enumerate([81, 85, 88, 93]):
        at = 0.5 + i * 0.12
        d = length - at
        tone = triangle(np.full(int(d * SR), note(midi))) * env(d, 0.002, 0.35)
        place(out, tone * 0.35, at)
    return finish(crush(out, bits=10, hold_rate=30000), peak_db=-3.0)


def heartbeat():
    # Low heat warning: lub-dub.
    length = 0.7
    t = t_axis(length)
    out = np.zeros(len(t))
    for at, g in [(0.0, 1.0), (0.18, 0.7)]:
        d = length - at
        tt = t_axis(d)
        beat = sine(60 * np.exp(-tt / 0.05) + 38) * env(d, 0.004, 0.07)
        place(out, beat * g, at)
    return finish(lowpass(out, 300), peak_db=-6.0)


def ui_select(seed=6):
    rng = np.random.default_rng(seed)
    length = 0.12
    f = np.full(int(length * SR), note(79))
    blip = 0.7 * square(f, 0.25) * env(length, 0.001, 0.03)
    tick = highpass(noise(len(f), rng), 4000) * env(length, 0.0002, 0.003) * 0.3
    return finish(crush(blip + tick, bits=6, hold_rate=22050), peak_db=-8.0)


# ---------------------------------------------------------------------- loops

def fire_loop(seed=11):
    rng = np.random.default_rng(seed)
    period = 4.0
    n = int(period * SR)

    def one(r):
        return np.stack([noise(n, r), crackles(r, period, 22, wrap=True)])

    def post(x):
        roar = lowpass(x[0], 500) * 0.9 + bandpass(x[0], 600, 2000) * 0.15
        return roar + x[1] * 0.7

    # Slow breathing in the roar: three whole cycles per period.
    breathe = 0.8 + 0.2 * np.sin(2 * np.pi * 3 / period * t_axis(period))
    return finish(periodic(one, period)(rng, post) * breathe, peak_db=-4.0, loop=True)


def hiss_loop(seed=12):
    rng = np.random.default_rng(seed)
    period = 2.0
    n = int(period * SR)
    make = periodic(lambda r: noise(n, r), period)
    out = make(rng, lambda x: highpass(x, 2500) * 0.8 + bandpass(x, 900, 2500) * 0.3)
    flutter = 0.85 + 0.15 * np.sin(2 * np.pi * 4 / period * t_axis(period))
    return finish(out * flutter, peak_db=-6.0, loop=True)


def wind_loop(seed=13):
    rng = np.random.default_rng(seed)
    period = 8.0
    n = int(period * SR)
    t = t_axis(period)
    make = periodic(lambda r: noise(n, r), period)
    low = make(rng, lambda x: lowpass(x, 500))
    high = make(np.random.default_rng(seed + 1), lambda x: bandpass(x, 700, 1800))
    # Gusts: two overlapping swells with whole cycles per period.
    gust = 0.55 + 0.3 * np.sin(2 * np.pi * 2 / period * t) + 0.15 * np.sin(2 * np.pi * 5 / period * t + 1)
    out = low * gust + high * gust ** 2 * 0.5
    return finish(out, peak_db=-5.0, loop=True)


def music_loop(seed=21):
    """
    A slow ambient bed: four soft chords, eight seconds each, with sparse
    pentatonic plucks drifting over them. Every note is placed with
    wrap-around and every filter runs through periodic(), so it loops
    without a seam.
    """
    rng = np.random.default_rng(seed)
    period = 32.0
    n = int(period * SR)

    chords = [[45, 52, 57, 60, 64],     # A minor
              [41, 48, 53, 57, 64],     # F major 7, gentle
              [48, 55, 60, 64, 67],     # C major
              [43, 50, 55, 59, 62]]     # G major
    pentatonic = [69, 72, 74, 76, 79, 81, 84]

    def one(r):
        pads = np.zeros(n)
        for i, chord in enumerate(chords):
            start = i * 8.0
            d = 9.5                          # overlaps the next chord a little
            tt = t_axis(d)
            shape = np.clip(tt / 2.5, 0, 1) * np.clip((d - tt) / 2.5, 0, 1)
            for m in chord:
                f = np.full(len(tt), note(m)) * (1 + 0.002 * np.sin(2 * np.pi * r.uniform(0.1, 0.3) * tt))
                tone = 0.6 * triangle(f) + 0.25 * sine(f * 0.5)
                place_wrapped(pads, tone * shape * 0.12, start)
        plucks = np.zeros(n)
        for k in range(22):
            at = (k * period / 22 + r.uniform(-0.4, 0.4)) % period
            m = int(r.choice(pentatonic))
            d = 2.5
            f = np.full(int(d * SR), note(m))
            tone = (0.6 * triangle(f) + 0.2 * square(f, 0.25)) * env(d, 0.004, 0.5)
            place_wrapped(plucks, tone * r.uniform(0.08, 0.16), at)
        return np.stack([pads, plucks])

    def post(x):
        pads = lowpass(x[0], 1600)
        plucks = lowpass(x[1], 3500)
        wet = np.roll(plucks, int(0.45 * SR)) * 0.35 + np.roll(plucks, int(0.9 * SR)) * 0.15
        return pads + plucks + lowpass(wet, 2000)

    out = periodic(one, period, warmup_periods=2)(rng, post)
    return finish(crush(out, bits=12, hold_rate=32000), peak_db=-6.0, loop=True)


SOUNDS = {
    "ignite":          ignite,
    "ember_pickup":    ember_pickup,
    "dash":            dash,
    "sizzle":          sizzle,
    "extinguish":      extinguish,
    "level_complete":  level_complete,
    "heartbeat":       heartbeat,
    "ui_select":       ui_select,
    "fire_loop":       fire_loop,
    "hiss_loop":       hiss_loop,
    "wind_loop":       wind_loop,
    "music_loop":      music_loop,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--only", default="")
    parser.add_argument("--out", default=OUT)
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for name, build in SOUNDS.items():
        if args.only and args.only not in name:
            continue
        x = build()
        wavfile.write(os.path.join(args.out, name + ".wav"), SR, (np.clip(x, -1, 1) * 32767).astype(np.int16))
        print(f"  {name:16s} {len(x) / SR:5.2f}s")


if __name__ == "__main__":
    main()
