#!/usr/bin/env python3
"""Procedural sound effects + ambient loops — stdlib `wave` + `math` only.

No Pillow, no numpy. Generates 44.1 kHz 16-bit mono WAVs into assets/Audio/.
These are the hermetic fallback; CC0 OGGs could replace them later without code
changes (the AudioManager loads by filename). Ambient loops are authored to be
seamless (integer number of cycles, gentle amplitude, fade-free wrap).

Run: python make_sfx.py
"""
from __future__ import annotations

import math
import os
import random
import struct
import wave

SR = 44100
AUDIO_DIR = os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", "..", "Audio")
)


def _write(name, samples):
    os.makedirs(AUDIO_DIR, exist_ok=True)
    path = os.path.join(AUDIO_DIR, name)
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(v * 32000))
        w.writeframes(bytes(frames))
    print(f"  wrote {name} ({len(samples)/SR:.2f}s)")


def env(i, n, attack=0.01, release=0.3):
    """Simple AD envelope in [0,1] over n samples."""
    a = int(attack * SR)
    r = int(release * SR)
    if i < a:
        return i / max(1, a)
    if i > n - r:
        return max(0.0, (n - i) / max(1, r))
    return 1.0


def laser(dur=0.18):
    n = int(dur * SR)
    out = []
    for i in range(n):
        t = i / SR
        f = 1400 - 900 * (i / n)          # downward chirp
        s = math.sin(2 * math.pi * f * t)
        s += 0.3 * math.sin(2 * math.pi * f * 2 * t)
        out.append(0.5 * s * env(i, n, 0.005, 0.12))
    return out


def explosion(dur=0.6):
    n = int(dur * SR)
    rng = random.Random(1)
    out = []
    lp = 0.0
    for i in range(n):
        noise = rng.uniform(-1, 1)
        lp += 0.15 * (noise - lp)          # low-pass -> rumble
        s = lp + 0.4 * math.sin(2 * math.pi * 60 * (i / SR))
        out.append(0.7 * s * env(i, n, 0.002, 0.5))
    return out


def hurt(dur=0.25):
    n = int(dur * SR)
    out = []
    for i in range(n):
        t = i / SR
        f = 320 - 180 * (i / n)
        s = math.sin(2 * math.pi * f * t)
        s += 0.4 * (1 if math.sin(2 * math.pi * f * t) > 0 else -1)  # buzz
        out.append(0.5 * s * env(i, n, 0.004, 0.18))
    return out


def arpeggio(dur=0.5, notes=(523, 659, 784, 1047)):
    n = int(dur * SR)
    out = []
    seg = n // len(notes)
    for i in range(n):
        f = notes[min(len(notes) - 1, i // seg)]
        t = i / SR
        s = math.sin(2 * math.pi * f * t) + 0.2 * math.sin(2 * math.pi * f * 2 * t)
        out.append(0.45 * s * env(i, n, 0.005, 0.15))
    return out


def chime(dur=0.4, base=880):
    n = int(dur * SR)
    out = []
    for i in range(n):
        t = i / SR
        s = math.sin(2 * math.pi * base * t) + 0.5 * math.sin(2 * math.pi * base * 1.5 * t)
        out.append(0.4 * s * env(i, n, 0.003, 0.3))
    return out


def ambient(name_seed, dur=4.0, root=110.0, seed=7):
    """A seamless low drone: sum of integer-harmonic sines (so it wraps at `dur`)
    plus slow filtered noise shimmer. Kept quiet — it sits under the SFX."""
    n = int(dur * SR)
    rng = random.Random(seed)
    # choose harmonics whose periods divide dur exactly for seamless looping
    base_cycles = round(root * dur)
    harmonics = [(base_cycles, 0.5), (base_cycles * 2, 0.22),
                 (round(base_cycles * 1.5), 0.18), (base_cycles * 3, 0.1)]
    out = [0.0] * n
    for cycles, amp in harmonics:
        f = cycles / dur
        ph = rng.uniform(0, 2 * math.pi)
        for i in range(n):
            out[i] += amp * math.sin(2 * math.pi * f * (i / SR) + ph)
    # gentle amplitude LFO — integer cycles over the loop, so it wraps seamlessly
    lfo_cycles = 2
    for i in range(n):
        lfo = 0.75 + 0.25 * math.sin(2 * math.pi * lfo_cycles * i / n)
        out[i] *= 0.18 * lfo
    return out


def main():
    print("make_sfx:")
    _write("laser.wav", laser())
    _write("explosion.wav", explosion())
    _write("hurt.wav", hurt())
    _write("level_up.wav", arpeggio())
    _write("wave_chime.wav", chime())
    _write("ambient_core.wav", ambient("core", root=110, seed=1))
    _write("ambient_foundry.wav", ambient("foundry", root=82, seed=2))
    _write("ambient_biolab.wav", ambient("biolab", root=146, seed=3))


if __name__ == "__main__":
    main()
