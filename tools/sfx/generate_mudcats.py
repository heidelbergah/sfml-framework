#!/usr/bin/env python3
"""
generate_mudcats.py - sounds and music for the Mud Cats infantry demo

Built from the same pieces as generate_sfx.py, so everything is synthesised
from scratch and yours to use.

The guns are built the way a real report is heard: the click of the action,
the supersonic crack, the blast itself with a low thump under it, and then
the long part, the sound rolling away over the fields and coming back off a
tree line. Everything is then driven a little and bit-crushed, so it stays
chiptune.

The voices are a small formant synthesiser. A glottal pulse (the buzz of
the vocal folds, with the jitter and shimmer and the occasional crack of a
real throat under strain) goes through four moving resonances that make
the vowels. A scream is a throat pushed past what it can do: high, tense,
breaking, rough with period doubling, ending in a gasp for air. A groan is
the same throat with no tension left in it.

    python3 tools/sfx/generate_mudcats.py          # writes assets/mudcats/*.wav
    python3 tools/sfx/generate_mudcats.py --only scream

Files ending in _loop loop seamlessly. There are two pieces of music: a calm
one for the quiet parts, and a combat one, heavier and darker, which the
game fades in when the fighting starts.
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
                          finish, periodic, artillery_far, shell_incoming, ricochet, impact_dirt,
                          footstep_mud, mg_burst, sweep_phase)

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "mudcats")

# Long beds are written at half rate: they are mostly low rumble and pads,
# and it halves the size of the biggest files.
HALF_RATE = {"music_calm_loop", "music_combat_loop", "front_loop", "rain_loop", "dawn_loop", "night_loop"}


def note(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def saw(freqs):
    return 2.0 * (sweep_phase(freqs) % 1.0) - 1.0


def click(rng, length=0.012, lo=2000, hi=7000):
    return bandpass(noise(int(length * SR), rng), lo, hi) * env(length, 0.0002, length / 4)


def drive(x, amount):
    """Soft clipping: louder in the middle, rounded at the top."""
    return np.tanh(x * amount) / np.tanh(amount)


def reverb(x, seconds=0.8, wet=0.25, cutoff=2500, rng=None, predelay=0.012):
    """A diffuse tail: the sound convolved with decaying noise, darker as it goes."""
    rng = rng or np.random.default_rng(7)
    t = t_axis(seconds)
    n = len(t)
    ir = noise(n, rng) * np.exp(-t / (seconds / 5.0))
    ir = lowpass(ir, cutoff)
    ir[: int(predelay * SR)] = 0.0
    ir /= np.sqrt(np.sum(ir ** 2)) or 1.0
    tail = sps.fftconvolve(x, ir)
    dry = np.concatenate([x, np.zeros(len(tail) - len(x))])
    return dry + tail * wet


def contour(t, points):
    """A curve through (time, value) points, in seconds."""
    ts, vs = zip(*points)
    return np.interp(t, ts, vs)


# ------------------------------------------------------------- the voice
#
# Four formants for each vowel (Hz), and their bandwidths. These are a
# man's, pushed up a little: people shouting open their mouths wider, which
# raises the first formant.

VOWELS = {
    "a":  ([850, 1250, 2550, 3500], [100, 110, 150, 220]),
    "ah": ([760, 1150, 2500, 3400], [100, 110, 150, 220]),
    "ae": ([700, 1650, 2450, 3400], [100, 110, 150, 220]),
    "e":  ([560, 1800, 2550, 3500], [90, 110, 150, 220]),
    "i":  ([320, 2250, 3000, 3700], [80, 110, 150, 220]),
    "o":  ([560, 880, 2450, 3400], [90, 100, 150, 220]),
    "u":  ([360, 760, 2350, 3300], [80, 100, 150, 220]),
    "uh": ([640, 1190, 2400, 3400], [100, 110, 150, 220]),
    "m":  ([260, 1150, 2300, 3300], [120, 300, 400, 400]),
}
FORMANT_GAIN = [1.0, 0.55, 0.28, 0.12]
NASAL = {"m"}


def resonator(x, freqs, bws, block=64):
    """A two-pole resonance whose centre and width move over time."""
    out = np.zeros_like(x)
    zi = np.zeros(2)
    n = len(x)
    for i in range(0, n, block):
        f = float(freqs[min(i, n - 1)])
        bw = float(bws[min(i, n - 1)])
        r = np.exp(-np.pi * bw / SR)
        th = 2 * np.pi * min(f, SR * 0.45) / SR
        a = [1.0, -2 * r * np.cos(th), r * r]
        b = [(1 - r) * np.sqrt(max(1e-9, 1 - 2 * r * np.cos(2 * th) + r * r))]
        out[i:i + block], zi = sps.lfilter(b, a, x[i:i + block], zi=zi)
    return out


def glottal(rng, f0, open_q=0.55, jitter=0.012, shimmer=0.08, sub=0.0):
    """
    The buzz of the vocal folds: a Rosenberg pulse per cycle, differentiated
    (that is what the lips do to it). `jitter` wobbles the period, `shimmer`
    the loudness of each cycle, and `sub` makes every other cycle weaker:
    period doubling, the rough edge of a voice under strain.
    """
    n = len(f0)
    if np.ndim(open_q) == 0:
        open_q = np.full(n, float(open_q))
    wobble = 1.0 + jitter * lowpass(noise(n, rng), 70) * 4.0
    phase = np.cumsum(f0 * wobble / SR)
    frac = phase % 1.0
    cycle = np.floor(phase).astype(int)
    cycles = int(cycle.max()) + 2
    amps = np.clip(1.0 + shimmer * rng.standard_normal(cycles), 0.2, 2.0)
    if np.ndim(sub) == 0:
        sub = np.full(n, float(sub))
    odd = (cycle % 2 == 1)
    a = amps[cycle] * np.where(odd, 1.0 - sub, 1.0)
    tp = open_q * 0.65
    tn = open_q * 0.35
    pulse = np.where(frac < tp, 0.5 * (1 - np.cos(np.pi * frac / tp)),
                     np.where(frac < open_q, np.cos(0.5 * np.pi * (frac - tp) / tn), 0.0))
    d = np.diff(pulse, prepend=0.0) * a
    d *= 0.1 * SR / np.maximum(f0, 30.0)     # about 1 at its peak, whatever the pitch
    return d, frac < open_q


def formants(keys, t):
    """Formant tracks from (time, vowel) keyframes, gliding between them."""
    freqs, bws = [], []
    for k in range(4):
        freqs.append(contour(t, [(at, VOWELS[v][0][k]) for at, v in keys]))
        bws.append(contour(t, [(at, VOWELS[v][1][k]) for at, v in keys]))
    nasal = contour(t, [(at, 1.0 if v in NASAL else 0.0) for at, v in keys])
    return freqs, bws, nasal


def voice(rng, f0, keys, loud, open_q=0.55, breath=0.08, jitter=0.012, shimmer=0.08, sub=0.0, strain=1.0):
    """
    One utterance. `f0` is the pitch per sample, `keys` the vowels as
    (time, vowel) keyframes, `loud` the envelope. `strain` drives the result:
    a voice past its limit distorts.
    """
    n = len(f0)
    t = np.arange(n) / SR
    buzz, open_ = glottal(rng, f0, open_q, jitter, shimmer, sub)
    # Breath: some all the time, more while the folds are open.
    air = highpass(noise(n, rng), 400) * (0.35 + 0.65 * open_) * breath
    src = buzz + air
    freqs, bws, nasal = formants(keys, t)
    out = np.zeros(n)
    for k in range(4):
        out += resonator(src, freqs[k], bws[k]) * FORMANT_GAIN[k]
    # A closed mouth: most of the sound is gone, and what is left is low.
    out = out * (1 - 0.7 * nasal) + lowpass(out, 500) * 0.7 * nasal
    out = out * loud
    if strain > 1.0:
        peak = np.max(np.abs(out)) or 1.0
        out = drive(out / peak, strain) * peak
    return out


def gasp(rng, length=0.3, gain=0.5, vowel="ah"):
    """A breath dragged in: noise through an open throat, rising and cut off."""
    t = t_axis(length)
    n = len(t)
    src = highpass(noise(n, rng), 600)
    freqs, bws, _ = formants([(0, vowel), (length, "uh")], t)
    out = sum(resonator(src, freqs[k], np.array(bws[k]) * 1.6) * FORMANT_GAIN[k] for k in range(3))
    shape = np.clip(t / (length * 0.7), 0, 1) ** 1.5 * np.clip((length - t) / 0.03, 0, 1)
    return out * shape * gain


def consonant(rng, kind, gain=0.6):
    """The noise of a consonant: a plosive burst or a hiss."""
    if kind in ("k", "t", "p", "d"):
        lo, hi, d = {"k": (1500, 4000, 0.03), "t": (3000, 8000, 0.025), "p": (300, 1500, 0.02), "d": (2500, 6000, 0.018)}[kind]
        return bandpass(noise(int(d * SR), rng), lo, hi) * env(d, 0.0005, d / 4) * gain
    if kind in ("s", "sh", "h"):
        lo, hi, d = {"s": (4000, 9000, 0.09), "sh": (1800, 5000, 0.1), "h": (600, 3000, 0.07)}[kind]
        tt = t_axis(d)
        return bandpass(noise(len(tt), rng), lo, hi) * np.sin(np.pi * tt / d) * gain
    return np.zeros(1)


def chip(x, bits=10, rate=24000):
    """The game's sound: a little grit on everything."""
    return crush(x, bits=bits, hold_rate=rate)


# ------------------------------------------------------------------- weapons

def report(rng, crack=1.0, blast_cut=3800, blast_decay=0.028, thump_hz=115, thump=0.8,
           tail=0.45, tail_gain=0.35, tail_cut=2200, slap_at=0.24, slap=0.16, mech=0.25,
           heat=2.2, length=1.6):
    """
    One shot, in layers:

      mech    the firing pin and the action, a tiny dry click first
      crack   the round going supersonic: very short, very bright
      blast   the muzzle: a hard burst of noise that dies fast
      thump   the low punch under it, falling in pitch
      tail    the report rolling away across open ground, darker as it goes
      slap    the same coming back off a tree line or a wall
    """
    t = t_axis(length)
    n = len(t)
    out = np.zeros(n)
    out += bandpass(noise(n, rng), 2500, 9000) * env(length, 0.0001, 0.0008) * mech
    out += highpass(noise(n, rng), 3500) * env(length, 0.0001, 0.0025) * crack
    out += lowpass(noise(n, rng), blast_cut) * env(length, 0.0003, blast_decay, 1.4)
    f = thump_hz * np.exp(-t / 0.035) + 42
    out += sine(f) * env(length, 0.0005, 0.065) * thump
    # The tail starts a beat after the blast and rolls: uneven, like the
    # sound coming back off many things at once.
    start = int(0.012 * SR)
    roll = np.clip(1.0 + 0.45 * lowpass(noise(n, rng), 9) * 6.0, 0.2, 2.0)
    body = sweep_lowpass(noise(n, rng), tail_cut, 220) * np.exp(-t / tail) * roll
    body[:start] = 0.0
    body[start:] *= np.clip(np.arange(n - start) / (0.03 * SR), 0, 1)
    out += body * tail_gain
    # One clear reflection.
    blast = lowpass(noise(int(0.12 * SR), rng), 900) * env(0.12, 0.002, 0.03)
    place(out, blast, slap_at, slap)
    return drive(out, heat)


def rifle(seed):
    rng = np.random.default_rng(seed)
    return finish(chip(report(rng, crack=1.1, blast_cut=4200, blast_decay=0.03, thump_hz=120, thump=0.85,
                              tail=0.5, tail_gain=0.38, slap_at=rng.uniform(0.22, 0.32), slap=0.18, length=1.8), 10, 32000))


def rifle_far(seed):
    rng = np.random.default_rng(seed)
    # Far off: no crack, the blast dulled, and much more of the tail.
    x = report(rng, crack=0.0, blast_cut=1100, blast_decay=0.06, thump_hz=80, thump=0.6, tail=0.8,
               tail_gain=0.7, tail_cut=900, slap_at=rng.uniform(0.4, 0.7), slap=0.3, mech=0.0, heat=1.2, length=2.2)
    return finish(lowpass(x, 1400), peak_db=-6.0)


def smg(seed):
    rng = np.random.default_rng(seed)
    # A pistol cartridge: a flatter pop, less crack, a short tail.
    return finish(chip(report(rng, crack=0.35, blast_cut=2800, blast_decay=0.018, thump_hz=150, thump=0.6,
                              tail=0.22, tail_gain=0.26, tail_cut=1800, slap_at=0.16, slap=0.1, mech=0.4,
                              heat=2.6, length=0.7), 10, 30000))


def smg_far(seed):
    rng = np.random.default_rng(seed)
    x = report(rng, crack=0.0, blast_cut=900, blast_decay=0.03, thump_hz=100, thump=0.5, tail=0.45,
               tail_gain=0.55, tail_cut=800, slap_at=0.35, slap=0.25, mech=0.0, heat=1.2, length=1.3)
    return finish(lowpass(x, 1200), peak_db=-8.0)


def pistol(seed):
    rng = np.random.default_rng(seed)
    return finish(chip(report(rng, crack=0.55, blast_cut=3300, blast_decay=0.014, thump_hz=170, thump=0.5,
                              tail=0.28, tail_gain=0.28, tail_cut=2000, slap_at=0.2, slap=0.12, mech=0.5,
                              heat=2.4, length=0.8), 10, 30000))


def mg(seed):
    rng = np.random.default_rng(seed)
    # A rifle round, fired from something heavy: tighter, a mechanical
    # clack of the bolt going back, a short tail (the next shot covers it).
    x = report(rng, crack=0.95, blast_cut=3800, blast_decay=0.024, thump_hz=105, thump=0.95,
               tail=0.32, tail_gain=0.3, slap_at=0.2, slap=0.12, mech=0.2, heat=2.6, length=0.9)
    clack = click(rng, 0.014, 1200, 4500) * 0.35
    place(x, clack, 0.042)
    return finish(chip(x, 10, 30000))


def mg_far(seed):
    rng = np.random.default_rng(seed)
    x = report(rng, crack=0.0, blast_cut=1000, blast_decay=0.04, thump_hz=75, thump=0.7, tail=0.5,
               tail_gain=0.5, tail_cut=800, slap_at=0.45, slap=0.2, mech=0.0, heat=1.3, length=1.2)
    return finish(lowpass(x, 1100), peak_db=-6.0)


def explosion(seed):
    """A shell or a grenade, close: crack, blast, a punch in the chest, then earth coming down."""
    rng = np.random.default_rng(seed)
    length = 3.2
    t = t_axis(length)
    n = len(t)
    crack = highpass(noise(n, rng), 2000) * env(length, 0.0002, 0.004) * 0.9
    blast = sweep_lowpass(noise(n, rng), 6000, 160) * env(length, 0.0008, 0.22, 1.2) * 1.3
    f = 75 * np.exp(-t / 0.18) + 26
    punch = sine(f) * env(length, 0.002, 0.42) * 1.7
    roll = np.clip(1.0 + 0.5 * lowpass(noise(n, rng), 6) * 6.0, 0.2, 2.0)
    rumble = lowpass(noise(n, rng), 280) * env(length, 0.04, 0.9) * roll * 0.8
    debris = np.zeros(n)
    for _ in range(220):
        at = 0.12 + rng.uniform(0, 1) ** 1.6 * 2.3
        size = rng.uniform(0.004, 0.02)
        bit = bandpass(noise(int(size * SR), rng), 700, 4500) * env(size, 0.0003, size / 4)
        place(debris, bit, at, gain=rng.uniform(0.04, 0.22) * max(0.08, 1 - at / 2.6))
    out = drive(mix(crack, blast, punch, rumble, lowpass(debris, 3500)), 2.4)
    return finish(chip(out, 10, 28000))


def explosion_far(seed):
    rng = np.random.default_rng(seed)
    length = 3.6
    t = t_axis(length)
    n = len(t)
    roll = np.clip(1.0 + 0.5 * lowpass(noise(n, rng), 5) * 6.0, 0.2, 2.0)
    boom = lowpass(noise(n, rng), 320) * env(length, 0.02, 0.7) * roll
    sub = sine(55 * np.exp(-t / 0.3) + 24) * env(length, 0.01, 0.6) * 0.9
    out = echo(mix(boom, sub), [0.6, 1.3], [0.35, 0.18], cutoff=200)
    return finish(out, peak_db=-4.0)


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


# ----------------------------------------------------------------- grenades

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


# ------------------------------------------------------------ kit, close in

def pistol_reload(seed=208):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(1.0 * SR))
    # Magazine catch, the empty one sliding out, the full one slapped home,
    # the slide pulled back and let go.
    for at, lo, hi, g, d in [(0.0, 2000, 6000, 0.6, 0.012), (0.05, 800, 3000, 0.4, 0.05),
                             (0.45, 900, 3500, 1.0, 0.02), (0.72, 1500, 5000, 0.7, 0.018), (0.8, 1200, 4500, 0.9, 0.02)]:
        place(out, click(rng, d, lo, hi), at, g)
    return finish(out, peak_db=-5.0)


def casing(seed):
    """Brass on the ground: a bright tink, and a smaller one as it bounces."""
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.35 * SR))
    for at, g in [(0.0, 1.0), (rng.uniform(0.09, 0.14), 0.45), (rng.uniform(0.19, 0.24), 0.2)]:
        d = 0.08
        tt = t_axis(d)
        ring = sum(sine(np.full(len(tt), f * rng.uniform(0.97, 1.03))) * env(d, 0.0003, dk) * gg
                   for f, dk, gg in [(4800, 0.03, 1.0), (7100, 0.018, 0.6), (9300, 0.01, 0.4)])
        place(out, ring, at, g)
    return finish(out, peak_db=-8.0)


def weapon_pickup(seed=209):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.7 * SR))
    swish = bandpass(noise(int(0.2 * SR), rng), 1200, 5000) * np.sin(np.pi * t_axis(0.2) / 0.2) ** 2 * 0.4
    place(out, swish, 0.0)
    place(out, click(rng, 0.03, 600, 2200), 0.16, 0.9)          # wood on the palm
    place(out, click(rng, 0.016, 2000, 6000), 0.3, 0.7)         # sling swivel
    place(out, click(rng, 0.02, 1500, 5000), 0.46, 0.8)         # bolt checked
    return finish(out, peak_db=-5.0)


def ammo_pickup(seed=210):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.6 * SR))
    for k in range(7):
        place(out, click(rng, 0.008, 3000, 8000), 0.04 + k * 0.045 + rng.uniform(0, 0.02), rng.uniform(0.3, 0.7))
    place(out, click(rng, 0.03, 500, 1800), 0.42, 0.8)          # pouch flap
    return finish(out, peak_db=-6.0)


def snap_heavy(seed):
    """A machine-gun round going past: a harder crack, and the air tearing."""
    rng = np.random.default_rng(seed)
    length = 0.35
    t = t_axis(length)
    n = len(t)
    crack = highpass(noise(n, rng), 1800) * env(length, 0.0001, 0.004) * 1.4
    tear = bandpass(noise(n, rng), 800, 4000) * env(length, 0.001, 0.05) * 0.5
    zip_ = sine(2600 * (0.4 ** (t / 0.15))) * env(length, 0.002, 0.06) * 0.25
    return finish(drive(mix(crack, tear, zip_), 1.8), peak_db=-2.0)


def impact_leaves(seed):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.3 * SR))
    for _ in range(18):
        place(out, click(rng, 0.006, 2000, 7000), rng.uniform(0, 0.15) ** 1.3, rng.uniform(0.2, 0.6))
    place(out, click(rng, 0.012, 900, 2600), 0.0, 0.9)          # a twig snapping
    return finish(out, peak_db=-6.0)


def splash(seed):
    rng = np.random.default_rng(seed)
    length = 0.5
    t = t_axis(length)
    n = len(t)
    out = lowpass(noise(n, rng), 1800) * env(length, 0.002, 0.07)
    for _ in range(7):
        d = rng.uniform(0.02, 0.05)
        tt = t_axis(d)
        f0 = rng.uniform(400, 1100)
        bubble = sine(f0 * (1 + 1.5 * tt / d)) * env(d, 0.001, d / 2)
        place(out, bubble, rng.uniform(0.02, 0.3), rng.uniform(0.15, 0.35))
    return finish(out, peak_db=-5.0)


def wade(seed):
    rng = np.random.default_rng(seed)
    length = 0.45
    t = t_axis(length)
    n = len(t)
    push = lowpass(noise(n, rng), 1100) * np.sin(np.pi * np.clip(t / 0.3, 0, 1)) ** 2
    out = push * 0.8
    for _ in range(4):
        d = 0.03
        tt = t_axis(d)
        place(out, sine(rng.uniform(300, 700) * (1 + tt / d)) * env(d, 0.001, 0.012), rng.uniform(0.05, 0.3), 0.25)
    return finish(out, peak_db=-6.0)


def footstep_grass(seed):
    rng = np.random.default_rng(seed)
    length = 0.25
    t = t_axis(length)
    n = len(t)
    swish = bandpass(noise(n, rng), 1500, 6000) * env(length, 0.01, 0.04) * 0.5
    crunch = np.zeros(n)
    for _ in range(6):
        place(crunch, click(rng, 0.005, 2000, 6000), rng.uniform(0.0, 0.05), rng.uniform(0.2, 0.5))
    thud = sine(np.full(n, 90.0)) * env(length, 0.004, 0.03) * 0.5
    return finish(mix(swish, crunch, thud), peak_db=-6.0)


def bayonet_swish(seed=214):
    rng = np.random.default_rng(seed)
    length = 0.22
    t = t_axis(length)
    shape = np.sin(np.pi * t / length) ** 3
    out = sweep_lowpass(highpass(noise(len(t), rng), 500), 1500, 5000) * shape
    return finish(out, peak_db=-6.0)


def stab(seed):
    """Steel going in: a thud, cloth tearing, and a wet sound that is worse than both."""
    rng = np.random.default_rng(seed)
    length = 0.6
    t = t_axis(length)
    n = len(t)
    thud = sine(95 * np.exp(-t / 0.03) + 48) * env(length, 0.001, 0.06)
    tear = highpass(noise(n, rng), 2500) * env(length, 0.0005, 0.02) * 0.6
    wet = bandpass(noise(n, rng), 250, 1100) * env(length, 0.01, 0.09) * (1 + 0.6 * np.sin(2 * np.pi * 38 * t)) * 0.9
    out = mix(thud, tear, wet)
    place(out, click(rng, 0.012, 1500, 5000), 0.03, 0.4)        # gristle
    # And pulled back out.
    place(out, bandpass(noise(int(0.12 * SR), rng), 300, 1500) * env(0.12, 0.02, 0.04) * 0.5, 0.28)
    return finish(drive(out, 1.6))


def club_hit(seed=215):
    rng = np.random.default_rng(seed)
    length = 0.4
    t = t_axis(length)
    n = len(t)
    thud = sine(110 * np.exp(-t / 0.025) + 55) * env(length, 0.001, 0.05)
    knock = bandpass(noise(n, rng), 400, 1200) * env(length, 0.0005, 0.03) * 0.8
    return finish(drive(mix(thud, knock), 1.5))


def smoke_pop(seed=216):
    rng = np.random.default_rng(seed)
    length = 0.5
    t = t_axis(length)
    n = len(t)
    pop = sine(170 * np.exp(-t / 0.03) + 70) * env(length, 0.001, 0.05)
    puff = lowpass(noise(n, rng), 1400) * env(length, 0.003, 0.09) * 0.8
    return finish(mix(pop, puff), peak_db=-4.0)


def smoke_hiss_loop(seed=217):
    rng = np.random.default_rng(seed)
    period = 3.0
    n = int(period * SR)
    t = t_axis(period)

    def post(x):
        return highpass(x, 2200) * 0.6 + bandpass(x, 600, 1500) * 0.3

    sputter = 0.75 + 0.15 * np.sin(2 * np.pi * 11 / period * t) + 0.1 * np.sin(2 * np.pi * 29 / period * t)
    return finish(periodic(lambda r: noise(n, r), period)(rng, post) * sputter, peak_db=-8.0, loop=True)


def flare_fire(seed=218):
    """The flare pistol: a hollow thoonk, and the star hissing up out of sight."""
    rng = np.random.default_rng(seed)
    length = 1.8
    t = t_axis(length)
    n = len(t)
    thoonk = sine(150 * np.exp(-t / 0.04) + 60) * env(length, 0.001, 0.07)
    blast = lowpass(noise(n, rng), 1600) * env(length, 0.0005, 0.03) * 0.8
    rise = np.clip((t - 0.05) / 0.1, 0, 1) * np.exp(-np.maximum(t - 0.15, 0) / 0.5)
    hiss = sweep_lowpass(bandpass(noise(n, rng), 1200, 7000), 1500, 6000) * rise * 0.35
    return finish(mix(thoonk, blast, hiss), peak_db=-3.0)


def flare_pop(seed=219):
    rng = np.random.default_rng(seed)
    length = 1.6
    t = t_axis(length)
    n = len(t)
    pop = lowpass(noise(n, rng), 2500) * env(length, 0.001, 0.02)
    fizz = bandpass(noise(n, rng), 2500, 8000) * env(length, 0.05, 0.6) * 0.25
    out = mix(pop, fizz)
    for _ in range(40):
        place(out, click(rng, 0.004, 3000, 9000), rng.uniform(0.05, 1.3), rng.uniform(0.05, 0.2))
    return finish(out, peak_db=-6.0)


# ------------------------------------------------------------------- fire

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
    base = explosion(seed)
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

def cry(rng, length=1.2, start=260, peak=560, end=190, vowel="a", rough=0.35, crack_at=None, strain=2.2):
    """
    A man hit: the breath forced out of him, the voice jumping up past where
    it can hold, wavering, breaking, then sliding down into a rasp as the
    air runs out.
    """
    t = t_axis(length)
    n = len(t)
    f0 = contour(t, [(0.0, start), (0.05, peak * 1.06), (0.12, peak), (length * 0.55, peak * 0.86),
                     (length * 0.82, peak * 0.62), (length, end)])
    # Unsteady: a slow wobble and a faster, irregular tremor.
    wobble = 0.03 * np.sin(2 * np.pi * 5.3 * t + 1.8 * np.sin(2 * np.pi * 0.9 * t))
    tremor = lowpass(noise(n, rng), 14) * 0.25
    f0 = f0 * (1 + wobble + tremor)
    # The voice cracks: a sudden jump up and back.
    at = crack_at if crack_at is not None else rng.uniform(0.25, 0.55) * length
    f0 *= 1 + 0.3 * np.exp(-((t - at) / 0.04) ** 2)
    loud = np.clip(t / 0.025, 0, 1) * np.clip((length - t) / (length * 0.35), 0, 1) ** 0.8
    loud *= 0.85 + 0.15 * np.sin(2 * np.pi * 6.5 * t)
    sub = contour(t, [(0, rough * 0.6), (0.15, rough), (length * 0.7, rough * 0.8), (length, rough + 0.3)])
    tense = contour(t, [(0, 0.45), (0.1, 0.38), (length * 0.7, 0.45), (length, 0.65)])
    keys = [(0.0, "ah"), (0.08, vowel), (length * 0.7, vowel), (length * 0.92, "uh"), (length, "uh")]
    return voice(rng, f0, keys, loud, open_q=tense, breath=0.12, jitter=0.02, shimmer=0.14, sub=sub, strain=strain)


def scream(seed, length=1.2, peak=560, then_gasp=True, again=False):
    rng = np.random.default_rng(seed)
    parts = [cry(rng, length, start=rng.uniform(220, 300), peak=peak, end=rng.uniform(170, 220),
                 vowel=rng.choice(["a", "ae", "ah"]))]
    if then_gasp:
        parts.append(gasp(rng, rng.uniform(0.25, 0.4), 0.45))
    if again:
        parts.append(cry(rng, length * 0.7, start=240, peak=peak * 0.92, end=180, vowel="ah", rough=0.45))
        parts.append(gasp(rng, 0.3, 0.35))
    out = np.concatenate(parts)
    return finish(chip(reverb(out, 0.5, 0.12), 10, 24000), peak_db=-2.0)


def scream_burn(seed):
    """Burning: high, past screaming, broken by desperate breaths."""
    rng = np.random.default_rng(seed)
    parts = []
    for k in range(3):
        parts.append(cry(rng, rng.uniform(0.9, 1.4), start=300, peak=rng.uniform(680, 820), end=260,
                         vowel=rng.choice(["a", "ae"]), rough=0.5, strain=3.0))
        parts.append(gasp(rng, rng.uniform(0.15, 0.25), 0.55, "ae"))
    out = np.concatenate(parts)
    return finish(chip(reverb(out, 0.5, 0.12), 10, 24000), peak_db=-2.0)


def groan(seed, length=1.8, pitch=115):
    """Low, slack and creaking: 'uhhh... nnnh'."""
    rng = np.random.default_rng(seed)
    t = t_axis(length)
    n = len(t)
    f0 = pitch * contour(t, [(0, 0.95), (0.3, 1.08), (length * 0.7, 0.92), (length, 0.75)])
    f0 = f0 * (1 + lowpass(noise(n, rng), 8) * 0.4)
    loud = np.sin(np.pi * np.clip(t / length, 0, 1)) ** 0.7
    keys = [(0, "uh"), (length * 0.45, "o"), (length * 0.75, "m"), (length, "m")]
    sub = contour(t, [(0, 0.25), (length * 0.6, 0.2), (length, 0.6)])        # creak at the end
    out = voice(rng, f0, keys, loud, open_q=0.72, breath=0.3, jitter=0.03, shimmer=0.18, sub=sub, strain=1.3)
    out = np.concatenate([out, gasp(rng, 0.35, 0.3, "uh")])
    return finish(chip(reverb(out, 0.4, 0.1), 10, 22050), peak_db=-5.0)


def sob(seed):
    """Whimpering, the breath catching: short falling 'uh-uh-uh' with gasps between."""
    rng = np.random.default_rng(seed)
    parts = []
    for k in range(int(rng.integers(3, 6))):
        d = rng.uniform(0.14, 0.26)
        t = t_axis(d)
        p = rng.uniform(260, 360) * (1 - 0.12 * k)
        f0 = p * contour(t, [(0, 1.1), (d * 0.3, 1.0), (d, 0.8)])
        loud = np.clip(t / 0.02, 0, 1) * np.clip((d - t) / 0.06, 0, 1)
        parts.append(voice(rng, f0, [(0, "uh"), (d, "u")], loud, open_q=0.6, breath=0.25, jitter=0.03, shimmer=0.2, sub=0.2))
        parts.append(gasp(rng, rng.uniform(0.08, 0.16), 0.5, "uh"))
    out = np.concatenate(parts)
    return finish(chip(reverb(out, 0.4, 0.1), 10, 22050), peak_db=-6.0)


def death_cry(seed):
    """Cut off: a short 'agh' stopped dead, then the breath going out of him."""
    rng = np.random.default_rng(seed)
    d = rng.uniform(0.25, 0.4)
    t = t_axis(d)
    n = len(t)
    f0 = contour(t, [(0, 280), (0.04, rng.uniform(400, 480)), (d, 300)]) * (1 + lowpass(noise(n, rng), 20) * 0.3)
    loud = np.clip(t / 0.02, 0, 1) * np.clip((d - t) / 0.015, 0, 1)
    cut = voice(rng, f0, [(0, "ah"), (d, "a")], loud, open_q=0.4, breath=0.1, jitter=0.03, sub=0.4, strain=2.5)
    rattle_len = rng.uniform(0.5, 0.8)
    tt = t_axis(rattle_len)
    f1 = np.full(len(tt), rng.uniform(38, 55)) * (1 + lowpass(noise(len(tt), rng), 15) * 0.6)
    rattle = voice(rng, f1, [(0, "uh"), (rattle_len, "uh")], np.exp(-tt / 0.3) * 0.5, open_q=0.3,
                   breath=0.6, jitter=0.08, shimmer=0.4, sub=0.5)
    out = np.concatenate([cut, np.zeros(int(0.05 * SR)), rattle])
    return finish(chip(reverb(out, 0.4, 0.1), 10, 22050), peak_db=-3.0)


def grunt(seed, pitch=135):
    rng = np.random.default_rng(seed)
    d = 0.2
    t = t_axis(d)
    f0 = pitch * contour(t, [(0, 1.15), (d, 0.85)])
    loud = np.clip(t / 0.008, 0, 1) * np.exp(-t / 0.08)
    out = voice(rng, f0, [(0, "uh"), (d, "m")], loud, open_q=0.4, breath=0.3, sub=0.3, strain=2.0)
    out = mix(consonant(rng, "h", 0.4), out)
    return finish(chip(out, 10, 22050), peak_db=-5.0)


def word(rng, syllables, pitch, shout=True):
    """
    A short shouted phrase. `syllables` is a list of (consonant, vowel, length).
    Pitch falls across the phrase and rises at the stressed syllable.
    """
    parts = []
    total = sum(s[2] for s in syllables)
    at = 0.0
    for k, (c, v, d) in enumerate(syllables):
        t = t_axis(d)
        n = len(t)
        p = pitch * (1.0 - 0.18 * at / total) * (1.15 if k == 0 else 1.0)
        f0 = p * contour(t, [(0, 0.95), (d * 0.3, 1.05), (d, 0.9)]) * (1 + lowpass(noise(n, rng), 18) * 0.15)
        loud = np.clip(t / 0.015, 0, 1) * np.clip((d - t) / 0.04, 0, 1)
        nasal = c == "m"
        keys = [(0, "m" if nasal else v), (min(0.05, d * 0.3), v), (d, v)]
        out = voice(rng, f0, keys, loud, open_q=0.42 if shout else 0.6, breath=0.12, jitter=0.015,
                    shimmer=0.1, sub=0.15 if shout else 0.05, strain=2.0 if shout else 1.2)
        if c and not nasal:
            out = mix(consonant(rng, c, 0.6), np.concatenate([np.zeros(int(0.02 * SR)), out]))
        parts.append(out)
        at += d
    return np.concatenate(parts)


def medic_call(seed=251):
    """'Me-dic!', the second syllable held and breaking."""
    rng = np.random.default_rng(seed)
    out = word(rng, [("m", "e", 0.2), ("d", "i", 0.55)], rng.uniform(270, 320))
    out = np.concatenate([out, consonant(rng, "k", 0.7), np.zeros(int(0.1 * SR))])
    return finish(chip(reverb(out, 0.6, 0.15), 10, 24000), peak_db=-3.0)


def bark(seed, syllables=3, pitch=170):
    """Shouted orders, words you can't quite make out: hard consonants, open vowels."""
    rng = np.random.default_rng(seed)
    sy = []
    for k in range(syllables):
        sy.append((rng.choice(["k", "t", "p", "d", "s", "h", "m", ""]), rng.choice(["a", "o", "e", "uh", "ah", "i"]),
                   rng.uniform(0.11, 0.18) if k < syllables - 1 else rng.uniform(0.22, 0.32)))
    out = word(rng, sy, pitch)
    return finish(chip(reverb(out, 0.6, 0.15), 10, 22050), peak_db=-4.0)


def charge_cry(seed=255):
    """'Hurraaah!' from a dozen throats at once."""
    rng = np.random.default_rng(seed)
    length = 2.4
    out = np.zeros(int((length + 0.5) * SR))
    for k in range(10):
        d = rng.uniform(1.4, 2.0)
        t = t_axis(d)
        n = len(t)
        p = rng.uniform(170, 300)
        f0 = p * contour(t, [(0, 0.9), (0.15, 1.0), (0.3, 1.18), (d * 0.8, 1.1), (d, 0.85)])
        f0 = f0 * (1 + lowpass(noise(n, rng), 6) * 0.2)
        loud = np.clip(t / 0.05, 0, 1) * np.clip((d - t) / 0.4, 0, 1)
        keys = [(0, "u"), (0.14, "u"), (0.22, "ah"), (0.3, "a"), (d, "a")]
        v = voice(rng, f0, keys, loud, open_q=0.42, breath=0.15, jitter=0.02, shimmer=0.12, sub=0.2, strain=2.2)
        place(out, v, rng.uniform(0.0, 0.35), rng.uniform(0.5, 1.0))
    return finish(chip(reverb(out, 1.0, 0.3), 10, 22050), peak_db=-3.0)


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
# The quiet music: 64 seconds at 60 bpm, eight chords of eight seconds, in
# D minor. A reed-organ drone, a lonely tune over its second half, and a
# bell marking the chords.

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


# --------------------------------------------------------------- weather

def rain_loop(seed=273):
    """Rain on mud and on water standing in the shell holes."""
    rng = np.random.default_rng(seed)
    period = 12.0
    n = int(period * SR)
    t = t_axis(period)

    def one(r):
        hiss = noise(n, r)
        drops = np.zeros(n)
        for _ in range(int(period * 260)):
            place_wrapped(drops, click(r, 0.004, 1500, 7000), r.uniform(0, period), r.uniform(0.03, 0.18))
        plinks = np.zeros(n)
        for _ in range(int(period * 3)):
            d = r.uniform(0.03, 0.06)
            tt = t_axis(d)
            f0 = r.uniform(900, 1900)
            place_wrapped(plinks, sine(f0 * (1 + 0.6 * tt / d)) * env(d, 0.001, d / 3), r.uniform(0, period), r.uniform(0.1, 0.25))
        return np.stack([hiss, drops, plinks])

    def post(x):
        return bandpass(x[0], 1200, 8000) * 0.18 + lowpass(x[0], 500) * 0.25 + x[1] + x[2] * 0.6

    gust = 0.85 + 0.1 * np.sin(2 * np.pi * 2 / period * t) + 0.05 * np.sin(2 * np.pi * 5 / period * t)
    return finish(periodic(one, period, warmup_periods=1)(rng, post) * gust, peak_db=-6.0, loop=True)


def dawn_loop(seed=274):
    """First light on the fields: a little wind, and birds who don't know yet."""
    rng = np.random.default_rng(seed)
    period = 24.0
    n = int(period * SR)

    def chirp(r):
        out = np.zeros(int(0.9 * SR))
        at = 0.0
        base = r.uniform(2800, 4200)
        for _ in range(int(r.integers(3, 8))):
            d = r.uniform(0.03, 0.08)
            tt = t_axis(d)
            f = base * contour(tt, [(0, r.uniform(0.85, 1.0)), (d, r.uniform(1.05, 1.35))])
            tone = sine(f) + 0.2 * sine(f * 2)
            place(out, tone * np.sin(np.pi * tt / d) ** 2, at, r.uniform(0.4, 1.0))
            at += d + r.uniform(0.02, 0.07)
            if at > 0.8:
                break
        return out

    def cuckoo(r):
        out = np.zeros(int(0.8 * SR))
        for at, f in [(0.0, 690.0), (0.28, 560.0)]:
            d = 0.22
            tt = t_axis(d)
            place(out, (sine(np.full(len(tt), f)) + 0.1 * sine(np.full(len(tt), 2 * f))) * np.sin(np.pi * tt / d), at)
        return out

    def one(r):
        wind = noise(n, r)
        birds = np.zeros(n)
        for _ in range(int(period * 1.4)):
            place_wrapped(birds, chirp(r), r.uniform(0, period), r.uniform(0.15, 0.45))
        for at in (3.0, 4.2, 15.0):
            place_wrapped(birds, cuckoo(r), at, 0.18)
        return np.stack([wind, birds])

    def post(x):
        return lowpass(x[0], 400) * 0.35 + reverb_loopless(x[1])

    return finish(periodic(one, period, warmup_periods=1)(rng, post), peak_db=-8.0, loop=True)


def reverb_loopless(x):
    """A little air around the birds, wrapped so the loop stays seamless."""
    out = x.copy()
    for d, g in [(0.09, 0.3), (0.21, 0.2), (0.37, 0.12)]:
        out += np.roll(lowpass(x, 3500), int(d * SR)) * g
    return out


# --------------------------------------------------------------- the fighting
#
# The combat track: heavy, dark, fast. 150 bpm in E, mostly Phrygian (the
# flat second, F over E, is what makes it menacing) with a raised third in
# the lead for a harmonic-minor edge. Palm-muted gallops under it all, a
# chromatic descent, a half-time breakdown, then back round.
#
# It is chiptune metal: the guitars are detuned saws driven into clipping
# and pushed through a speaker-shaped filter, the drums are noise and
# falling sines, and the whole thing is bit-crushed at the end.

BPM = 150
STEP = 60.0 / BPM / 4          # one sixteenth: 0.1 s
BAR = STEP * 16

RIFFS = {
    # m = palm-muted chug, o = open power chord (with length in sixteenths), - = rest
    "A1": "m40 - m40 m40 m40 - m40 m40 m40 - m40 m40 o41:2 - o43:2 -",
    "A2": "m40 - m40 m40 m40 - m40 m40 m40 - m40 m40 o46:4 - - -",
    "B1": "o40:3 - - o39:3 - - o38:3 - - o37:3 - - m40 m40 m40 m40",
    "B2": "o36:6 - - - - - o35:6 - - - - - m40 m40 m40 m40",
    "BD": "o40:5 - - - - - m40 - o41:4 - - - m40 - m40 -",
    "BE": "o40:5 - - - - - m40 - o46:4 - - - m40 m40 m40 m40",
}

# (riff, drums, lead bar or None)
SONG = (
    [("A1", "A", None), ("A2", "A", None), ("A1", "A", None), ("A2", "Afill", None)] +
    [("B1", "B", None), ("B2", "B", None), ("B1", "B", None), ("B2", "Bfill", None)] +
    [("A1", "A", 0), ("A2", "A", 1), ("A1", "A", 2), ("A2", "A", 3),
     ("A1", "A", 4), ("A2", "A", 5), ("A1", "A", 6), ("A2", "Afill", 7)] +
    [("BD", "D", None), ("BE", "D", None), ("BD", "D", None), ("BE", "Dfill", None)] +
    [("A1", "A", 4), ("A2", "A", 5), ("A1", "A", 6), ("A2", "A", 7)] +
    [("B1", "B", None), ("B2", "B", None), ("B1", "B", None), ("B2", "Bfill", None)]
)

LEAD = [  # (midi, sixteenths) per bar
    [(76, 6), (77, 2), (76, 4), (74, 4)],
    [(72, 6), (71, 2), (72, 4), (76, 4)],
    [(77, 6), (79, 2), (80, 4), (79, 4)],
    [(77, 8), (76, 8)],
    [(83, 6), (84, 2), (83, 4), (81, 4)],
    [(80, 6), (81, 2), (80, 4), (77, 4)],
    [(76, 4), (77, 4), (80, 4), (83, 4)],
    [(82, 8), (76, 8)],
]


def parse_riff(text):
    out = []
    for k, tok in enumerate(text.split()):
        if tok == "-":
            continue
        kind, rest = tok[0], tok[1:]
        midi, _, steps = rest.partition(":")
        out.append((k, kind, int(midi), int(steps) if steps else 1))
    return out


def power_chord(f, d, rng, muted):
    """Root, fifth and octave, each two slightly detuned saws: raw, before the amp."""
    tt = t_axis(d)
    pick = 1 + 0.012 * np.exp(-tt / 0.015)
    x = np.zeros(len(tt))
    for ratio in (1.0, 1.4983, 2.0):
        ff = np.full(len(tt), f * ratio) * pick
        x += saw(ff * 0.996) + saw(ff * 1.004)
    if muted:
        x = lowpass(x, 650) * env(d, 0.002, 0.05)
    else:
        x = x * env(d, 0.003, 1.6) * np.clip((d - tt) / 0.03, 0, 1)
    return x


def kick(rng):
    d = 0.3
    tt = t_axis(d)
    body = sine(130 * np.exp(-tt / 0.03) + 45) * env(d, 0.001, 0.12)
    beater = highpass(noise(len(tt), rng), 2500) * env(d, 0.0002, 0.003) * 0.5
    return body + beater


def snare_hit(rng, g=1.0):
    d = 0.3
    tt = t_axis(d)
    body = sine(np.full(len(tt), 185.0) * (1 + 0.2 * np.exp(-tt / 0.01))) * env(d, 0.001, 0.05) * 0.7
    rattle = bandpass(noise(len(tt), rng), 1500, 9000) * env(d, 0.001, 0.11)
    return (body + rattle) * g


def hat(rng, g=1.0, open_=False):
    d = 0.25 if open_ else 0.05
    return highpass(noise(int(d * SR), rng), 7000) * env(d, 0.0005, 0.08 if open_ else 0.012) * g


def crash(rng, g=1.0):
    d = 2.0
    tt = t_axis(d)
    shimmer = highpass(noise(len(tt), rng), 3500) * env(d, 0.001, 0.7)
    ring = sum(sine(np.full(len(tt), f)) * env(d, 0.001, 0.5) * 0.05 for f in (3150, 4420, 5870, 7330))
    return (shimmer + ring) * g


def tom(rng, midi):
    d = 0.4
    tt = t_axis(d)
    f = note(midi) * (1 + 0.3 * np.exp(-tt / 0.02))
    return sine(f) * env(d, 0.001, 0.15) + lowpass(noise(len(tt), rng), 1200) * env(d, 0.001, 0.02) * 0.4


def music_combat_loop(seed=282):
    rng = np.random.default_rng(seed)
    period = len(SONG) * BAR
    n = int(period * SR)

    def one(r):
        gtr_l = np.zeros(n)
        gtr_r = np.zeros(n)
        bass = np.zeros(n)
        kicks = np.zeros(n)
        snares = np.zeros(n)
        cymbals = np.zeros(n)
        lead = np.zeros(n)
        for b, (riff, drums, lead_bar) in enumerate(SONG):
            t0 = b * BAR
            hits = parse_riff(RIFFS[riff])
            for step, kind, midi, steps in hits:
                at = t0 + step * STEP
                d = steps * STEP + (0.02 if kind == "m" else 0.05)
                muted = kind == "m"
                # Two guitars, one each side in the real thing; here they
                # just thicken each other.
                place_wrapped(gtr_l, power_chord(note(midi), d, r, muted), at)
                place_wrapped(gtr_r, power_chord(note(midi) * 1.002, d, r, muted), at + 0.004)
                tt = t_axis(d)
                fb = np.full(len(tt), note(midi - 12))
                place_wrapped(bass, (triangle(fb) + 0.5 * square(fb, 0.5)) * env(d, 0.002, 0.08 if muted else 0.8), at)
                # The kick follows the guitar in the gallops.
                if drums.startswith("A") or (drums.startswith("B") and (kind == "o" or step >= 12)):
                    place_wrapped(kicks, kick(r), at, 0.9 if kind == "o" else 0.75)
            if drums.startswith("D"):
                for step in (0, 3, 6, 10, 14):
                    place_wrapped(kicks, kick(r), t0 + step * STEP, 0.95)
                place_wrapped(snares, snare_hit(r, 1.0), t0 + 8 * STEP)
                place_wrapped(cymbals, crash(r, 0.5), t0)
                place_wrapped(cymbals, crash(r, 0.35), t0 + 8 * STEP)
                for step in range(0, 16, 2):
                    place_wrapped(cymbals, hat(r, 0.25, open_=step % 4 == 2), t0 + step * STEP)
            else:
                for step in (4, 12):
                    place_wrapped(snares, snare_hit(r, 0.9), t0 + step * STEP)
                for step in range(0, 16, 2):
                    place_wrapped(cymbals, hat(r, 0.3 if step % 4 == 0 else 0.2), t0 + step * STEP)
                if b % 4 == 0 or drums.startswith("B") and b % 2 == 0:
                    place_wrapped(cymbals, crash(r, 0.45), t0)
            if drums.endswith("fill"):
                for k, m in enumerate([52, 50, 47, 45, 43, 43]):
                    place_wrapped(snares, tom(r, m), t0 + (10 + k) * STEP, 0.8)
            if lead_bar is not None:
                at = t0
                for midi, steps in LEAD[lead_bar]:
                    d = steps * STEP
                    tt = t_axis(d + 0.05)
                    vib = 1 + 0.012 * np.sin(2 * np.pi * 5.5 * tt) * np.clip((tt - 0.12) / 0.1, 0, 1)
                    f = np.full(len(tt), note(midi)) * vib
                    tone = square(f, 0.25) + 0.35 * saw(f * 1.003)
                    shape = np.clip(tt / 0.008, 0, 1) * np.clip((d + 0.05 - tt) / 0.06, 0, 1)
                    place_wrapped(lead, tone * shape, at)
                    at += d
        return np.stack([gtr_l, gtr_r, bass, kicks, snares, cymbals, lead])

    def post(x):
        def amp(g):
            g = drive(highpass(g, 80) * 0.6, 7.0)
            return lowpass(bandpass(g, 90, 5000), 3600)
        guitars = (amp(x[0]) + amp(x[1])) * 0.32
        bass = drive(lowpass(x[2], 700) * 0.8, 1.8) * 0.4
        drums = x[3] * 0.85 + x[4] * 0.55 + x[5] * 0.5
        lead = drive(lowpass(x[6], 4000) * 0.5, 2.5) * 0.18
        lead = lead + np.roll(lead, int(3 * STEP * SR)) * 0.3 + np.roll(lead, int(6 * STEP * SR)) * 0.12
        return guitars + bass + drums + lead

    out = periodic(one, period, warmup_periods=1)(rng, post)
    return finish(crush(out, bits=10, hold_rate=26000), peak_db=-2.0, loop=True)


# --------------------------------------------------------- raids and radios

def kill_thud(seed=484):
    """Your kill: a heavy, low thump with a dry crack on top. Felt more than heard."""
    rng = np.random.default_rng(seed)
    length = 0.45
    t = t_axis(length)
    n = len(t)
    body = sine(70 * np.exp(-t / 0.06) + 38) * env(length, 0.001, 0.11) * 1.2
    knock = bandpass(noise(n, rng), 180, 900) * env(length, 0.0005, 0.025) * 0.7
    crack = highpass(noise(n, rng), 2500) * env(length, 0.0002, 0.004) * 0.3
    return finish(chip(drive(mix(body, knock, crack), 2.2), 10, 22050), peak_db=-2.0)


def sniper(seed):
    rng = np.random.default_rng(seed)
    # A heavier load: a harder crack, a deeper thump, a tail that goes on.
    x = report(rng, crack=1.4, blast_cut=4600, blast_decay=0.034, thump_hz=105, thump=1.0,
               tail=0.75, tail_gain=0.45, slap_at=rng.uniform(0.3, 0.45), slap=0.22, length=2.4, heat=2.6)
    return finish(chip(x, 10, 32000))


def radio_static(rng, n):
    hiss = bandpass(noise(n, rng), 800, 5000)
    crackle = np.zeros(n)
    for _ in range(int(n / SR * 30)):
        place(crackle, click(rng, 0.004, 1500, 6000), rng.uniform(0, n / SR - 0.01), rng.uniform(0.2, 0.8))
    return hiss * 0.5 + crackle


def radio_voice(rng, seconds):
    """Somebody far away on a bad line: words you can't make out, squashed into a narrow band."""
    out = np.zeros(int(seconds * SR))
    at = 0.1
    while at < seconds - 0.4:
        sy = [(rng.choice(["k", "t", "p", "d", "s", ""]), rng.choice(["a", "o", "e", "uh"]), rng.uniform(0.08, 0.16)) for _ in range(int(rng.integers(2, 5)))]
        w = word(rng, sy, rng.uniform(150, 190), shout=False)
        place(out, w, at, 0.8)
        at += len(w) / SR + rng.uniform(0.08, 0.25)
    return bandpass(out, 500, 2600)


def radio_call(seed=485):
    """The set: a squelch, garbled voices, static, a squelch."""
    rng = np.random.default_rng(seed)
    length = 2.6
    n = int(length * SR)
    out = radio_static(rng, n) * 0.35
    voice = radio_voice(rng, length)
    out[:len(voice)] += drive(voice / (np.max(np.abs(voice)) or 1.0) * 1.5, 2.0) * 0.8
    for at in (0.0, length - 0.12):
        place(out, sine(np.full(int(0.08 * SR), 1900.0)) * env(0.08, 0.002, 0.03) * 0.5, at)
    return finish(chip(out, 8, 16000), peak_db=-4.0)


def radio_squelch(seed=486):
    rng = np.random.default_rng(seed)
    out = radio_static(rng, int(0.35 * SR)) * env(0.35, 0.002, 0.12)
    place(out, sine(np.full(int(0.06 * SR), 2100.0)) * env(0.06, 0.001, 0.02) * 0.6, 0.0)
    return finish(chip(out, 8, 16000), peak_db=-6.0)


def radio_loop(seed=487):
    rng = np.random.default_rng(seed)
    period = 4.0
    n = int(period * SR)
    make = periodic(lambda r: noise(n, r), period)
    hiss = make(rng, lambda x: bandpass(x, 900, 4500))
    t = t_axis(period)
    flutter = 0.7 + 0.3 * np.sin(2 * np.pi * 6 / period * t) * np.sin(2 * np.pi * 1 / period * t)
    return finish(chip(hiss * flutter, 8, 16000), peak_db=-12.0, loop=True)


def night_loop(seed=488):
    """Before dawn on the ridge: a cold wind, guns a long way off, now and then a bird of the night."""
    rng = np.random.default_rng(seed)
    period = 30.0
    n = int(period * SR)
    booms = [artillery_far(491 + k) for k in range(3)]

    def owl(r):
        out = np.zeros(int(1.2 * SR))
        for at, f, d in [(0.0, 420.0, 0.25), (0.4, 380.0, 0.5)]:
            tt = t_axis(d)
            place(out, sine(np.full(len(tt), f) * (1 - 0.03 * tt / d)) * np.sin(np.pi * tt / d) ** 1.5, at)
        return out

    def one(r):
        wind = noise(n, r)
        far = np.zeros(n)
        for k in range(6):
            place_wrapped(far, booms[k % 3], r.uniform(0, period), r.uniform(0.2, 0.45))
        birds = np.zeros(n)
        for at in (6.0, 21.0):
            place_wrapped(birds, owl(r), at, 0.12)
        return np.stack([wind, far, birds])

    def post(x):
        return lowpass(x[0], 300) * 0.4 + bandpass(x[0], 1500, 4000) * 0.03 + x[1] + x[2]

    gust = 0.75 + 0.25 * np.sin(2 * np.pi * 2 / period * t_axis(period))
    return finish(periodic(one, period, warmup_periods=1)(rng, post) * gust, peak_db=-6.0, loop=True)


def alarm_bell(seed=489):
    """The alarm in their trench: a shell case hung from a post, hammered."""
    rng = np.random.default_rng(seed)
    length = 2.6
    out = np.zeros(int(length * SR))
    for k in range(14):
        d = 0.5
        tt = t_axis(d)
        ring = sum(sine(np.full(len(tt), f)) * env(d, 0.0005, dk) * g for f, dk, g in [(1120, 0.2, 1.0), (1830, 0.12, 0.6), (2710, 0.07, 0.4)])
        at = k * 0.16 + rng.uniform(0, 0.02)
        g = rng.uniform(0.6, 1.0)
        place(out, ring, at, g)
        place(out, click(rng, 0.006, 2000, 7000), at, g * 0.5)          # the hammer
    return finish(chip(reverb(out, 0.8, 0.2), 10, 22050), peak_db=-5.0)


def satchel_fuse(seed=490):
    rng = np.random.default_rng(seed)
    length = 1.2
    t = t_axis(length)
    hiss = highpass(noise(len(t), rng), 2500) * np.clip(t / 0.05, 0, 1) * (0.7 + 0.3 * np.sin(2 * np.pi * 23 * t))
    out = hiss * env(length, 0.0, 1.5) * 0.6
    for _ in range(14):
        place(out, click(rng, 0.004, 2000, 8000), rng.uniform(0.05, 1.1), rng.uniform(0.2, 0.6))
    return finish(out, peak_db=-6.0)


def wire_cut(seed=492):
    """The last strand going: a twang, and the wire springing back."""
    rng = np.random.default_rng(seed)
    length = 0.6
    t = t_axis(length)
    twang = sine(1700 * (1 - 0.4 * t / length)) * env(length, 0.0005, 0.12) * 0.5
    snap = click(rng, 0.01, 2500, 8000)
    out = np.zeros(len(t))
    place(out, snap, 0.0, 1.0)
    out += twang
    out += bandpass(noise(len(t), rng), 3000, 7000) * env(length, 0.01, 0.08) * 0.2
    return finish(out, peak_db=-5.0)


def wire_snip(seed):
    rng = np.random.default_rng(seed)
    out = np.zeros(int(0.12 * SR))
    place(out, click(rng, 0.008, 2500, 8000), 0.0, 1.0)
    place(out, click(rng, 0.005, 4000, 9000), 0.02, 0.4)
    return finish(out, peak_db=-8.0)


def mortar_fire(seed):
    """A mortar: not a crack but a hollow cough, the bomb leaving the tube,
    with a ring of the tube itself under it."""
    rng = np.random.default_rng(seed)
    length = 1.1
    t = t_axis(length)
    n = len(t)
    cough = bandpass(noise(n, rng), 120, 900) * env(length, 0.001, 0.05) * 1.1
    pop = sine(95 * np.exp(-t / 0.03) + 55) * env(length, 0.001, 0.08) * 0.9
    ring = sine(np.full(n, rng.uniform(380, 430))) * env(length, 0.002, 0.18) * 0.12
    air = lowpass(noise(n, rng), 600) * env(length, 0.01, 0.35) * 0.35
    x = mix(cough, pop, ring, air)
    x = reverb(x, seconds=0.9, wet=0.3, cutoff=1800, rng=rng)
    return finish(chip(drive(x, 1.6), 10, 24000), peak_db=-2.0)


def mortar_incoming(seed=498):
    """A bomb coming down: higher and shorter than a shell, falling in pitch."""
    rng = np.random.default_rng(seed)
    length = 1.1
    t = t_axis(length)
    f = 2300 - 1100 * (t / length) ** 1.4
    tone = 0.5 * sine(f) + 0.18 * triangle(f * 1.5)
    air = bandpass(noise(len(t), rng), 1500, 6000) * 0.12
    grow = (t / length) ** 2.0
    return finish((tone + air) * grow, fade_out=0.004)


def shovel(seed):
    """A spade going into the earth, and the spoil thrown off it."""
    rng = np.random.default_rng(seed)
    length = 0.5
    t = t_axis(length)
    n = len(t)
    scrape = bandpass(noise(n, rng), 1200, 4500) * env(length, 0.005, 0.07) * 0.6
    chunk = lowpass(noise(n, rng), 500) * env(length, 0.001, 0.05) * 0.8
    out = mix(scrape, chunk)
    throw_at = int(rng.uniform(0.22, 0.3) * SR)
    spill = lowpass(noise(n - throw_at, rng), 1500) * env((n - throw_at) / SR, 0.01, 0.1) * 0.35
    out[throw_at:] += spill
    return finish(chip(out, 10, 22050), peak_db=-6.0)


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
#
# Names ending _1, _2... are variants: the game picks one at random each
# time, so a sound heard a hundred times never repeats exactly.

SOUNDS = {
    # weapons
    "rifle_1":            lambda: rifle(401),
    "rifle_2":            lambda: rifle(402),
    "rifle_3":            lambda: rifle(403),
    "rifle_far_1":        lambda: rifle_far(404),
    "rifle_far_2":        lambda: rifle_far(405),
    "smg_1":              lambda: smg(406),
    "smg_2":              lambda: smg(407),
    "smg_far_1":          lambda: smg_far(414),
    "smg_far_2":          lambda: smg_far(415),
    "pistol_1":           lambda: pistol(408),
    "pistol_2":           lambda: pistol(416),
    "mg_1":               lambda: mg(409),
    "mg_2":               lambda: mg(410),
    "mg_3":               lambda: mg(411),
    "mg_far_1":           lambda: mg_far(412),
    "mg_far_2":           lambda: mg_far(413),
    "rifle_bolt":         rifle_bolt,
    "rifle_reload":       rifle_reload,
    "smg_reload":         smg_reload,
    "pistol_reload":      pistol_reload,
    "mg_reload":          mg_reload,
    "mg_setup":           mg_setup,
    "overheat":           overheat,
    "click_empty":        click_empty,
    "casing_1":           lambda: casing(417),
    "casing_2":           lambda: casing(418),
    "casing_3":           lambda: casing(419),
    "weapon_pickup":      weapon_pickup,
    "ammo_pickup":        ammo_pickup,
    "bayonet_swish":      bayonet_swish,
    "stab_1":             lambda: stab(452),
    "stab_2":             lambda: stab(453),
    "club_hit":           club_hit,
    # grenades, smoke, flares, shells, fire
    "grenade_pin":        grenade_pin,
    "grenade_bounce":     grenade_bounce,
    "throw":              throw,
    "smoke_pop":          smoke_pop,
    "smoke_hiss":         smoke_hiss_loop,
    "flare_fire":         flare_fire,
    "flare_pop":          flare_pop,
    "explosion_1":        lambda: explosion(421),
    "explosion_2":        lambda: explosion(422),
    "explosion_3":        lambda: explosion(423),
    "explosion_far_1":    lambda: explosion_far(427),
    "explosion_far_2":    lambda: explosion_far(428),
    "artillery_far_1":    lambda: artillery_far(424),
    "artillery_far_2":    lambda: artillery_far(425),
    "shell_incoming":     lambda: shell_incoming(426),
    "flamer_loop":        flamer_loop,
    "flamer_start":       flamer_start,
    "flamer_stop":        flamer_stop,
    "tank_burst":         tank_burst,
    "burning_loop":       burning_loop,
    # rounds arriving
    "snap_1":             lambda: snap(431),
    "snap_2":             lambda: snap(432),
    "snap_3":             lambda: snap(433),
    "snap_heavy_1":       lambda: snap_heavy(429),
    "snap_heavy_2":       lambda: snap_heavy(430),
    "whiz_1":             lambda: whiz(434),
    "whiz_2":             lambda: whiz(435),
    "ricochet_1":         lambda: ricochet(436, 2400, 1300),
    "ricochet_2":         lambda: ricochet(437, 3100, 1900, 0.6),
    "impact_dirt_1":      lambda: impact_dirt(438),
    "impact_dirt_2":      lambda: impact_dirt(439),
    "impact_flesh_1":     lambda: impact_flesh(440),
    "impact_flesh_2":     lambda: impact_flesh(441),
    "impact_flesh_3":     lambda: impact_flesh(442),
    "impact_leaves_1":    lambda: impact_leaves(454),
    "impact_leaves_2":    lambda: impact_leaves(455),
    "gore_1":             lambda: gore(443),
    "gore_2":             lambda: gore(444),
    "impact_sandbag":     lambda: impact_sandbag(445),
    "impact_wood":        lambda: impact_wood(446),
    "bottle_break":       bottle_break,
    "splash_1":           lambda: splash(456),
    "splash_2":           lambda: splash(457),
    # bodies
    "body_fall_1":        lambda: body_fall(447),
    "body_fall_2":        lambda: body_fall(448),
    "footstep_1":         lambda: footstep_mud(449),
    "footstep_2":         lambda: footstep_mud(450),
    "footstep_3":         lambda: footstep_mud(451),
    "footstep_grass_1":   lambda: footstep_grass(458),
    "footstep_grass_2":   lambda: footstep_grass(459),
    "footstep_grass_3":   lambda: footstep_grass(460),
    "wade_1":             lambda: wade(467),
    "wade_2":             lambda: wade(468),
    "drag_loop":          drag_loop,
    "bandage":            bandage,
    # voices
    "scream_1":           lambda: scream(461, 1.1, 560),
    "scream_2":           lambda: scream(462, 0.9, 640),
    "scream_3":           lambda: scream(463, 1.4, 500, again=True),
    "scream_4":           lambda: scream(469, 1.0, 600, then_gasp=False),
    "scream_burn_1":      lambda: scream_burn(475),
    "scream_burn_2":      lambda: scream_burn(476),
    "groan_1":            lambda: groan(464, 1.8, 115),
    "groan_2":            lambda: groan(465, 2.4, 100),
    "groan_3":            lambda: groan(466, 1.4, 135),
    "sob_1":              lambda: sob(477),
    "sob_2":              lambda: sob(478),
    "death_cry_1":        lambda: death_cry(479),
    "death_cry_2":        lambda: death_cry(480),
    "death_cry_3":        lambda: death_cry(481),
    "grunt_1":            lambda: grunt(482, 135),
    "grunt_2":            lambda: grunt(483, 120),
    "medic_call_1":       lambda: medic_call(251),
    "medic_call_2":       lambda: medic_call(252),
    "bark_1":             lambda: bark(471, 3, 170),
    "bark_2":             lambda: bark(472, 2, 160),
    "bark_3":             lambda: bark(473, 4, 185),
    "bark_4":             lambda: bark(474, 2, 200),
    "charge_cry":         charge_cry,
    "kill_thud":          kill_thud,
    "sniper_1":           lambda: sniper(493),
    "sniper_2":           lambda: sniper(494),
    "radio_call":         radio_call,
    "radio_squelch":      radio_squelch,
    "radio_loop":         radio_loop,
    "night_loop":         night_loop,
    "alarm_bell":         alarm_bell,
    "satchel_fuse":       satchel_fuse,
    "wire_cut":           wire_cut,
    "wire_snip_1":        lambda: wire_snip(495),
    "wire_snip_2":        lambda: wire_snip(496),
    "mortar_fire_1":      lambda: mortar_fire(497),
    "mortar_fire_2":      lambda: mortar_fire(499),
    "mortar_incoming":    mortar_incoming,
    "shovel_1":           lambda: shovel(500),
    "shovel_2":           lambda: shovel(501),
    "shovel_3":           lambda: shovel(502),
    "whistle":            whistle,
    # the world
    "tinnitus":           tinnitus,
    "heartbeat":          heartbeat,
    "front_loop":         front_loop,
    "rain_loop":          rain_loop,
    "dawn_loop":          dawn_loop,
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
