#!/usr/bin/env python3
"""Generate the original, redistributable LeetDJ tutorial music pack."""

from __future__ import annotations

import math
import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np


SAMPLE_RATE = 44_100
DURATION_SECONDS = 96
ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "res" / "tutorials" / "audio"

TRACKS = (
    {
        "file": "neon-current.mp3",
        "title": "Neon Current",
        "bpm": 124,
        "root": 55.0,
        "mode": (0, 3, 5, 7, 10),
        "seed": 124,
    },
    {
        "file": "glass-horizon.mp3",
        "title": "Glass Horizon",
        "bpm": 128,
        "root": 65.406,
        "mode": (0, 2, 3, 7, 10),
        "seed": 128,
    },
    {
        "file": "solar-circuit.mp3",
        "title": "Solar Circuit",
        "bpm": 126,
        "root": 73.416,
        "mode": (0, 2, 5, 7, 9),
        "seed": 126,
    },
)


def envelope(length: int, attack: float, release: float) -> np.ndarray:
    values = np.ones(length, dtype=np.float32)
    attack_samples = min(length, max(1, int(attack * SAMPLE_RATE)))
    release_samples = min(length, max(1, int(release * SAMPLE_RATE)))
    values[:attack_samples] = np.linspace(0, 1, attack_samples, dtype=np.float32)
    values[-release_samples:] *= np.linspace(1, 0, release_samples, dtype=np.float32)
    return values


def add_tone(
    mix: np.ndarray,
    start: float,
    duration: float,
    frequency: float,
    amplitude: float,
    *,
    decay: float = 0.0,
) -> None:
    first = int(start * SAMPLE_RATE)
    length = min(int(duration * SAMPLE_RATE), mix.size - first)
    if first < 0 or length <= 0:
        return
    time = np.arange(length, dtype=np.float32) / SAMPLE_RATE
    phase = 2 * math.pi * frequency * time
    tone = np.sin(phase) + 0.23 * np.sin(2 * phase) + 0.08 * np.sin(3 * phase)
    shape = envelope(length, 0.008, min(0.08, duration / 3))
    if decay:
        shape *= np.exp(-time * decay)
    mix[first:first + length] += (tone * shape * amplitude).astype(np.float32)


def add_noise(
    mix: np.ndarray,
    start: float,
    duration: float,
    amplitude: float,
    rng: np.random.Generator,
    *,
    decay: float,
) -> None:
    first = int(start * SAMPLE_RATE)
    length = min(int(duration * SAMPLE_RATE), mix.size - first)
    if first < 0 or length <= 0:
        return
    time = np.arange(length, dtype=np.float32) / SAMPLE_RATE
    noise = rng.standard_normal(length).astype(np.float32)
    # First difference removes most low-frequency energy for a hat/snare timbre.
    noise = np.concatenate(([0], np.diff(noise))).astype(np.float32)
    noise /= max(1.0, float(np.max(np.abs(noise))))
    mix[first:first + length] += noise * np.exp(-time * decay) * amplitude


def synthesize(track: dict[str, object]) -> np.ndarray:
    bpm = int(track["bpm"])
    root = float(track["root"])
    mode = tuple(int(value) for value in track["mode"])
    rng = np.random.default_rng(int(track["seed"]))
    beat = 60.0 / bpm
    count = math.ceil(DURATION_SECONDS / beat)
    left = np.zeros(DURATION_SECONDS * SAMPLE_RATE, dtype=np.float32)
    right = np.zeros_like(left)

    for index in range(count):
        when = index * beat
        bar_beat = index % 4
        phrase = int(when // 16)
        section_gain = (0.70, 0.86, 1.0, 0.82, 1.0, 0.72)[min(phrase, 5)]

        if bar_beat in (0, 2):
            add_tone(left, when, 0.20, 52, 0.34 * section_gain, decay=18)
            add_tone(right, when, 0.20, 52, 0.34 * section_gain, decay=18)
        if bar_beat in (1, 3):
            add_noise(left, when, 0.15, 0.15 * section_gain, rng, decay=24)
            add_noise(right, when, 0.15, 0.15 * section_gain, rng, decay=24)
        add_noise(left, when + beat / 2, 0.055, 0.065 * section_gain, rng, decay=55)
        add_noise(right, when + beat / 2, 0.055, 0.065 * section_gain, rng, decay=55)

        bass_step = mode[(index // 4) % len(mode)]
        bass_frequency = root * (2 ** (bass_step / 12))
        add_tone(left, when, beat * 0.75, bass_frequency, 0.14 * section_gain)
        add_tone(right, when, beat * 0.75, bass_frequency, 0.14 * section_gain)

        if phrase >= 1 and bar_beat in (0, 2):
            lead_step = mode[(index // 2 + phrase) % len(mode)] + 24
            lead_frequency = root * (2 ** (lead_step / 12))
            pan = 0.75 if index % 4 == 0 else 0.35
            add_tone(left, when, beat * 1.4, lead_frequency, 0.08 * section_gain * pan)
            add_tone(right, when, beat * 1.4, lead_frequency, 0.08 * section_gain * (1.1 - pan))

    stereo = np.column_stack((left, right))
    peak = float(np.max(np.abs(stereo))) or 1.0
    return np.tanh(stereo / peak * 1.35).astype(np.float32) * 0.72


def write_track(track: dict[str, object]) -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    samples = synthesize(track)
    with tempfile.TemporaryDirectory(prefix="leetdj-tutorial-") as temporary:
        wav_path = Path(temporary) / "source.wav"
        with wave.open(str(wav_path), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(SAMPLE_RATE)
            output.writeframes((samples * 32767).astype("<i2").tobytes())
        subprocess.run(
            [
                "ffmpeg", "-v", "error", "-y", "-i", str(wav_path),
                "-c:a", "libmp3lame", "-q:a", "4",
                "-metadata", f"title={track['title']}",
                "-metadata", "artist=LeetDJ Training Band",
                "-metadata", "album=LeetDJ Tutorial Library",
                "-metadata", f"bpm={track['bpm']}",
                str(OUTPUT_DIR / str(track["file"])),
            ],
            check=True,
        )


def main() -> None:
    for track in TRACKS:
        write_track(track)
        print(OUTPUT_DIR / str(track["file"]))


if __name__ == "__main__":
    main()
