#!/usr/bin/env python3
"""Tests negativos y de determinismo del banco objetivo de #349.

Ejecutar con el venv de requirements.txt:
  python scripts/test_benchmark.py

Requiere el driver compilado y la imagen de Docker de #351 para el control de
fragmentación. Si el driver no está, ese test se salta y se informa (no se
inventa PASS).
"""

from __future__ import annotations

import hashlib
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest
from scipy.signal import butter, sosfilt

SCRIPTS_DIR = Path(__file__).resolve().parent
EVIDENCE_DIR = SCRIPTS_DIR.parent
REPO = EVIDENCE_DIR.parents[2]
SCRATCH = Path(os.environ.get("RX349_SCRATCH", REPO.parent / "assessment-194" / "scratch" / "349-auto"))
DRIVER = Path(os.environ.get("RX349_DRIVER", SCRATCH / "bench" / "speex_driver"))
IMAGE = os.environ.get("RX349_IMAGE", "x6200-speexdsp-capture-bench:1.2.1-1")

sys.path.insert(0, str(SCRIPTS_DIR))
import run_benchmark as rb  # noqa: E402


def test_off_no_filter_is_bit_exact():
    rng = np.random.default_rng(6200)
    x = rng.uniform(-0.5, 0.5, 44100)
    q = rb.quantize(x)
    assert np.array_equal(q, rb.quantize(rb.dequantize(q)))
    assert np.max(np.abs(q.astype(np.int32))) <= 32768


def test_proxy_filter_deterministic():
    rng = np.random.default_rng(6200)
    x = rng.normal(0, 0.1, 44100)
    a = sosfilt(rb.SOS_NORMAL, x).tobytes()
    b = sosfilt(rb.SOS_NORMAL, x).tobytes()
    assert a == b
    dx = sosfilt(rb.SOS_DX, x).tobytes()
    assert a != dx


def test_noise_seed_changes():
    a = rb.noise_unit("AWGN", 1000, 6200)
    b = rb.noise_unit("AWGN", 1000, 6200)
    c = rb.noise_unit("AWGN", 1000, 6201)
    assert np.array_equal(a, b)
    assert not np.array_equal(a, c)


def test_no_speech_mask():
    assert not np.any(rb.active_mask(np.zeros(44100)))
    assert not rb.has_speech(np.zeros(44100))
    assert not rb.has_speech(np.full(44100, 1e-8))
    tone = 0.2 * np.sin(2 * np.pi * 1000 * np.arange(44100) / 44100)
    assert not rb.has_speech(tone)
    assert not rb.has_speech(np.random.default_rng(6200).normal(0, 0.1, 44100))


def test_stoi_self_is_one():
    rng = np.random.default_rng(6200)
    x = rng.normal(0, 0.2, 44100 * 2)
    x = sosfilt(rb.SOS_NORMAL, x)
    value = rb.stoi(x[441:-441], x[441:-441], 44100, extended=False)
    assert abs(value - 1.0) <= 1e-3


def test_injected_delay_recovered():
    rng = np.random.default_rng(6200)
    x = rng.normal(0, 0.2, 44100 * 2)
    x = sosfilt(rb.SOS_NORMAL, x)
    delayed = np.concatenate([np.zeros(882), x[:-882]])
    ref = x[441 : len(x) - 441]
    proc = delayed[441 + 882 : len(delayed) - 441]
    n = min(len(ref), len(proc))
    value = rb.stoi(ref[:n], proc[:n], 44100, extended=False)
    assert value > 0.99


def test_hash_mismatch_detected():
    a = rb.sha256_bytes(b"voz")
    b = rb.sha256_bytes(b"otra-voz")
    assert a != b


def _tone_raw(path: Path) -> bytes:
    n = 44100 * 2
    t = np.arange(n) / 44100.0
    x = 0.2 * np.sin(2 * np.pi * 1000 * t)
    q = rb.quantize(x)
    path.write_bytes(q.tobytes())
    return path.read_bytes()


def _run_driver(in_path: Path, out_path: Path, preset: int, blocks: str) -> None:
    cmd = [
        "docker", "run", "--rm",
        "-v", f"{SCRATCH}:/data:rw",
        "-w", "/data",
        IMAGE,
        f"/data/{DRIVER.relative_to(SCRATCH)}",
        "--in", f"/data/{in_path.relative_to(SCRATCH)}",
        "--out", f"/data/{out_path.relative_to(SCRATCH)}",
        "--preset", str(preset),
        "--blocks", blocks,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    assert proc.returncode == 0, proc.stderr[-2000:]


@pytest.mark.skipif(not DRIVER.is_file(), reason="driver no compilado")
def test_speex_partition_invariant_fresh_process():
    work = SCRATCH / "test_cases"
    work.mkdir(parents=True, exist_ok=True)
    raw = work / "tone.raw"
    _tone_raw(raw)
    hashes = []
    for tag, blocks in (("4410", "4410"), ("441", "441"), ("mixed", "1,440,441,442,257")):
        out = work / f"tone_{tag}.raw"
        _run_driver(raw, out, 2, blocks)
        hashes.append(hashlib.sha256(out.read_bytes()).hexdigest())
    assert len(set(hashes)) == 1


@pytest.mark.skipif(not DRIVER.is_file(), reason="driver no compilado")
def test_driver_off_bit_exact():
    work = SCRATCH / "test_cases"
    work.mkdir(parents=True, exist_ok=True)
    raw = work / "tone_off.raw"
    before = _tone_raw(raw)
    out = work / "tone_off_out.raw"
    _run_driver(raw, out, 0, "4410")
    assert out.read_bytes() == before
