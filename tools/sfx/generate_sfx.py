#!/usr/bin/env python3
"""
generate_sfx.py - sound effect starter pack for Mud Cats

Every sound is synthesised from scratch: noise, square and sine waves,
pitch sweeps and filters, with a little bit-crushing for a retro edge.
Nothing is sampled, so the results are yours to use however you like.

    python3 tools/sfx/generate_sfx.py            # writes assets/sounds/*.wav
    python3 tools/sfx/generate_sfx.py --only mg   # just the ones whose name contains "mg"

Needs numpy and scipy (pip install numpy scipy).

Each sound is seeded, so a run always produces the same files. Change a
seed or a number and regenerate to get a new variation. Files ending in
_loop are built to loop seamlessly.
"""

import argparse
import os

import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "sounds")


# ------------------------------------------------------------ building blocks

def t_axis(seconds):
    return np.arange(int(seconds * SR)) / SR


def env(seconds, attack=0.002, decay=0.2, curve=1.0):
    """Instant-ish attack, exponential-style decay. `curve` > 1 decays faster at the start."""
    t = t_axis(seconds)
    a = np.clip(t / max(attack, 1e-6), 0.0, 1.0)
    d = np.exp(-np.maximum(t - attack, 0.0) / max(decay, 1e-6)) ** curve
    return a * d


def noise(n, rng):
    return rng.uniform(-1.0, 1.0, n)


def sweep_phase(freqs):
    """Phase (in cycles) for a per-sample frequency array."""
    return np.cumsum(freqs) / SR


def sine(freqs):
    return np.sin(2.0 * np.pi * sweep_phase(freqs))


def square(freqs, duty=0.5):
    return np.where((sweep_phase(freqs) % 1.0) < duty, 1.0, -1.0)


def triangle(freqs):
    p = sweep_phase(freqs) % 1.0
    return 4.0 * np.abs(p - 0.5) - 1.0


def lowpass(x, cutoff, order=2):
    b, a = signal.butter(order, min(cutoff, SR * 0.45) / (SR / 2), "low")
    return signal.lfilter(b, a, x)


def highpass(x, cutoff, order=2):
    b, a = signal.butter(order, cutoff / (SR / 2), "high")
    return signal.lfilter(b, a, x)


def bandpass(x, lo, hi, order=2):
    b, a = signal.butter(order, [lo / (SR / 2), min(hi, SR * 0.45) / (SR / 2)], "band")
    return signal.lfilter(b, a, x)


def sweep_lowpass(x, start_hz, end_hz, block=64):
    """Low-pass whose cutoff glides from start to end over the sound (log-spaced)."""
    out = np.zeros_like(x)
    zi = None
    n = len(x)
    for i in range(0, n, block):
        frac = i / max(n - 1, 1)
        cutoff = start_hz * (end_hz / start_hz) ** frac
        b, a = signal.butter(2, min(cutoff, SR * 0.45) / (SR / 2), "low")
        if zi is None:
            zi = signal.lfilter_zi(b, a) * 0.0
        out[i:i + block], zi = signal.lfilter(b, a, x[i:i + block], zi=zi)
    return out


def crush(x, bits=8, hold_rate=22050):
    """Retro grit: hold each sample for a while, then quantise."""
    step = SR / hold_rate
    idx = (np.floor(np.arange(len(x)) / step) * step).astype(int)
    held = x[np.clip(idx, 0, len(x) - 1)]
    levels = 2 ** (bits - 1)
    peak = np.max(np.abs(held)) or 1.0
    return np.round(held / peak * levels) / levels * peak


def echo(x, delays, gains, cutoff=None):
    """Adds delayed, quieter copies; each optionally darker, like reflections off hills."""
    longest = int(max(delays) * SR)
    out = np.concatenate([x, np.zeros(longest)])
    for d, g in zip(delays, gains):
        copy = lowpass(x, cutoff) if cutoff else x
        start = int(d * SR)
        out[start:start + len(x)] += copy * g
    return out


def pad(x, seconds):
    n = int(seconds * SR)
    return np.concatenate([x, np.zeros(max(0, n - len(x)))])[:max(n, len(x))]


def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros(n)
    for p in parts:
        out[:len(p)] += p
    return out


def place(canvas, sound, at_seconds, gain=1.0):
    start = max(0, int(at_seconds * SR))
    end = min(len(canvas), start + len(sound))
    if end > start:
        canvas[start:end] += sound[:end - start] * gain


def place_wrapped(canvas, sound, at_seconds, gain=1.0):
    """Like place, but anything past the end wraps to the start: for loops."""
    n = len(canvas)
    idx = (int(at_seconds * SR) + np.arange(len(sound))) % n
    np.add.at(canvas, idx, sound * gain)


def finish(x, peak_db=-1.0, fade_in=0.001, fade_out=0.015, loop=False):
    """Remove DC, fade the edges (unless looping), trim silence, normalise."""
    x = x - np.mean(x)
    if not loop:
        nz = np.nonzero(np.abs(x) > 1e-4 * (np.max(np.abs(x)) or 1.0))[0]
        if len(nz):
            x = x[: nz[-1] + 1]
        fi, fo = int(fade_in * SR), int(fade_out * SR)
        if fi:
            x[:fi] *= np.linspace(0.0, 1.0, fi)
        if fo and len(x) > fo:
            x[-fo:] *= np.linspace(1.0, 0.0, fo)
    peak = np.max(np.abs(x)) or 1.0
    return x / peak * (10 ** (peak_db / 20.0))


def periodic(build, period_seconds, warmup_periods=3):
    """
    Seamless loops. `build(rng)` makes one period of raw signal; it is
    repeated, filtered by `build`'s own filters through several periods so
    every filter reaches steady state, and the last period kept. Because
    the input repeats exactly, the output does too: no click at the seam.
    """
    def make(rng, post):
        one = build(rng)
        reps = warmup_periods + 1
        tiled = np.tile(one, (1, reps)) if one.ndim == 2 else np.tile(one, reps)
        processed = post(tiled)
        n = one.shape[-1]
        return processed[-n:]
    return make


# ------------------------------------------------------------------- weapons

def gunshot(rng, body_cutoff=2600, body_decay=0.06, crack=1.0, thump_hz=95, thump=0.9, length=0.45):
    t = t_axis(length)
    n = len(t)
    blast = lowpass(noise(n, rng), body_cutoff) * env(length, 0.0005, body_decay, 1.3)
    snap = highpass(noise(n, rng), 3000) * env(length, 0.0002, 0.006) * crack
    f = thump_hz * np.exp(-t / 0.05) + 40
    boom = sine(f) * env(length, 0.0008, 0.07) * thump
    return blast + snap + boom


def mg_burst(seed, shots, interval=0.092):
    rng = np.random.default_rng(seed)
    length = shots * interval + 0.9
    out = np.zeros(int(length * SR))
    for i in range(shots):
        jitter = rng.uniform(-0.006, 0.006)
        shot = gunshot(rng, body_cutoff=rng.uniform(2200, 3000), body_decay=0.045,
                       crack=rng.uniform(0.7, 1.0), thump_hz=rng.uniform(85, 105), thump=0.8)
        place(out, shot, i * interval + jitter, gain=rng.uniform(0.85, 1.0))
        # The action cycling: a short metallic tick between shots.
        tick = bandpass(noise(int(0.012 * SR), rng), 2500, 6000) * env(0.012, 0.0002, 0.003) * 0.35
        place(out, tick, i * interval + 0.045)
    # The burst rolling away across the field.
    out = echo(out, [0.16, 0.34], [0.22, 0.1], cutoff=900)
    return finish(crush(out, bits=10, hold_rate=30000))


def rifle_shot(seed):
    rng = np.random.default_rng(seed)
    shot = gunshot(rng, body_cutoff=3400, body_decay=0.09, crack=1.2, thump_hz=120, thump=0.6, length=0.6)
    out = echo(shot, [0.21, 0.47, 0.8], [0.3, 0.16, 0.07], cutoff=700)
    return finish(crush(out, bits=10, hold_rate=32000))


def tank_cannon(seed=11):
    rng = np.random.default_rng(seed)
    length = 2.8
    t = t_axis(length)
    n = len(t)
    crack = highpass(noise(n, rng), 1500) * env(length, 0.0003, 0.02) * 0.9
    blast = sweep_lowpass(noise(n, rng), 3500, 160) * env(length, 0.001, 0.35, 1.1) * 1.2
    f = 70 * np.exp(-t / 0.25) + 28
    boom = sine(f) * env(length, 0.002, 0.45) * 1.4
    rumble = lowpass(noise(n, rng), 120) * env(length, 0.05, 0.9) * 0.9
    out = mix(crack, blast, boom, rumble)
    out = echo(out, [0.35, 0.9], [0.25, 0.12], cutoff=400)
    return finish(crush(out, bits=11, hold_rate=30000))


def explosion_near(seed=21):
    rng = np.random.default_rng(seed)
    length = 3.0
    t = t_axis(length)
    n = len(t)
    blast = sweep_lowpass(noise(n, rng), 5000, 180) * env(length, 0.001, 0.5, 1.0) * 1.2
    f = 60 * np.exp(-t / 0.3) + 25
    thump = sine(f) * env(length, 0.002, 0.5) * 1.5
    # Debris pattering down: sparse clicks thinning out over time.
    debris = np.zeros(n)
    for _ in range(140):
        at = rng.uniform(0.15, 2.4) ** 1.4 / 2.4 ** 0.4
        bit = bandpass(noise(int(0.01 * SR), rng), 1200, 5000) * env(0.01, 0.0002, 0.002)
        place(debris, bit, at, gain=rng.uniform(0.05, 0.25) * max(0.1, 1 - at / 2.6))
    out = mix(blast, thump, debris)
    return finish(crush(out, bits=10, hold_rate=28000))


def artillery_far(seed=31):
    rng = np.random.default_rng(seed)
    length = 3.5
    t = t_axis(length)
    n = len(t)
    boom = lowpass(noise(n, rng), 260) * env(length, 0.03, 0.6) * 1.0
    f = 45 * np.exp(-t / 0.4) + 22
    sub = sine(f) * env(length, 0.02, 0.7) * 0.8
    out = mix(boom, sub)
    out = echo(out, [0.55, 1.2, 1.9], [0.45, 0.28, 0.14], cutoff=180)
    return finish(out)


def shell_incoming(seed=41):
    rng = np.random.default_rng(seed)
    length = 1.8
    t = t_axis(length)
    f = 1500 - 850 * (t / length) ** 1.6
    f = f * (1 + 0.012 * np.sin(2 * np.pi * 7 * t))
    tone = 0.6 * sine(f) + 0.25 * triangle(f * 2.01)
    air = bandpass(noise(len(t), rng), 900, 4000) * 0.15
    grow = (t / length) ** 2.2
    out = (tone + air) * grow
    # It ends abruptly, so an explosion can be played right on the cut.
    return finish(out, fade_out=0.004)


def ricochet(seed, start=2400, end=1300, length=0.75):
    rng = np.random.default_rng(seed)
    t = t_axis(length)
    f = start * (end / start) ** (t / length)
    f = f * (1 + 0.04 * np.sin(2 * np.pi * rng.uniform(18, 30) * t))
    whine = sine(f) * env(length, 0.002, 0.25)
    ping = highpass(noise(len(t), rng), 3000) * env(length, 0.0002, 0.008) * 0.8
    return finish(crush(mix(whine, ping), bits=9, hold_rate=26000))


def impact_dirt(seed):
    rng = np.random.default_rng(seed)
    length = 0.45
    t = t_axis(length)
    n = len(t)
    thud = sine(110 * np.exp(-t / 0.03) + 45) * env(length, 0.001, 0.05)
    crumble = lowpass(noise(n, rng), 1800) * env(length, 0.002, 0.09) * 0.7
    grit = bandpass(noise(n, rng), 2000, 7000) * env(length, 0.001, 0.02) * 0.4
    return finish(mix(thud, crumble, grit))


def armor_hit(seed=51):
    rng = np.random.default_rng(seed)
    length = 1.6
    t = t_axis(length)
    n = len(t)
    # Inharmonic partials, each decaying at its own rate: that is what
    # makes it read as a struck plate of steel rather than a note.
    partials = [(310, 0.6, 0.5), (742, 0.5, 0.35), (1290, 0.4, 0.25), (2170, 0.3, 0.15), (3110, 0.2, 0.09)]
    ring = sum(a * sine(np.full(n, f * rng.uniform(0.98, 1.02))) * env(length, 0.0005, d) for f, a, d in partials)
    hit = highpass(noise(n, rng), 1500) * env(length, 0.0002, 0.01) * 1.0
    thump = sine(90 * np.exp(-t / 0.04) + 40) * env(length, 0.001, 0.06) * 0.8
    return finish(crush(mix(ring, hit, thump), bits=10, hold_rate=30000))


# ---------------------------------------------------------------------- tank

def diesel(rng, seconds, rate_curve, roughness=0.25):
    """Engine firing as a train of pulses; `rate_curve` is firing rate (Hz) per sample."""
    n = int(seconds * SR)
    out = np.zeros(n)
    phase = sweep_phase(rate_curve[:n])
    fire = np.nonzero(np.diff(np.floor(phase)) > 0)[0]
    pulse_len = int(0.03 * SR)
    shape = env(0.03, 0.001, 0.008)
    for i in fire:
        p = lowpass(noise(pulse_len, rng), 900) * shape * rng.uniform(1 - roughness, 1 + roughness)
        end = min(n, i + pulse_len)
        out[i:end] += p[:end - i]
    return out


def engine_idle_loop(seed=61):
    rng = np.random.default_rng(seed)
    period = 2.0
    n = int(period * SR)
    # Exactly 44 firings and whole cycles of every tone per period (22 Hz
    # firing, 44 Hz hum), so the rhythm loops cleanly. All filtering
    # happens inside periodic(), so no filter start-up lands on the seam.
    hum = 0.25 * sine(np.full(n, 44.0)) + 0.12 * square(np.full(n, 22.0), 0.3)
    make = periodic(lambda r: np.stack([diesel(r, period, np.full(n + 10, 22.0)), hum]), period)

    def post(x):
        pulses, tone = x[0], x[1]
        body = lowpass(pulses, 700) + 0.4 * lowpass(highpass(pulses, 120), 300)
        return lowpass(body, 900) + lowpass(tone, 400)

    return finish(make(rng, post), peak_db=-3.0, loop=True)


def engine_rev(seed=62):
    rng = np.random.default_rng(seed)
    length = 2.8
    t = t_axis(length)
    rate = 22 + 26 * np.clip(np.sin(np.pi * np.clip((t - 0.3) / 2.0, 0, 1)) ** 1.5, 0, 1)
    out = diesel(rng, length, rate, roughness=0.3)
    out = lowpass(out, 900) * 1.2
    growl = 0.3 * square(rate * 2, 0.3)
    return finish(lowpass(out + lowpass(growl, 500), 1200))


def tracks_clank_loop(seed=71):
    rng = np.random.default_rng(seed)
    period = 2.0
    n = int(period * SR)

    # The squeal wobbles once per period and has a whole number of cycles,
    # so it is periodic as it stands.
    squeal = 0.05 * sine(np.full(n, 1900.0) * (1 + 0.01 * np.sin(2 * np.pi * 0.5 * t_axis(period))))

    def one(r):
        clanks = np.zeros(n)
        for k in range(18):   # 9 links a second
            ping = bandpass(noise(int(0.04 * SR), r), 700, 3500) * env(0.04, 0.0005, 0.012)
            place_wrapped(clanks, ping, (k / 9.0 + r.uniform(-0.004, 0.004)) % period,
                          gain=r.uniform(0.6, 1.0))
        return np.stack([clanks, noise(n, r)])

    make = periodic(one, period)
    out = make(rng, lambda x: x[0] + lowpass(x[1], 400) * 0.3)
    return finish(out + squeal, peak_db=-3.0, loop=True)


def turret_traverse_loop(seed=81):
    rng = np.random.default_rng(seed)
    period = 2.0
    n = int(period * SR)
    whine = 0.4 * square(np.full(n, 180.0), 0.4) + 0.3 * sine(np.full(n, 360.0))
    ticks = np.zeros(n)
    for k in range(60):   # gear teeth, 30 a second
        tick = bandpass(noise(int(0.006 * SR), rng), 2000, 6000) * env(0.006, 0.0002, 0.0015)
        place(ticks, tick, k / 30.0, gain=0.5)
    make = periodic(lambda r: whine + ticks, period)
    out = make(rng, lambda x: lowpass(x, 2200))
    return finish(out, peak_db=-4.0, loop=True)


def shell_load(seed=91):
    rng = np.random.default_rng(seed)
    length = 0.9
    t = t_axis(length)
    n = len(t)
    slide = bandpass(noise(n, rng), 800, 5000) * np.clip((0.25 - t) / 0.25, 0, 1) ** 0.5 * (t < 0.25) * 0.4
    clunk = np.zeros(n)
    hit = mix(sine(np.full(int(0.5 * SR), 160.0)) * env(0.5, 0.001, 0.06),
              sine(np.full(int(0.5 * SR), 1320.0)) * env(0.5, 0.0005, 0.08) * 0.35,
              highpass(noise(int(0.5 * SR), rng), 1500) * env(0.5, 0.0002, 0.008) * 0.7)
    place(clunk, hit, 0.26)
    return finish(crush(slide + clunk, bits=10, hold_rate=30000))


# --------------------------------------------------------------------- radio

def radio_static_loop(seed=101):
    rng = np.random.default_rng(seed)
    period = 3.0
    n = int(period * SR)

    def one(r):
        hiss = noise(n, r) * 0.5
        flutter = 0.75 + 0.25 * np.sin(2 * np.pi * (1 / period) * 3 * t_axis(period))
        pops = np.zeros(n)
        for _ in range(25):
            pop = noise(int(0.004 * SR), r) * env(0.004, 0.0001, 0.001)
            place(pops, pop, r.uniform(0, period - 0.01), gain=r.uniform(0.6, 1.6))
        return hiss * flutter + pops

    make = periodic(one, period)
    out = make(rng, lambda x: bandpass(x, 350, 3200))
    return finish(out, peak_db=-6.0, loop=True)


def radio_squelch(seed=102):
    rng = np.random.default_rng(seed)
    length = 0.32
    t = t_axis(length)
    burst = bandpass(noise(len(t), rng), 500, 3500) * (t < 0.22) * 0.8
    tail = sine(np.full(len(t), 1100.0)) * ((t > 0.24) & (t < 0.3)) * 0.5
    return finish(crush(burst + tail, bits=8, hold_rate=16000))


# ---------------------------------------------------------------------- desk

def paper_rustle(seed, length=0.55):
    rng = np.random.default_rng(seed)
    t = t_axis(length)
    n = len(t)
    crinkles = np.zeros(n)
    for _ in range(int(length * 70)):
        c = noise(int(0.008 * SR), rng) * env(0.008, 0.0005, 0.002)
        place(crinkles, c, rng.uniform(0, length - 0.01), gain=rng.uniform(0.2, 1.0))
    body = noise(n, rng) * 0.25 * np.sin(np.pi * t / length) ** 0.6
    return finish(bandpass(crinkles + body, 1500, 9000) * 0.9)


def page_turn(seed=121):
    rng = np.random.default_rng(seed)
    length = 0.38
    t = t_axis(length)
    swish = noise(len(t), rng) * np.sin(np.pi * t / length) ** 1.5
    out = sweep_lowpass(highpass(swish, 900), 2500, 8000)
    snap = paper_rustle(seed + 1, 0.06)
    return finish(mix(out, pad(np.zeros(1), 0.3), np.concatenate([np.zeros(int(0.3 * SR)), snap * 0.5])))


def footstep_mud(seed):
    rng = np.random.default_rng(seed)
    length = 0.35
    t = t_axis(length)
    n = len(t)
    squelch = lowpass(noise(n, rng), 600) * env(length, 0.01, 0.07)
    suck = bandpass(noise(n, rng), 300, 900) * np.exp(-((t - 0.16) / 0.04) ** 2) * 0.6
    wobble = sine(np.full(n, 140.0) * (1 + 0.3 * np.sin(2 * np.pi * 18 * t))) * env(length, 0.005, 0.05) * 0.4
    return finish(mix(squelch, suck, wobble))


# ------------------------------------------------------------------------ UI

def ui_click(seed=131):
    rng = np.random.default_rng(seed)
    length = 0.05
    blip = square(np.full(int(length * SR), 1250.0), 0.25) * env(length, 0.0005, 0.012)
    tick = highpass(noise(int(length * SR), rng), 4000) * env(length, 0.0002, 0.002) * 0.4
    return finish(crush(blip + tick, bits=6, hold_rate=22050), peak_db=-6.0)


def ui_hover():
    length = 0.03
    blip = square(np.full(int(length * SR), 1900.0), 0.125) * env(length, 0.0005, 0.006)
    return finish(blip, peak_db=-12.0)


def letter_arrives():
    # A short, original rising arpeggio: something good in the post.
    notes = [(72, 0.0), (76, 0.09), (79, 0.18), (84, 0.27)]
    length = 1.1
    out = np.zeros(int(length * SR))
    for midi, at in notes:
        f = 440.0 * 2 ** ((midi - 69) / 12)
        d = length - at
        tone = 0.6 * triangle(np.full(int(d * SR), f)) + 0.2 * square(np.full(int(d * SR), f), 0.25)
        place(out, tone * env(d, 0.003, 0.25), at)
    return finish(crush(out, bits=8, hold_rate=22050), peak_db=-4.0)


# ------------------------------------------------------------------- catalog

SOUNDS = {
    "mg_burst_1":            lambda: mg_burst(1, 5),
    "mg_burst_2":            lambda: mg_burst(2, 7),
    "mg_burst_3":            lambda: mg_burst(3, 3),
    "rifle_shot_1":          lambda: rifle_shot(4),
    "rifle_shot_2":          lambda: rifle_shot(5),
    "tank_cannon":           tank_cannon,
    "explosion_near":        explosion_near,
    "artillery_far":         artillery_far,
    "shell_incoming":        shell_incoming,
    "ricochet_1":            lambda: ricochet(6, 2400, 1300),
    "ricochet_2":            lambda: ricochet(7, 3100, 1900, 0.6),
    "impact_dirt_1":         lambda: impact_dirt(8),
    "impact_dirt_2":         lambda: impact_dirt(9),
    "armor_hit":             armor_hit,
    "engine_idle_loop":      engine_idle_loop,
    "engine_rev":            engine_rev,
    "tracks_clank_loop":     tracks_clank_loop,
    "turret_traverse_loop":  turret_traverse_loop,
    "shell_load":            shell_load,
    "radio_static_loop":     radio_static_loop,
    "radio_squelch":         radio_squelch,
    "paper_rustle_1":        lambda: paper_rustle(111),
    "paper_rustle_2":        lambda: paper_rustle(112, 0.45),
    "page_turn":             page_turn,
    "footstep_mud_1":        lambda: footstep_mud(141),
    "footstep_mud_2":        lambda: footstep_mud(142),
    "ui_click":              ui_click,
    "ui_hover":              ui_hover,
    "letter_arrives":        letter_arrives,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--only", default="", help="only generate sounds whose name contains this")
    parser.add_argument("--out", default=OUT, help="output folder (default assets/sounds)")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    for name, build in SOUNDS.items():
        if args.only and args.only not in name:
            continue
        x = build()
        path = os.path.join(args.out, name + ".wav")
        wavfile.write(path, SR, (np.clip(x, -1, 1) * 32767).astype(np.int16))
        print(f"  {name:22s} {len(x) / SR:5.2f}s")


if __name__ == "__main__":
    main()
