#!/usr/bin/env python3
"""
generate_mudcats.py - sounds and music for the Mud Cats infantry demo

Built from the same pieces as generate_sfx.py, so everything is synthesised
from scratch and yours to use. That includes the voices: the screams, groans
and shouted orders are a buzz pushed through vowel-shaped filters, the way
old speech synthesisers worked, so they come out rough and not quite human.

    python3 tools/sfx/generate_mudcats.py          # writes assets/mudcats/*.wav
    python3 tools/sfx/generate_mudcats.py --only scream

Files ending in _loop loop seamlessly. The two music layers are the same
length and in the same key, so the game plays both at once and fades
between them as the fighting picks up and dies down.
"""

import argparse
import os
import sys

import numpy as np
from scipy import signal as sps
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_sfx import (SR, t_axis, env, noise, sine, square, triangle, lowpass, highpass,  # noqa: E402
                          bandpass, sweep_lowpass, crush, echo, mix, place, place_wrapped,
                          finish, periodic, gunshot, explosion_near, artillery_far,
                          shell_incoming, ricochet, impact_dirt, footstep_mud, mg_burst,
                          sweep_phase)

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "mudcats")

# Long beds are written at half rate: they are mostly low rumble and pads,
# and it halves the size of the biggest files.
HALF_RATE = {"music_calm_loop", "music_combat_loop", "front_loop"}


def note(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def saw(freqs):
    return 2.0 * (sweep_phase(freqs) % 1.0) - 1.0


def click(rng, length=0.012, lo=2000, hi=7000):
    return bandpass(noise(int(length * SR), rng), lo, hi) * env(length, 0.0002, length / 4)


# ------------------------------------------------------------------- weapons

def rifle(seed):
    rng = np.random.default_rng(seed)
    shot = gunshot(rng, body_cutoff=3200, body_decay=0.085, crack=1.25, thump_hz=115, thump=0.7, length=0.6)
    out = echo(shot, [0.23, 0.52, 0.9], [0.32, 0.17, 0.08], cutoff=650)
    return finish(crush(out, bits=10, hold_rate=32000))


def rifle_far(seed):
    rng = np.random.default_rng(seed)
    shot = gunshot(rng, body_cutoff=1500, body_decay=0.07, crack=0.3, thump_hz=90, thump=0.5, length=0.5)
    out = echo(lowpass(shot, 1100), [0.3, 0.7, 1.2], [0.45, 0.25, 0.12], cutoff=500)
    return finish(out, peak_db=-6.0)


def smg(seed):
    rng = np.random.default_rng(seed)
    shot = gunshot(rng, body_cutoff=2300, body_decay=0.035, crack=0.75, thump_hz=100, thump=0.55, length=0.3)
    return finish(crush(echo(shot, [0.12, 0.27], [0.2, 0.08], cutoff=800), bits=10, hold_rate=30000))


def pistol(seed):
    rng = np.random.default_rng(seed)
    shot = gunshot(rng, body_cutoff=2900, body_decay=0.03, crack=0.95, thump_hz=130, thump=0.4, length=0.3)
    return finish(crush(echo(shot, [0.15, 0.33], [0.22, 0.1], cutoff=900), bits=10, hold_rate=30000))


def mg(seed):
    rng = np.random.default_rng(seed)
    shot = gunshot(rng, body_cutoff=2600, body_decay=0.05, crack=0.95, thump_hz=95, thump=0.85, length=0.35)
    tick = click(rng, 0.012, 2500, 6000) * 0.3
    out = mix(shot, np.concatenate([np.zeros(int(0.045 * SR)), tick]))
    return finish(crush(echo(out, [0.14, 0.31], [0.22, 0.1], cutoff=850), bits=10, hold_rate=30000))


def mg_far(seed):
    rng = np.random.default_rng(seed)
    shot = gunshot(rng, body_cutoff=1300, body_decay=0.05, crack=0.2, thump_hz=80, thump=0.6, length=0.35)
    return finish(echo(lowpass(shot, 1000), [0.25, 0.6], [0.4, 0.2], cutoff=450), peak_db=-6.0)


def rifle_bolt(seed=201):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.42 * SR))
    for at, lo, hi, g in [(0.0, 1500, 4500, 0.8), (0.07, 2500, 7000, 0.5), (0.24, 1200, 4000, 0.9), (0.3, 2500, 6500, 0.6)]:
        place(out, click(rng, 0.018, lo, hi), at, g)
    return finish(out, peak_db=-5.0)


def rifle_reload(seed=202):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(1.1 * SR))
    # Bolt back, the clip pressed in, the empty clip pinging out, bolt home.
    for at, lo, hi, g in [(0.0, 1500, 4500, 0.7), (0.08, 2500, 7000, 0.4),
                          (0.35, 2000, 6000, 0.6), (0.38, 2000, 6000, 0.5), (0.42, 2000, 6000, 0.5),
                          (0.45, 2000, 6000, 0.4), (0.48, 2000, 6000, 0.45),
                          (0.85, 1200, 4000, 0.9), (0.92, 2500, 6500, 0.6)]:
        place(out, click(rng, 0.018, lo, hi), at, g)
    ping = sine(np.full(int(0.3 * SR), 3400.0)) * env(0.3, 0.001, 0.06) * 0.25
    place(out, ping, 0.6)
    return finish(out, peak_db=-5.0)


def smg_reload(seed=203):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(1.0 * SR))
    for at, lo, hi, g in [(0.0, 1200, 3500, 0.7), (0.05, 2000, 5000, 0.4),
                          (0.55, 900, 3000, 0.9), (0.6, 2500, 6000, 0.5), (0.85, 1500, 4500, 0.8)]:
        place(out, click(rng, 0.022, lo, hi), at, g)
    return finish(out, peak_db=-5.0)


def mg_reload(seed=204):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(2.4 * SR))
    place(out, click(rng, 0.03, 900, 3000), 0.0, 1.0)              # cover up
    for k in range(26):                                             # belt rattling in
        place(out, click(rng, 0.01, 2500, 7000), 0.4 + k * 0.035 + rng.uniform(0, 0.01), rng.uniform(0.2, 0.5))
    place(out, click(rng, 0.04, 600, 2500), 1.6, 1.0)               # cover slammed
    place(out, click(rng, 0.025, 1200, 4000), 2.0, 0.8)             # charging handle
    place(out, click(rng, 0.02, 2000, 5000), 2.08, 0.6)
    return finish(out, peak_db=-5.0)


def click_empty(seed=205):
    rng = np.random.default_rng(seed)
    return finish(click(rng, 0.02, 2500, 6000), peak_db=-10.0)


def mg_setup(seed=206):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.9 * SR))
    thud = sine(80 * np.exp(-t_axis(0.3) / 0.04) + 40) * env(0.3, 0.002, 0.06)
    place(out, thud, 0.0, 0.8)
    place(out, click(rng, 0.03, 800, 3000), 0.05, 0.6)
    place(out, click(rng, 0.02, 1500, 5000), 0.55, 0.7)
    place(out, click(rng, 0.02, 2500, 6000), 0.62, 0.5)
    return finish(out, peak_db=-4.0)


def overheat(seed=207):
    rng = np.random.default_rng(seed)
    length = 1.4
    hiss = highpass(noise(int(length * SR), rng), 3500) * env(length, 0.02, 0.4)
    ping = sine(np.full(int(length * SR), 2600.0)) * env(length, 0.001, 0.2) * 0.2
    return finish(mix(hiss, ping), peak_db=-6.0)


# ------------------------------------------------------------ grenades, fire

def grenade_pin(seed=211):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.5 * SR))
    place(out, click(rng, 0.02, 2000, 7000), 0.0, 0.8)
    ring = sine(np.full(int(0.4 * SR), 4100.0)) * env(0.4, 0.001, 0.08) * 0.25
    place(out, ring, 0.01)
    place(out, click(rng, 0.03, 600, 2500), 0.25, 0.6)              # the cap thrown aside
    return finish(out, peak_db=-6.0)


def grenade_bounce(seed=212):
    rng = np.random.default_rng(seed)
    length = 0.3
    t = t_axis(length)
    thunk = sine(160 * np.exp(-t / 0.02) + 90) * env(length, 0.001, 0.03)
    mud = lowpass(noise(len(t), rng), 900) * env(length, 0.002, 0.04) * 0.6
    return finish(mix(thunk, mud), peak_db=-4.0)


def throw(seed=213):
    rng = np.random.default_rng(seed)
    length = 0.35
    t = t_axis(length)
    shape = np.sin(np.pi * np.clip(t / 0.25, 0, 1)) ** 2
    return finish(sweep_lowpass(highpass(noise(len(t), rng), 300), 800, 3500) * shape, peak_db=-6.0)


def explosion(seed):
    return explosion_near(seed)


def crackles(rng, seconds, per_second, loudness=(0.2, 1.0), wrap=False):
    n = int(seconds * SR)
    out = np.zeros(n)
    for _ in range(int(seconds * per_second)):
        size = int(rng.uniform(0.002, 0.007) * SR)
        shape = env(size / SR, 0.0002, size / SR / 4)
        pop = bandpass(noise(len(shape), rng), 1500, 9000) * shape
        at = rng.uniform(0, seconds - (0 if wrap else size / SR))
        (place_wrapped if wrap else place)(out, pop, at, gain=rng.uniform(*loudness))
    return out


def flamer_loop(seed=221):
    rng = np.random.default_rng(seed)
    period = 2.0
    n = int(period * SR)
    t = t_axis(period)

    def one(r):
        return np.stack([noise(n, r), crackles(r, period, 40, wrap=True)])

    def post(x):
        roar = lowpass(x[0], 700) * 1.0 + bandpass(x[0], 800, 2600) * 0.35 + highpass(x[0], 5000) * 0.08
        return roar + x[1] * 0.4

    flutter = 0.82 + 0.1 * np.sin(2 * np.pi * 14 / period * t) + 0.08 * np.sin(2 * np.pi * 37 / period * t)
    return finish(periodic(one, period)(rng, post) * flutter, peak_db=-3.0, loop=True)


def flamer_start(seed=222):
    rng = np.random.default_rng(seed)
    length = 0.8
    t = t_axis(length)
    n = len(t)
    pop = sine(110 * np.exp(-t / 0.03) + 60) * env(length, 0.002, 0.05)
    whoosh = sweep_lowpass(noise(n, rng), 400, 4000) * np.clip(t / 0.12, 0, 1) * env(length, 0.0, 0.25)
    return finish(mix(pop, whoosh), peak_db=-3.0)


def flamer_stop(seed=223):
    rng = np.random.default_rng(seed)
    length = 0.6
    hiss = sweep_lowpass(noise(int(length * SR), rng), 3000, 400) * env(length, 0.002, 0.18)
    return finish(hiss, peak_db=-6.0)


def tank_burst(seed=224):
    rng = np.random.default_rng(seed)
    base = explosion_near(seed)
    length = len(base) / SR
    t = t_axis(length)
    whoosh = sweep_lowpass(noise(len(t), rng), 300, 3000) * np.clip(t / 0.08, 0, 1) * env(length, 0.0, 0.7) * 0.9
    roar = lowpass(noise(len(t), rng), 500) * env(length, 0.1, 1.0) * 0.6
    return finish(mix(base, whoosh, roar))


def burning_loop(seed=225):
    rng = np.random.default_rng(seed)
    period = 4.0
    n = int(period * SR)

    def one(r):
        return np.stack([noise(n, r), crackles(r, period, 30, wrap=True)])

    def post(x):
        return lowpass(x[0], 450) * 0.7 + x[1] * 0.8

    breathe = 0.8 + 0.2 * np.sin(2 * np.pi * 3 / period * t_axis(period))
    return finish(periodic(one, period)(rng, post) * breathe, peak_db=-5.0, loop=True)


# ------------------------------------------------------------------- bullets

def snap(seed):
    """A round passing close: the supersonic crack, then a short falling hiss."""
    rng = np.random.default_rng(seed)
    length = 0.3
    t = t_axis(length)
    n = len(t)
    crack = highpass(noise(n, rng), 2500) * env(length, 0.0001, 0.003) * 1.2
    zip_ = sine(3200 * (0.45 ** (t / 0.15))) * env(length, 0.002, 0.05) * 0.25
    air = bandpass(noise(n, rng), 1500, 5000) * env(length, 0.002, 0.04) * 0.4
    return finish(mix(crack, zip_, air), peak_db=-3.0)


def whiz(seed):
    rng = np.random.default_rng(seed)
    length = 0.4
    t = t_axis(length)
    shape = np.exp(-((t - 0.12) / 0.06) ** 2)
    out = sweep_lowpass(bandpass(noise(len(t), rng), 600, 6000), 6000, 900) * shape
    return finish(out, peak_db=-6.0)


def impact_flesh(seed):
    rng = np.random.default_rng(seed)
    length = 0.4
    t = t_axis(length)
    n = len(t)
    thud = sine(95 * np.exp(-t / 0.03) + 50) * env(length, 0.001, 0.05)
    wet = bandpass(noise(n, rng), 250, 1400) * env(length, 0.003, 0.05) * 0.8
    smack = bandpass(noise(n, rng), 1500, 4000) * env(length, 0.0005, 0.008) * 0.5
    return finish(mix(thud, wet, smack))


def gore(seed):
    """Something worse than a hit: a wet burst and a crack of bone."""
    rng = np.random.default_rng(seed)
    length = 0.7
    t = t_axis(length)
    n = len(t)
    out = np.zeros(n)
    for k in range(5):
        at = k * 0.04 + rng.uniform(0, 0.03)
        d = 0.2
        blob = bandpass(noise(int(d * SR), rng), 200, 1200) * env(d, 0.002, 0.04)
        place(out, blob, at, rng.uniform(0.5, 1.0))
    crack = click(rng, 0.02, 1500, 6000)
    place(out, crack, 0.02, 0.9)
    place(out, click(rng, 0.015, 2000, 7000), 0.05, 0.6)
    thud = sine(70 * np.exp(-t / 0.05) + 40) * env(length, 0.002, 0.08)
    return finish(mix(out, thud))


def impact_sandbag(seed):
    rng = np.random.default_rng(seed)
    length = 0.3
    t = t_axis(length)
    thud = sine(120 * np.exp(-t / 0.02) + 60) * env(length, 0.001, 0.035)
    grit = lowpass(noise(len(t), rng), 2500) * env(length, 0.001, 0.03) * 0.6
    return finish(mix(thud, grit), peak_db=-3.0)


def impact_wood(seed):
    rng = np.random.default_rng(seed)
    length = 0.3
    t = t_axis(length)
    knock = bandpass(noise(len(t), rng), 500, 1100) * env(length, 0.0005, 0.03)
    tone = sine(np.full(len(t), 420.0)) * env(length, 0.001, 0.04) * 0.5
    return finish(mix(knock, tone, click(rng, 0.01, 2500, 7000) * 0.5), peak_db=-3.0)


def bottle_break(seed=231):
    rng = np.random.default_rng(seed)
    length = 0.9
    t = t_axis(length)
    out = np.zeros(len(t))
    for f in rng.uniform(2200, 7000, 10):
        out += sine(np.full(len(t), f)) * env(length, 0.0005, rng.uniform(0.03, 0.15)) * rng.uniform(0.1, 0.3)
    for _ in range(40):
        place(out, click(rng, 0.006, 3000, 9000), rng.uniform(0, 0.5) ** 1.5, rng.uniform(0.1, 0.5))
    return finish(out, peak_db=-3.0)


def body_fall(seed):
    rng = np.random.default_rng(seed)
    length = 0.5
    t = t_axis(length)
    thud = sine(65 * np.exp(-t / 0.05) + 35) * env(length, 0.004, 0.08)
    mud = lowpass(noise(len(t), rng), 700) * env(length, 0.01, 0.1) * 0.7
    kit = click(rng, 0.02, 1500, 4000) * 0.3
    return finish(mix(thud, mud, np.concatenate([np.zeros(int(0.06 * SR)), kit])))


def drag_loop(seed=232):
    rng = np.random.default_rng(seed)
    period = 2.0
    n = int(period * SR)
    t = t_axis(period)
    make = periodic(lambda r: noise(n, r), period)
    scrape = make(rng, lambda x: lowpass(x, 600) * 0.8 + bandpass(x, 700, 1800) * 0.2)
    tug = 0.5 + 0.5 * np.sin(2 * np.pi * 2 / period * t) ** 2
    return finish(scrape * tug, peak_db=-8.0, loop=True)


def bandage(seed=233):
    rng = np.random.default_rng(seed)
    length = 1.2
    t = t_axis(length)
    out = np.zeros(len(t))
    for k in range(4):
        d = 0.18
        r = noise(int(d * SR), rng) * np.sin(np.pi * t_axis(d) / d) ** 2
        place(out, highpass(r, 1500), 0.05 + k * 0.27, 0.6)
    return finish(out, peak_db=-8.0)


# -------------------------------------------------------------------- voices

VOWELS = {
    "a": [(800, 1.0), (1200, 0.6), (2500, 0.25)],
    "o": [(500, 1.0), (900, 0.5), (2400, 0.15)],
    "e": [(550, 1.0), (1800, 0.5), (2550, 0.25)],
    "u": [(350, 1.0), (750, 0.35), (2300, 0.1)],
    "i": [(300, 1.0), (2200, 0.4), (3000, 0.25)],
}


def voice(rng, f0, vowels, loud, rasp=0.2, breath=0.15):
    """
    A buzz at pitch `f0` (per sample) shaped by formants. `vowels` is a list
    of (vowel, weight array) pairs that cross-fade; `loud` is the envelope.
    """
    n = len(f0)
    jitter = lowpass(noise(n, rng), 30) * 0.03
    buzz = lowpass(saw(f0 * (1 + jitter)), 4000)
    grit = 1.0 + rasp * lowpass(noise(n, rng), 120) * 3.0
    src = buzz * grit + noise(n, rng) * breath
    out = np.zeros(n)
    for name, weight in vowels:
        shaped = np.zeros(n)
        for f, g in VOWELS[name]:
            bw = max(80, f * 0.12)
            shaped += bandpass(src, f - bw, f + bw) * g
        out += shaped * weight
    return out * loud


def scream(seed, length=1.5, high=520, low=300):
    rng = np.random.default_rng(seed)
    t = t_axis(length)
    n = len(t)
    rise = np.clip(t / 0.15, 0, 1)
    f0 = (low + (high - low) * rise * np.exp(-t / (length * 0.8))) * (1 + 0.03 * np.sin(2 * np.pi * 6.5 * t))
    w = np.clip(t / length, 0, 1)
    loud = np.clip(t / 0.05, 0, 1) * np.clip((length - t) / 0.4, 0, 1) * (0.8 + 0.2 * np.sin(2 * np.pi * 3 * t))
    out = voice(rng, f0, [("a", 1 - w * 0.6), ("o", w * 0.6)], loud, rasp=0.45, breath=0.2)
    return finish(crush(out, bits=10, hold_rate=24000), peak_db=-3.0)


def groan(seed, length=1.8, pitch=115):
    rng = np.random.default_rng(seed)
    t = t_axis(length)
    f0 = pitch * (1 - 0.18 * t / length) * (1 + 0.02 * np.sin(2 * np.pi * 4 * t))
    loud = np.sin(np.pi * np.clip(t / length, 0, 1)) ** 0.8
    out = voice(rng, f0, [("o", np.ones(len(t)) * 0.7), ("u", np.ones(len(t)) * 0.3)], loud, rasp=0.3, breath=0.4)
    return finish(crush(out, bits=10, hold_rate=22050), peak_db=-6.0)


def medic_call(seed=251):
    """'Me-dic!': two syllables, the second held and breaking."""
    rng = np.random.default_rng(seed)
    out = np.zeros(int(1.2 * SR))
    for at, d, v, f in [(0.0, 0.2, "e", 300), (0.24, 0.7, "i", 340)]:
        t = t_axis(d)
        f0 = np.full(len(t), float(f)) * (1 - 0.1 * t / d) * (1 + 0.04 * np.sin(2 * np.pi * 7 * t))
        loud = np.clip(t / 0.03, 0, 1) * np.clip((d - t) / 0.15, 0, 1)
        place(out, voice(rng, f0, [(v, np.ones(len(t)))], loud, rasp=0.4, breath=0.25), at)
    place(out, highpass(noise(int(0.05 * SR), rng), 3000) * env(0.05, 0.001, 0.01) * 0.4, 0.21)     # the 'd'
    place(out, highpass(noise(int(0.06 * SR), rng), 2500) * env(0.06, 0.002, 0.02) * 0.5, 0.93)     # the 'c'
    return finish(crush(out, bits=10, hold_rate=24000), peak_db=-3.0)


def bark(seed, syllables=3, pitch=150):
    """Gruff shouted gibberish for orders: rough syllables with hard starts."""
    rng = np.random.default_rng(seed)
    out = np.zeros(int((syllables * 0.17 + 0.3) * SR))
    for k in range(syllables):
        d = rng.uniform(0.1, 0.18) if k < syllables - 1 else 0.28
        at = k * 0.17
        t = t_axis(d)
        f0 = np.full(len(t), pitch * rng.uniform(0.9, 1.2)) * (1 - 0.15 * t / d)
        loud = np.clip(t / 0.01, 0, 1) * np.clip((d - t) / 0.05, 0, 1)
        v = rng.choice(list(VOWELS.keys()))
        place(out, voice(rng, f0, [(v, np.ones(len(t)))], loud, rasp=0.5, breath=0.2), at)
        place(out, highpass(noise(int(0.03 * SR), rng), 2000) * env(0.03, 0.001, 0.008) * 0.6, at)
    return finish(crush(out, bits=10, hold_rate=22050), peak_db=-4.0)


def whistle(seed=261):
    """The officer's whistle: two blasts, the pea trilling inside."""
    rng = np.random.default_rng(seed)
    out = np.zeros(int(1.8 * SR))
    for at, d in [(0.0, 0.45), (0.55, 1.0)]:
        t = t_axis(d)
        trill = np.sign(np.sin(2 * np.pi * 27 * t))
        f = 2900 * (1 + 0.035 * trill) * (1 + 0.004 * np.sin(2 * np.pi * 5 * t))
        tone = sine(f) * (0.75 + 0.25 * trill)
        air = highpass(noise(len(t), rng), 4000) * 0.12
        shape = np.clip(t / 0.02, 0, 1) * np.clip((d - t) / 0.06, 0, 1)
        place(out, (tone + air) * shape, at)
    return finish(echo(out, [0.3, 0.7], [0.18, 0.07], cutoff=2500), peak_db=-4.0)


# --------------------------------------------------------------- the front

def tinnitus():
    length = 4.0
    t = t_axis(length)
    tone = sine(np.full(len(t), 4100.0)) + 0.3 * sine(np.full(len(t), 4170.0))
    shape = np.clip(t / 0.05, 0, 1) * np.exp(-t / 1.3)
    return finish(tone * shape, peak_db=-12.0)


def heartbeat():
    length = 0.8
    out = np.zeros(int(length * SR))
    for at, g in [(0.0, 1.0), (0.2, 0.7)]:
        d = length - at
        tt = t_axis(d)
        place(out, sine(55 * np.exp(-tt / 0.05) + 35) * env(d, 0.004, 0.07) * g, at)
    return finish(lowpass(out, 280), peak_db=-6.0)


def front_loop(seed=271):
    """
    The war somewhere else: wind over mud, guns rumbling on the horizon,
    and now and then a machine gun or a rifle a few fields away.
    """
    rng = np.random.default_rng(seed)
    period = 40.0
    n = int(period * SR)
    t = t_axis(period)

    booms = [artillery_far(31 + k) for k in range(4)]
    bursts = [mg_burst(301 + k, int(rng.integers(3, 8))) for k in range(3)]
    pops = [rifle(311 + k) for k in range(3)]

    def one(r):
        wind = noise(n, r)
        far = np.zeros(n)
        for k in range(9):
            place_wrapped(far, booms[k % 4], k * period / 9 + r.uniform(-1.5, 1.5), r.uniform(0.35, 0.8))
        near = np.zeros(n)
        for k in range(5):
            place_wrapped(near, bursts[k % 3], r.uniform(0, period), r.uniform(0.15, 0.3))
        for k in range(14):
            place_wrapped(near, pops[k % 3], r.uniform(0, period), r.uniform(0.08, 0.2))
        return np.stack([wind, far, near])

    def post(x):
        return lowpass(x[0], 350) * 0.35 + x[1] + lowpass(x[2], 1200)

    gust = 0.8 + 0.2 * np.sin(2 * np.pi * 3 / period * t)
    out = periodic(one, period, warmup_periods=2)(rng, post)
    return finish(out * gust, peak_db=-3.0, loop=True)


# --------------------------------------------------------------------- music
#
# 64 seconds at 60 bpm, eight chords of eight seconds, in D minor. The calm
# layer is a reed-organ drone with a lonely tune over its second half; the
# combat layer adds a low ostinato, timpani and snare rolls that build into
# every chord change. Both are written to the same grid so they line up.

PERIOD = 64.0
CHORDS = [
    [38, 50, 53, 57],       # D minor
    [34, 46, 50, 53],       # B flat
    [31, 43, 50, 55],       # G minor
    [33, 45, 49, 52],       # A major: the dominant pulls back to D
    [38, 50, 53, 57],       # D minor
    [29, 41, 48, 53],       # F major
    [31, 43, 46, 55],       # G minor
    [33, 45, 49, 52],       # A major
]

TUNE = [  # (midi, start, length) over the second half
    (69, 32.0, 2.0), (74, 34.0, 1.0), (72, 35.0, 1.0), (70, 36.0, 3.0), (69, 39.0, 1.0),
    (67, 40.0, 2.0), (69, 42.0, 1.0), (70, 43.0, 1.0), (72, 44.0, 2.5), (69, 46.5, 1.5),
    (65, 48.0, 2.0), (67, 50.0, 1.0), (69, 51.0, 1.0), (72, 52.0, 2.0), (70, 54.0, 2.0),
    (69, 56.0, 3.0), (67, 59.0, 1.0), (61, 60.0, 2.0), (64, 62.0, 2.0),
]


def organ(f, n):
    return (saw(f * 0.997) + saw(f * 1.003) + 0.5 * square(f * 0.5, 0.5)) / 2.5


def music_calm_loop(seed=281):
    rng = np.random.default_rng(seed)
    n = int(PERIOD * SR)

    def one(r):
        pads = np.zeros(n)
        for i, chord in enumerate(CHORDS):
            d = 9.5
            tt = t_axis(d)
            shape = np.clip(tt / 2.5, 0, 1) * np.clip((d - tt) / 2.5, 0, 1)
            for m in chord:
                f = np.full(len(tt), note(m)) * (1 + 0.002 * np.sin(2 * np.pi * r.uniform(0.1, 0.3) * tt))
                place_wrapped(pads, organ(f, len(tt)) * shape * 0.1, i * 8.0)
        tune = np.zeros(n)
        for m, at, d in TUNE:
            tt = t_axis(d + 1.5)
            f = np.full(len(tt), note(m)) * (1 + 0.006 * np.sin(2 * np.pi * 5 * tt) * np.clip(tt / 0.6, 0, 1))
            shape = np.clip(tt / 0.08, 0, 1) * np.exp(-np.maximum(tt - d, 0) / 0.4) * (0.85 + 0.15 * np.exp(-tt / 0.3))
            tone = 0.6 * square(f, 0.3) + 0.4 * triangle(f)
            place_wrapped(tune, tone * shape * 0.11, at)
        bell = np.zeros(n)
        for i, chord in enumerate(CHORDS):
            for k in (0, 4):
                d = 4.0
                tt = t_axis(d)
                f = note(chord[0] + 24)
                tone = sine(np.full(len(tt), f)) + 0.3 * sine(np.full(len(tt), f * 2.76))
                place_wrapped(bell, tone * env(d, 0.002, 1.1) * 0.08, i * 8.0 + k)
        return np.stack([pads, tune, bell])

    def post(x):
        pads = lowpass(x[0], 1100)
        tune = lowpass(x[1], 2600)
        wet = np.roll(tune, int(0.5 * SR)) * 0.3 + np.roll(tune, int(1.0 * SR)) * 0.15
        return pads + tune + lowpass(wet, 1600) + lowpass(x[2], 3000)

    out = periodic(one, PERIOD, warmup_periods=2)(rng, post)
    return finish(crush(out, bits=12, hold_rate=30000), peak_db=-4.0, loop=True)


def timpani(rng, midi, d=1.6):
    tt = t_axis(d)
    f = note(midi) * (1 + 0.15 * np.exp(-tt / 0.03))
    body = sine(f) * env(d, 0.002, 0.45) + 0.4 * sine(f * 1.5) * env(d, 0.002, 0.2)
    skin = lowpass(noise(len(tt), rng), 900) * env(d, 0.001, 0.04) * 0.6
    return body + skin


def snare(rng, g=1.0):
    d = 0.18
    tt = t_axis(d)
    rattle = bandpass(noise(len(tt), rng), 1500, 7000) * env(d, 0.001, 0.05)
    head = sine(np.full(len(tt), 190.0)) * env(d, 0.001, 0.03) * 0.5
    return (rattle + head) * g


def music_combat_loop(seed=282):
    rng = np.random.default_rng(seed)
    n = int(PERIOD * SR)

    def one(r):
        bass = np.zeros(n)
        drums = np.zeros(n)
        brass = np.zeros(n)
        pattern = [1.0, 0.0, 0.6, 0.8, 0.0, 0.6, 0.9, 0.5]          # per eighth note at 120 bpm
        for i, chord in enumerate(CHORDS):
            base = i * 8.0
            root = chord[0]
            for e in range(32):                                        # eighths across the chord
                g = pattern[e % 8]
                if not g:
                    continue
                d = 0.22
                tt = t_axis(d)
                f = np.full(len(tt), note(root + (12 if e % 16 == 14 else 0)))
                tone = 0.6 * saw(f) + 0.4 * square(f * 0.5, 0.5)
                place_wrapped(bass, tone * env(d, 0.003, 0.09) * g * 0.35, base + e * 0.25)
            for b in range(4):                                         # timpani on every bar
                place_wrapped(drums, timpani(r, root + 12 if b % 2 else root + 7), base + b * 2.0, 0.6)
            for k in range(8):                                         # roll building into the change
                place_wrapped(drums, snare(r, 0.15 + 0.04 * k), base + 6.0 + k * 0.125)
            for k in range(16):
                place_wrapped(drums, snare(r, 0.45 + 0.03 * k), base + 7.0 + k * 0.0625)
            d = 3.0
            tt = t_axis(d)
            shape = np.clip(tt / 0.05, 0, 1) * np.exp(-tt / 0.9)
            for m in chord[1:]:
                f = np.full(len(tt), note(m))
                place_wrapped(brass, (saw(f) + saw(f * 1.004)) * shape * 0.07, base)
        return np.stack([bass, drums, brass])

    def post(x):
        return lowpass(x[0], 700) + x[1] + sweep_free_brass(x[2])

    out = periodic(one, PERIOD, warmup_periods=2)(rng, post)
    return finish(crush(out, bits=12, hold_rate=30000), peak_db=-4.0, loop=True)


def sweep_free_brass(x):
    return lowpass(x, 1500) * 0.9


# --------------------------------------------------------------------- menus

def ui_select(seed=291):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.15 * SR))
    place(out, click(rng, 0.02, 1500, 5000), 0.0, 1.0)
    place(out, click(rng, 0.015, 2500, 7000), 0.04, 0.5)
    return finish(out, peak_db=-6.0)


def ui_hover(seed=292):
    rng = np.random.default_rng(seed)
    return finish(click(rng, 0.01, 3000, 8000), peak_db=-14.0)


def paper(seed=293):
    rng = np.random.default_rng(seed)
    length = 0.5
    t = t_axis(length)
    crinkles = np.zeros(len(t))
    for _ in range(35):
        place(crinkles, noise(int(0.008 * SR), rng) * env(0.008, 0.0005, 0.002), rng.uniform(0, 0.45), rng.uniform(0.2, 1.0))
    body = noise(len(t), rng) * 0.25 * np.sin(np.pi * t / length) ** 0.6
    return finish(bandpass(crinkles + body, 1500, 9000), peak_db=-6.0)


# ------------------------------------------------------------------- catalog

SOUNDS = {
    "rifle_1":            lambda: rifle(401),
    "rifle_2":            lambda: rifle(402),
    "rifle_3":            lambda: rifle(403),
    "rifle_far_1":        lambda: rifle_far(404),
    "rifle_far_2":        lambda: rifle_far(405),
    "smg_1":              lambda: smg(406),
    "smg_2":              lambda: smg(407),
    "pistol":             lambda: pistol(408),
    "mg_1":               lambda: mg(409),
    "mg_2":               lambda: mg(410),
    "mg_3":               lambda: mg(411),
    "mg_far_1":           lambda: mg_far(412),
    "mg_far_2":           lambda: mg_far(413),
    "rifle_bolt":         rifle_bolt,
    "rifle_reload":       rifle_reload,
    "smg_reload":         smg_reload,
    "mg_reload":          mg_reload,
    "mg_setup":           mg_setup,
    "overheat":           overheat,
    "click_empty":        click_empty,
    "grenade_pin":        grenade_pin,
    "grenade_bounce":     grenade_bounce,
    "throw":              throw,
    "explosion_1":        lambda: explosion(421),
    "explosion_2":        lambda: explosion(422),
    "explosion_3":        lambda: explosion(423),
    "artillery_far_1":    lambda: artillery_far(424),
    "artillery_far_2":    lambda: artillery_far(425),
    "shell_incoming":     lambda: shell_incoming(426),
    "flamer_loop":        flamer_loop,
    "flamer_start":       flamer_start,
    "flamer_stop":        flamer_stop,
    "tank_burst":         tank_burst,
    "burning_loop":       burning_loop,
    "snap_1":             lambda: snap(431),
    "snap_2":             lambda: snap(432),
    "snap_3":             lambda: snap(433),
    "whiz_1":             lambda: whiz(434),
    "whiz_2":             lambda: whiz(435),
    "ricochet_1":         lambda: ricochet(436, 2400, 1300),
    "ricochet_2":         lambda: ricochet(437, 3100, 1900, 0.6),
    "impact_dirt_1":      lambda: impact_dirt(438),
    "impact_dirt_2":      lambda: impact_dirt(439),
    "impact_flesh_1":     lambda: impact_flesh(440),
    "impact_flesh_2":     lambda: impact_flesh(441),
    "impact_flesh_3":     lambda: impact_flesh(442),
    "gore_1":             lambda: gore(443),
    "gore_2":             lambda: gore(444),
    "impact_sandbag":     lambda: impact_sandbag(445),
    "impact_wood":        lambda: impact_wood(446),
    "bottle_break":       bottle_break,
    "body_fall_1":        lambda: body_fall(447),
    "body_fall_2":        lambda: body_fall(448),
    "footstep_1":         lambda: footstep_mud(449),
    "footstep_2":         lambda: footstep_mud(450),
    "footstep_3":         lambda: footstep_mud(451),
    "drag_loop":          drag_loop,
    "bandage":            bandage,
    "scream_1":           lambda: scream(461, 1.5, 520, 300),
    "scream_2":           lambda: scream(462, 1.1, 600, 380),
    "scream_3":           lambda: scream(463, 2.0, 460, 260),
    "groan_1":            lambda: groan(464, 1.8, 115),
    "groan_2":            lambda: groan(465, 2.4, 100),
    "groan_3":            lambda: groan(466, 1.4, 135),
    "medic_call":         medic_call,
    "bark_1":             lambda: bark(471, 3, 150),
    "bark_2":             lambda: bark(472, 2, 140),
    "bark_3":             lambda: bark(473, 4, 160),
    "bark_4":             lambda: bark(474, 2, 175),
    "whistle":            whistle,
    "tinnitus":           tinnitus,
    "heartbeat":          heartbeat,
    "front_loop":         front_loop,
    "music_calm_loop":    music_calm_loop,
    "music_combat_loop":  music_combat_loop,
    "ui_select":          ui_select,
    "ui_hover":           ui_hover,
    "paper":              paper,
}


def write(path, x, half):
    if half:
        # Resample three copies end to end and keep the middle one, so the
        # filter inside the resampler sees a loop and leaves no seam.
        n = len(x) // 2
        x = sps.resample_poly(np.tile(x, 3), 1, 2)[n:2 * n]
        rate = SR // 2
    else:
        rate = SR
    wavfile.write(path, rate, (np.clip(x, -1, 1) * 32767).astype(np.int16))


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
        write(os.path.join(args.out, name + ".wav"), x, name in HALF_RATE)
        print(f"  {name:18s} {len(x) / SR:5.2f}s")


if __name__ == "__main__":
    main()
