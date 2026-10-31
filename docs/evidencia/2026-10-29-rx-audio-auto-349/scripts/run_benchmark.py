#!/usr/bin/env python3
"""Banco objetivo de #349: STOI/ESTOI sobre 12 clips LibriSpeech, 72 escenarios y
4 brazos (Normal/DX x Off/SSB voice) con SpeexDSP real del repo vigente.

Uso (desde el worktree, con el venv de requirements.txt):
  RX349_SCRATCH=/ruta/fuera/del/repo python scripts/run_benchmark.py

El corpus, el driver compilado y los PCM intermedios quedan FUERA del
repositorio. Este script escribe en docs/evidencia/2026-10-29-rx-audio-auto-349/:
  manifest.json, measurements.csv, aggregate.json y commands.log.

No usa la radio, no transmite y no incluye audio de QSO ni clips en el repo.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import numpy as np
import soundfile as sf
from scipy.signal import butter, resample_poly, sosfilt, welch
from pystoi import stoi

EVIDENCE_DIR = Path(__file__).resolve().parents[1]
REPO = EVIDENCE_DIR.parents[2]
DEFAULT_SCRATCH = REPO.parent / "assessment-194" / "scratch" / "349-auto"
DEFAULT_CORPUS = DEFAULT_SCRATCH / "corpus" / "LibriSpeech" / "test-clean"
DEFAULT_IMAGE = "x6200-speexdsp-capture-bench:1.2.1-1"
CORPUS_URL = "https://www.openslr.org/resources/12/test-clean.tar.gz"
CORPUS_MD5 = "32fa31d27d2e1cad72775fee3f4849a9"
SEED = 6200
SNRS = (0, 5, 10)
NOISE_TYPES = ("AWGN", "QRM_TONE")
ARMS = ("N_OFF", "N_SPEEX", "DX_OFF", "DX_SPEEX")
PRESET = {"N_OFF": 0, "N_SPEEX": 2, "DX_OFF": 0, "DX_SPEEX": 2}
SOS_NORMAL = butter(4, [150, 2850], btype="bandpass", fs=44100, output="sos")
SOS_DX = butter(4, [300, 2400], btype="bandpass", fs=44100, output="sos")
FRAME = 882  # 20 ms a 44100 Hz
GUARD = 441
SPEEX_DELAY = 882

BENCH_CONTAINER = os.environ.get("RX349_BENCH_CONTAINER")
SCRATCH_GLOBAL: Path | None = None
DRIVER_PATH: Path | None = None
IMAGE_GLOBAL = os.environ.get("RX349_IMAGE", "x6200-speexdsp-capture-bench:1.2.1-1")


# --------------------------------------------------------------------------- #
# utilidades
# --------------------------------------------------------------------------- #
def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_path(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


SCRIPT_DIR = Path(__file__).resolve().parent
TRACE_FILES = (
    "run_benchmark.py",
    "test_benchmark.py",
    "speex_driver.c",
    "build_speex_driver.sh",
    "requirements.txt",
    "fetch_corpus.sh",
)


def script_hashes() -> dict[str, str]:
    """Hashes de los scripts que producen/validan los 288 datos (trazabilidad #361)."""
    return {name: sha256_path(SCRIPT_DIR / name) for name in TRACE_FILES if (SCRIPT_DIR / name).is_file()}


def dbfs(values: np.ndarray) -> float:
    if values.size == 0:
        return float("nan")
    rms = float(np.sqrt(np.mean(np.asarray(values, dtype=np.float64) ** 2) + 1e-30))
    return 20.0 * np.log10(max(rms, 1e-12))


def quantize(x: np.ndarray) -> np.ndarray:
    return np.clip(np.rint(np.asarray(x, dtype=np.float64) * 32768.0), -32768, 32767).astype("<i2")


def dequantize(q: np.ndarray) -> np.ndarray:
    return np.asarray(q, dtype=np.float64) / 32768.0


def clipped_count(values: np.ndarray | bytes | bytearray) -> int:
    """Cuenta muestras S16 saturadas a ±FS/32767, sin overflow en int64."""
    if isinstance(values, (bytes, bytearray)):
        q = np.frombuffer(values, dtype="<i2").astype(np.int64)
    else:
        q = np.asarray(values, dtype=np.int64)
    return int(np.count_nonzero(np.abs(q) >= 32767))


def common_window(clean_ref: np.ndarray, offsets: dict[str, int]) -> tuple[int, int]:
    """Ventana de referencia UNICA para todos los brazos de un escenario.

    start=GUARD; end=len(clean_ref)-GUARD-max(offset). Cada brazo toma
    ref=clean_ref[start:end] y proc=out_f[start+offset:end+offset]; asi los
    STOI/ESTOI pareados usan exactamente el mismo intervalo.
    """
    max_offset = max(offsets.values()) if offsets else 0
    start = GUARD
    end = len(clean_ref) - GUARD - max_offset
    if end - start <= 1000:
        raise ValueError(f"ventana comun insuficiente: {end - start}")
    return start, end


def active_mask(clean: np.ndarray) -> np.ndarray:
    """Máscara de muestras activas por energía en ventanas de 20 ms."""
    n_frames = len(clean) // FRAME
    if n_frames == 0:
        return np.zeros(0, dtype=bool)
    frames = clean[: n_frames * FRAME].reshape(n_frames, FRAME)
    rms = np.sqrt(np.mean(frames.astype(np.float64) ** 2, axis=1) + 1e-30)
    peak = float(np.max(rms))
    if peak <= 1e-12:
        return np.zeros(len(clean), dtype=bool)
    threshold = peak * (10.0 ** (-35.0 / 20.0))
    frame_mask = rms > threshold
    mask = np.repeat(frame_mask, FRAME)
    if len(mask) < len(clean):
        mask = np.concatenate([mask, np.zeros(len(clean) - len(mask), dtype=bool)])
    return mask


def has_speech(clean: np.ndarray) -> bool:
    """Heurística explícita para rechazar silencio/tono/ruido estacionario como voz.

    No sustituye una verificación humana: es un gate del banco para que un input
    sin voz no produzca agregados. Voz real: hay energía y una fracción activa
    intermedia; un tono puro o ruido estacionario activan prácticamente todas
    las ventanas y se rechazan.
    """
    clean = np.asarray(clean, dtype=np.float64)
    if len(clean) < 3 * FRAME or float(np.max(np.abs(clean))) < 1e-4:
        return False
    mask = active_mask(clean)
    if not np.any(mask):
        return False
    fraction = float(np.mean(mask))
    return 0.05 <= fraction <= 0.98


def rms_active(x: np.ndarray, mask: np.ndarray) -> float:
    values = np.asarray(x, dtype=np.float64)[mask]
    if values.size == 0:
        return 0.0
    return float(np.sqrt(np.mean(values ** 2) + 1e-30))


def band_db(x: np.ndarray, lo: float, hi: float) -> float:
    if len(x) < 16:
        return float("nan")
    f, pxx = welch(np.asarray(x, dtype=np.float64), fs=44100, nperseg=min(4096, len(x)))
    band = (f >= lo) & (f <= hi)
    total = float(np.sum(pxx[band]))
    return 10.0 * np.log10(max(total, 1e-30))


def tone_amp(x: np.ndarray, freq: float = 2650.0) -> float:
    x = np.asarray(x, dtype=np.float64)
    n = np.arange(len(x), dtype=np.float64)
    return float(np.abs(2.0 / max(len(x), 1) * np.sum(x * np.exp(-2j * np.pi * freq * n / 44100.0))))


def percentile(values: list[float], p: float) -> float:
    return float(np.percentile(np.asarray(values, dtype=np.float64), p)) if values else float("nan")


def run(cmd: list[str], *, check: bool = True, timeout: int | None = None) -> subprocess.CompletedProcess:
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    if check and proc.returncode != 0:
        raise RuntimeError(f"comando fallo ({proc.returncode}): {' '.join(cmd)}\n{proc.stderr[-3000:]}")
    return proc


# --------------------------------------------------------------------------- #
# corpus
# --------------------------------------------------------------------------- #
def _load_transcripts(corpus_dir: Path) -> dict[str, str]:
    texts: dict[str, str] = {}
    for path in corpus_dir.rglob("*.trans.txt"):
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line.strip():
                continue
            key, _, value = line.partition(" ")
            texts[key] = value.strip()
    return texts


def select_clips(corpus_dir: Path) -> list[dict[str, Any]]:
    if not corpus_dir.is_dir():
        raise SystemExit(f"BLOCKED_CORPUS: no existe {corpus_dir}")
    transcripts = _load_transcripts(corpus_dir)
    speakers = sorted(p for p in corpus_dir.iterdir() if p.is_dir())
    selected: list[dict[str, Any]] = []

    for speaker in speakers:
        candidates: list[tuple[str, Path, float, int, int]] = []
        for path in sorted(speaker.rglob("*.flac")):
            try:
                info = sf.info(str(path))
            except Exception:
                continue
            if info.samplerate != 16000 or info.channels != 1:
                raise SystemExit(f"BLOCKED_CORPUS: {path} no es 16 kHz mono")
            if 4.0 <= float(info.duration) <= 12.0:
                candidates.append((path.stem, path, float(info.duration), info.samplerate, info.channels))
        if len(candidates) < 3:
            continue
        for clip_id, path, duration, sr, channels in candidates[:3]:
            selected.append(
                {
                    "clip_id": clip_id,
                    "speaker_id": speaker.name,
                    "path": path,
                    "duration": duration,
                    "original_sr": sr,
                    "channels": channels,
                    "transcript": transcripts.get(clip_id, "UNKNOWN"),
                }
            )
        if len(selected) // 3 == 4:
            break

    if len(selected) != 12:
        raise SystemExit(f"BLOCKED_CORPUS: se esperaban 12 clips, hay {len(selected)}")
    return selected


def load_clip_44100(item: dict[str, Any], derived_dir: Path) -> dict[str, Any]:
    audio, sr = sf.read(str(item["path"]), dtype="float64", always_2d=False)
    if sr != 16000 or audio.ndim != 1:
        raise SystemExit(f"BLOCKED_CORPUS: {item['path']} inesperado")
    resampled = resample_poly(audio, 441, 160)
    s16 = quantize(resampled)
    voice = dequantize(s16)
    derived_dir.mkdir(parents=True, exist_ok=True)
    raw_path = derived_dir / f"{item['clip_id']}.raw"
    raw_path.write_bytes(s16.tobytes())
    return {
        **item,
        "voice": voice,
        "s16": s16,
        "n_samples": len(s16),
        "converted_sha256": sha256_path(raw_path),
        "original_sha256": sha256_path(item["path"]),
        "raw_path": str(raw_path),
    }


# --------------------------------------------------------------------------- #
# alineación
# --------------------------------------------------------------------------- #
def alignment() -> dict[str, Any]:
    imp = np.zeros(8192, dtype=np.float64)
    imp[2048] = 1.0
    normal = sosfilt(SOS_NORMAL, imp)
    dx = sosfilt(SOS_DX, imp)
    idx_n = int(np.argmax(np.abs(normal)))
    idx_d = int(np.argmax(np.abs(dx)))
    d_dfl = idx_d - idx_n
    if abs(d_dfl) > 441:
        raise SystemExit(f"BLOCKED_ALIGNMENT: d_dfl={d_dfl} (>441)")
    return {
        "impulse_length": len(imp),
        "impulse_peak_index": 2048,
        "normal_peak_index": idx_n,
        "dx_peak_index": idx_d,
        "d_dfl_samples": d_dfl,
        "offsets": {
            "N_OFF": 0,
            "N_SPEEX": SPEEX_DELAY,
            "DX_OFF": d_dfl,
            "DX_SPEEX": d_dfl + SPEEX_DELAY,
        },
    }


# --------------------------------------------------------------------------- #
# pipeline por escenario
# --------------------------------------------------------------------------- #
def noise_unit(noise_type: str, n: int, seed: int) -> np.ndarray:
    if noise_type == "AWGN":
        return np.random.default_rng(seed).standard_normal(n)
    if noise_type == "QRM_TONE":
        t = np.arange(n, dtype=np.float64) / 44100.0
        return np.sin(2.0 * np.pi * 2650.0 * t)
    raise ValueError(noise_type)


def build_scenario(
    clip: dict[str, Any],
    snr_db: int,
    noise_type: str,
    scenario_index: int,
    out_dir: Path,
) -> dict[str, Any]:
    voice = clip["voice"]
    clean_normal = sosfilt(SOS_NORMAL, voice)
    if not has_speech(clean_normal):
        return {"invalid": "NO_SPEECH", "clip_id": clip["clip_id"], "snr_db": snr_db, "noise_type": noise_type}
    mask = active_mask(clean_normal)

    unit = noise_unit(noise_type, len(voice), SEED + scenario_index)
    unit_normal = sosfilt(SOS_NORMAL, unit)
    clean_rms = rms_active(clean_normal, mask)
    noise_rms = rms_active(unit_normal, mask)
    if clean_rms <= 0.0 or noise_rms <= 0.0:
        return {"invalid": "NO_SPEECH_OR_NOISE", "clip_id": clip["clip_id"], "snr_db": snr_db, "noise_type": noise_type}

    scale = clean_rms / noise_rms * (10.0 ** (-snr_db / 20.0))
    input_mix = voice + scale * unit
    peak = float(np.max(np.abs(input_mix)))
    factor = 1.0
    if peak > 0.95:
        factor = 0.95 / peak
        input_mix = input_mix * factor
    clean_ref = clean_normal * factor

    normal_in = sosfilt(SOS_NORMAL, input_mix)
    dx_in = sosfilt(SOS_DX, input_mix)

    inputs = {
        "N_OFF": normal_in,
        "N_SPEEX": normal_in,
        "DX_OFF": dx_in,
        "DX_SPEEX": dx_in,
    }
    raw_inputs: dict[str, Path] = {}
    for arm, signal in inputs.items():
        q = quantize(signal)
        path = out_dir / f"in_{arm}.raw"
        path.write_bytes(q.tobytes())
        raw_inputs[arm] = path

    return {
        "invalid": None,
        "clip_id": clip["clip_id"],
        "speaker_id": clip["speaker_id"],
        "snr_db": snr_db,
        "noise_type": noise_type,
        "scenario_index": scenario_index,
        "clean_ref": clean_ref,
        "input_mix": input_mix,
        "inputs": inputs,
        "raw_inputs": raw_inputs,
        "scale": scale,
        "peak_factor": factor,
        "mask": mask,
        "scene_dir": out_dir,
    }


def driver_command(in_rel: str, out_rel: str, preset: int, blocks: str = "4410") -> list[str]:
    args = [
        "--in", f"/data/{in_rel}",
        "--out", f"/data/{out_rel}",
        "--preset", str(preset),
        "--blocks", blocks,
    ]
    if BENCH_CONTAINER:
        return ["docker", "exec", BENCH_CONTAINER, f"/data/{driver_rel()}"] + args
    return ["docker", "run", "--rm", "-v", f"{SCRATCH_GLOBAL}:/data:rw", "-w", "/data",
            IMAGE_GLOBAL, f"/data/{driver_rel()}"] + args


def driver_rel() -> str:
    return str(DRIVER_PATH.relative_to(SCRATCH_GLOBAL))


def driver_json(cmd: list[str]) -> dict[str, Any]:
    proc = run(cmd, check=False)
    lines = [line for line in proc.stdout.strip().splitlines() if line.startswith("{")]
    if proc.returncode != 0 or not lines:
        raise RuntimeError(f"driver fallo ({proc.returncode}): {proc.stderr[-2000:]}")
    return json.loads(lines[-1])


def measure_arm(
    arm: str,
    scene: dict[str, Any],
    driver: Path,
    image: str,
    scratch: Path,
    offsets: dict[str, int],
    window: tuple[int, int],
) -> dict[str, Any]:
    raw_in = scene["raw_inputs"][arm]
    out_path = scene["scene_dir"] / f"out_{arm}.raw"
    cmd = driver_command(str(raw_in.relative_to(scratch)), str(out_path.relative_to(scratch)), PRESET[arm])
    meta = driver_json(cmd)
    in_bytes = raw_in.read_bytes()
    out_bytes = out_path.read_bytes()
    if len(out_bytes) != len(in_bytes):
        raise RuntimeError(f"longitud distinta en {arm}: {len(in_bytes)} != {len(out_bytes)}")
    out_s16 = np.frombuffer(out_bytes, dtype="<i2")
    out_f = dequantize(out_s16)
    input_float = scene["inputs"][arm]
    clean_ref = scene["clean_ref"]
    offset = offsets[arm]
    if offset < 0:
        raise RuntimeError(f"offset negativo {arm}")
    start, end = window
    if start != GUARD or end <= start + 1000:
        raise RuntimeError(f"ventana comun invalida: {start}:{end}")
    ref_win = clean_ref[start:end]
    proc_win = out_f[start + offset : end + offset]
    if len(ref_win) != len(proc_win) or len(ref_win) < 1000:
        raise RuntimeError(f"ventana comun invalida {arm}: {len(ref_win)}/{len(proc_win)}")
    if end + offset > len(out_f):
        raise RuntimeError(f"ventana fuera de rango {arm}")

    stoi_value = float(stoi(ref_win, proc_win, 44100, extended=False))
    estoi_value = float(stoi(ref_win, proc_win, 44100, extended=True))

    mask = scene["mask"]
    mask_win = mask[start : min(end, len(mask))]
    if len(mask_win) < len(ref_win):
        mask_win = np.concatenate([mask_win, np.zeros(len(ref_win) - len(mask_win), dtype=bool)])
    inactive = ~mask_win

    clipped_in = clipped_count(in_bytes)
    clipped_out = clipped_count(out_s16)
    qrm_supp = None
    if scene["noise_type"] == "QRM_TONE":
        a_in = tone_amp(scene["inputs"][arm])
        a_out = tone_amp(out_f)
        qrm_supp = float(20.0 * np.log10(max(a_out, 1e-12) / max(a_in, 1e-12)))

    return {
        "arm": arm,
        "preset": PRESET[arm],
        "stoi": stoi_value,
        "estoi": estoi_value,
        "valid": True,
        "off_bit_exact": (in_bytes == out_bytes) if PRESET[arm] == 0 else None,
        "rms_in_dbfs": dbfs(scene["inputs"][arm][: min(len(scene["inputs"][arm]), len(out_f))]),
        "rms_out_dbfs": dbfs(out_f),
        "peak_dbfs": dbfs(np.array([np.max(np.abs(out_f))])),
        "clipped_samples_in": clipped_in,
        "clipped_samples_out": clipped_out,
        "dc_offset": float(np.mean(out_f)),
        "samples_in": int(len(in_bytes) // 2),
        "samples_out": int(len(out_bytes) // 2),
        "offset_samples": int(offset),
        "window_samples": int(len(ref_win)),
        "window_start_samples": int(start),
        "window_end_samples": int(end),
        "cpu_p50_ms": float(meta.get("cb_p50_ms", float("nan"))),
        "cpu_p99_ms": float(meta.get("cb_p99_ms", float("nan"))),
        "cpu_max_ms": float(meta.get("cb_max_ms", float("nan"))),
        "residual_dbfs": dbfs((proc_win - ref_win)[inactive]) if np.any(inactive) else None,
        "band_300_2400_db": float(band_db(proc_win, 300, 2400) - band_db(ref_win, 300, 2400)),
        "band_2400_2850_db": float(band_db(proc_win, 2400, 2850) - band_db(ref_win, 2400, 2850)),
        "qrm_suppression_db": qrm_supp,
        "output_sha256": sha256_bytes(out_bytes),
        "input_sha256": sha256_bytes(in_bytes),
        "notes": "",
    }


def run_matrix(
    clips: list[dict[str, Any]],
    scratch: Path,
    driver: Path,
    image: str,
    offsets: dict[str, int],
    run_name: str,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[str]]:
    base = scratch / run_name
    base.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, Any]] = []
    invalid: list[dict[str, Any]] = []
    artifacts: list[str] = []
    scenario_index = 0

    for clip in clips:
        for snr in SNRS:
            for noise_type in NOISE_TYPES:
                scene_dir = base / f"{clip['clip_id']}_{snr}_{noise_type}"
                scene_dir.mkdir(parents=True, exist_ok=True)
                scene = build_scenario(clip, snr, noise_type, scenario_index, scene_dir)
                scenario_index += 1
                if scene.get("invalid"):
                    invalid.append(scene)
                    continue
                window = common_window(scene["clean_ref"], offsets)
                arm_results: dict[str, dict[str, Any]] = {}
                for arm in ARMS:
                    result = measure_arm(arm, scene, driver, image, scratch, offsets, window)
                    arm_results[arm] = result
                    row = {
                        "scenario_id": f"s{scenario_index - 1:03d}",
                        "clip_id": clip["clip_id"],
                        "speaker_id": clip["speaker_id"],
                        "snr_db": snr,
                        "noise_type": noise_type,
                        "arm": arm,
                        "preset": PRESET[arm],
                        "stoi": result["stoi"],
                        "estoi": result["estoi"],
                        "valid": True,
                        "delta_stoi_vs_N_OFF": None,
                        "rms_in_dbfs": result["rms_in_dbfs"],
                        "rms_out_dbfs": result["rms_out_dbfs"],
                        "peak_dbfs": result["peak_dbfs"],
                        "clipped_samples_in": result["clipped_samples_in"],
                        "clipped_samples_out": result["clipped_samples_out"],
                        "dc_offset": result["dc_offset"],
                        "samples_in": result["samples_in"],
                        "samples_out": result["samples_out"],
                        "offset_samples": result["offset_samples"],
                        "window_samples": result["window_samples"],
                        "window_start_samples": result["window_start_samples"],
                        "window_end_samples": result["window_end_samples"],
                        "cpu_p50_ms": result["cpu_p50_ms"],
                        "cpu_p99_ms": result["cpu_p99_ms"],
                        "residual_dbfs": result["residual_dbfs"],
                        "band_300_2400_db": result["band_300_2400_db"],
                        "band_2400_2850_db": result["band_2400_2850_db"],
                        "qrm_suppression_db": result["qrm_suppression_db"],
                        "off_bit_exact": result["off_bit_exact"],
                        "output_sha256": result["output_sha256"],
                        "notes": "",
                    }
                    rows.append(row)
                    artifacts.append(result["output_sha256"])
                base_stoi = arm_results["N_OFF"]["stoi"]
                for row in rows[-4:]:
                    row["delta_stoi_vs_N_OFF"] = row["stoi"] - base_stoi

    return rows, invalid, artifacts


# --------------------------------------------------------------------------- #
# controles y determinismo
# --------------------------------------------------------------------------- #
def controls(clips: list[dict[str, Any]], scratch: Path, driver: Path, image: str, offsets: dict[str, int]) -> dict[str, Any]:
    clip = clips[0]
    ctrl_dir = scratch / "controls"
    ctrl_dir.mkdir(parents=True, exist_ok=True)

    raw = ctrl_dir / "voice.raw"
    raw.write_bytes(clip["s16"].tobytes())

    def call(out_name: str, preset: int, blocks: str) -> tuple[bytes, dict[str, Any]]:
        out = ctrl_dir / out_name
        cmd = driver_command(str(raw.relative_to(scratch)), str(out.relative_to(scratch)), preset, blocks)
        meta = driver_json(cmd)
        return out.read_bytes(), meta

    off_bytes, off_meta = call("off4410.raw", 0, "4410")
    ssb_a, _ = call("ssb4410.raw", 2, "4410")
    ssb_b, _ = call("ssb441.raw", 2, "441")
    ssb_c, _ = call("ssbmix.raw", 2, "1,440,441,442,257")

    filter_a = sosfilt(SOS_NORMAL, clip["voice"]).tobytes()
    filter_b = sosfilt(SOS_NORMAL, clip["voice"]).tobytes()

    self_ref = clip["voice"][GUARD : len(clip["voice"]) - GUARD]
    self_stoi = float(stoi(self_ref, self_ref, 44100, extended=False))

    shifted = np.concatenate([np.zeros(882), clip["voice"][:-882]])
    ref_win = clip["voice"][GUARD : len(clip["voice"]) - GUARD]
    proc_win = shifted[GUARD + 882 : len(shifted) - GUARD]
    n_common = min(len(ref_win), len(proc_win))
    shifted_stoi = float(stoi(ref_win[:n_common], proc_win[:n_common], 44100, extended=False))

    return {
        "off_unfiltered_bit_exact": off_bytes == raw.read_bytes(),
        "off_unfiltered_meta": off_meta,
        "proxy_filter_deterministic": filter_a == filter_b,
        "speex_partition_hashes": {
            "b4410": sha256_bytes(ssb_a),
            "b441": sha256_bytes(ssb_b),
            "bmixed": sha256_bytes(ssb_c),
        },
        "speex_partition_invariant": sha256_bytes(ssb_a) == sha256_bytes(ssb_b) == sha256_bytes(ssb_c),
        "stoi_self": self_stoi,
        "stoi_self_ok": abs(self_stoi - 1.0) <= 1e-3,
        "injected_delay_recovered": shifted_stoi > 0.99,
        "alignment": offsets,
    }


def determinism(rows_a: list[dict[str, Any]], rows_b: list[dict[str, Any]]) -> dict[str, Any]:
    key = lambda r: (r["scenario_id"], r["arm"])
    a = {key(r): r for r in rows_a}
    b = {key(r): r for r in rows_b}
    if set(a) != set(b):
        return {"equal": False, "reason": "conjuntos de filas distintos"}
    diffs = []
    hashes_ok = True
    for k in a:
        hashes_ok = hashes_ok and (a[k]["output_sha256"] == b[k]["output_sha256"])
        for field in ("stoi", "estoi", "rms_out_dbfs"):
            if abs(float(a[k][field]) - float(b[k][field])) > 1e-6:
                diffs.append((k, field, a[k][field], b[k][field]))
    return {
        "equal": hashes_ok and not diffs,
        "output_hashes_equal": hashes_ok,
        "metric_differences": diffs[:10],
        "rows": len(a),
    }


# --------------------------------------------------------------------------- #
# agregados
# --------------------------------------------------------------------------- #
def _cpu_stats(rows: list[dict[str, Any]]) -> dict[str, Any]:
    """CPU del driver por brazo; Speex separado de bypass (review #361)."""
    def block(selected: list[dict[str, Any]]) -> dict[str, Any]:
        if not selected:
            return {"n": 0}
        return {
            "n": len(selected),
            "p50_median_ms_per_callback": float(np.nanmedian([r["cpu_p50_ms"] for r in selected])),
            "p99_median_ms_per_callback": float(np.nanmedian([r["cpu_p99_ms"] for r in selected])),
            "p99_max_ms": float(np.nanmax([r["cpu_p99_ms"] for r in selected])),
        }
    per_arm = {arm: block([r for r in rows if r["arm"] == arm]) for arm in ARMS}
    speex = [r for r in rows if r["arm"] in ("N_SPEEX", "DX_SPEEX")]
    bypass = [r for r in rows if r["arm"] in ("N_OFF", "DX_OFF")]
    return {
        "callback_audio_ms": 100.0,
        "speex_active": block(speex),
        "bypass": block(bypass),
        "per_arm": per_arm,
        "note": "tiempo por callback de 4410 muestras (100 ms de audio); host x86_64, NO Cortex-A7; no mide latencia audible ni p99 de la radio",
    }


def aggregate(rows: list[dict[str, Any]]) -> dict[str, Any]:
    by_key = {(r["scenario_id"], r["arm"]): r for r in rows}
    scenario_ids = sorted({r["scenario_id"] for r in rows})
    stats: dict[str, Any] = {}
    speaker_of = {r["scenario_id"]: r["speaker_id"] for r in rows}

    for arm in ARMS:
        values = [by_key[(sid, arm)]["stoi"] for sid in scenario_ids]
        estoi = [by_key[(sid, arm)]["estoi"] for sid in scenario_ids]
        delta = [by_key[(sid, arm)]["stoi"] - by_key[(sid, "N_OFF")]["stoi"] for sid in scenario_ids]
        stats[arm] = {
            "median_stoi": float(np.median(values)),
            "p10_stoi": percentile(values, 10),
            "p90_stoi": percentile(values, 90),
            "median_estoi": float(np.median(estoi)),
            "median_delta_stoi_vs_N_OFF": float(np.median(delta)),
            "p10_delta": percentile(delta, 10),
            "p90_delta": percentile(delta, 90),
            "fraction_delta_gt_0": float(np.mean(np.asarray(delta) > 0)),
            "n": len(values),
        }

    units = sorted({r["speaker_id"] for r in rows})
    deltas_by_speaker = {sp: [] for sp in units}
    for sid in scenario_ids:
        sp = speaker_of[sid]
        deltas_by_speaker[sp].append(by_key[(sid, "N_SPEEX")]["stoi"] - by_key[(sid, "N_OFF")]["stoi"])

    rng = np.random.default_rng(SEED)
    boot = []
    for _ in range(1000):
        chosen = rng.choice(len(units), size=len(units), replace=True)
        pooled: list[float] = []
        for idx in chosen:
            pooled.extend(deltas_by_speaker[units[int(idx)]])
        boot.append(float(np.median(pooled)))

    return {
        "primary_metric": "STOI",
        "secondary": "ESTOI",
        "n_scenarios": len(scenario_ids),
        "n_speakers": len(units),
        "speakers": units,
        "arms": stats,
        "delta_N_SPEEX_vs_N_OFF": {
            "bootstrap_speakers": 1000,
            "seed": SEED,
            "ci95": [percentile(boot, 2.5), percentile(boot, 97.5)],
            "median": float(np.median(boot)),
        },
        "cpu_host": _cpu_stats(rows),
    }


# --------------------------------------------------------------------------- #
# main
# --------------------------------------------------------------------------- #
def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--corpus-dir", type=Path, default=Path(os.environ.get("RX349_CORPUS_DIR", DEFAULT_CORPUS)))
    parser.add_argument("--scratch", type=Path, default=Path(os.environ.get("RX349_SCRATCH", DEFAULT_SCRATCH)))
    parser.add_argument("--driver", type=Path, default=None)
    parser.add_argument("--image", default=DEFAULT_IMAGE)
    parser.add_argument("--skip-second-pass", action="store_true")
    parser.add_argument("--limit-clips", type=int, default=None)
    args = parser.parse_args()

    source_commit = run(["git", "-C", str(REPO), "rev-parse", "HEAD"]).stdout.strip()
    source_status = run(["git", "-C", str(REPO), "status", "--short"]).stdout.splitlines()
    source_tree_clean = not source_status
    source_scripts = script_hashes()

    scratch: Path = args.scratch
    scratch.mkdir(parents=True, exist_ok=True)
    driver = args.driver or (scratch / "bench" / "speex_driver")
    if not driver.is_file():
        raise SystemExit(f"BLOCKED: falta driver {driver}; correr scripts/build_speex_driver.sh")
    if not (scratch / "bench" / "build.json").is_file():
        raise SystemExit("BLOCKED: falta bench/build.json")
    global SCRATCH_GLOBAL, DRIVER_PATH, IMAGE_GLOBAL
    SCRATCH_GLOBAL = scratch
    DRIVER_PATH = driver
    IMAGE_GLOBAL = args.image

    clips = select_clips(args.corpus_dir)
    if args.limit_clips:
        clips = clips[: args.limit_clips]
    derived = scratch / "derived"
    loaded = [load_clip_44100(item, derived) for item in clips]
    align = alignment()

    rows_a, invalid_a, artifacts_a = run_matrix(loaded, scratch, driver, args.image, align["offsets"], "run_a")
    rows_b: list[dict[str, Any]] = []
    if not args.skip_second_pass:
        rows_b, _, _ = run_matrix(loaded, scratch, driver, args.image, align["offsets"], "run_b")
    det = determinism(rows_a, rows_b) if rows_b else {"equal": None, "reason": "segunda pasada omitida"}
    ctrl = controls(loaded, scratch, driver, args.image, align["offsets"])

    # validar Off bit-exacto en todas las filas Off (proxy incluido)
    off_rows = [r for r in rows_a if r["arm"] in ("N_OFF", "DX_OFF")]
    off_ok = all(r["off_bit_exact"] is True for r in off_rows)
    clipped = any((r["clipped_samples_in"] or 0) > 0 or (r["clipped_samples_out"] or 0) > 0 for r in rows_a)
    nan_bad = any(not np.isfinite(r["stoi"]) or not np.isfinite(r["estoi"]) for r in rows_a)

    status_ok = (
        len(rows_a) == 288
        and not invalid_a
        and off_ok
        and ctrl["off_unfiltered_bit_exact"]
        and ctrl["proxy_filter_deterministic"]
        and ctrl["speex_partition_invariant"]
        and ctrl["stoi_self_ok"]
        and ctrl["injected_delay_recovered"]
        and det.get("equal") is True
        and not nan_bad
        and not clipped
    )
    status = "DONE_AUTOMATED" if status_ok else "BLOCKED"

    manifest = {
        "issue": 349,
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "git_commit": source_commit,
        "git_status": source_status,
        "source_tree_clean_at_start": source_tree_clean,
        "script_hashes": source_scripts,
        "traceability": "git_commit/script_hashes identifican el codigo exacto con el que se generaron las 288 filas; para el rerun limpio source_tree_clean_at_start debe ser true. El commit final del PR solo agrega evidencia/docs si los hashes de scripts no cambian.",
        "status": status,
        "corpus": {
            "source": CORPUS_URL,
            "license": "LibriSpeech test-clean, CC BY 4.0 (OpenSLR SLR12)",
            "archive_md5_published": CORPUS_MD5,
            "archive_md5_computed": run(["md5sum", str(scratch / "corpus" / "test-clean.tar.gz")], check=False).stdout.split()[0]
            if (scratch / "corpus" / "test-clean.tar.gz").is_file()
            else "NOT_AVAILABLE",
            "archive_sha256": sha256_path(scratch / "corpus" / "test-clean.tar.gz")
            if (scratch / "corpus" / "test-clean.tar.gz").is_file()
            else "NOT_AVAILABLE",
            "clips": [
                {
                    "clip_id": c["clip_id"],
                    "speaker_id": c["speaker_id"],
                    "duration_s": round(c["duration"], 3),
                    "original_sha256": c["original_sha256"],
                    "converted_sha256": c["converted_sha256"],
                    "transcript": c["transcript"],
                }
                for c in loaded
            ],
        },
        "seed": SEED,
        "snr_db": list(SNRS),
        "noise_types": list(NOISE_TYPES),
        "arms": list(ARMS),
        "filters": {
            "normal_hz": [150, 2850],
            "dx_hz": [300, 2400],
            "butter_order": 4,
            "implementation": "scipy.signal.butter + sosfilt causal, estado inicial cero",
        },
        "speex": {
            line.split("=", 1)[0]: line.split("=", 1)[1].strip().strip("'\"")
            for line in (scratch / "bench" / "build.json").read_text().splitlines()
            if "=" in line
        },
        "source_sha256": {
            "audio_enhancement.c": sha256_path(REPO / "src" / "audio_enhancement.c"),
            "audio_enhancement.h": sha256_path(REPO / "src" / "audio_enhancement.h"),
        },
        "alignment": align,
        "dependencies": {
            "numpy": np.__version__,
            "scipy": __import__("scipy").__version__,
            "pystoi": __import__("pystoi").__version__,
            "soundfile": __import__("soundfile").__version__,
            "python": sys.version.split()[0],
        },
        "rows": len(rows_a),
        "counts": {"rows": len(rows_a), "invalid_scenarios": len(invalid_a), "off_rows": len(off_rows)},
    }

    if len(rows_a) != 288:
        status_ok = False
        status = "BLOCKED"

    (EVIDENCE_DIR / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    csv_fields = [
        "scenario_id", "clip_id", "speaker_id", "snr_db", "noise_type", "arm", "preset",
        "stoi", "estoi", "valid", "delta_stoi_vs_N_OFF", "rms_in_dbfs", "rms_out_dbfs",
        "peak_dbfs", "clipped_samples_in", "clipped_samples_out", "dc_offset", "samples_in",
        "samples_out", "offset_samples", "window_samples", "window_start_samples",
        "window_end_samples", "cpu_p50_ms", "cpu_p99_ms",
        "residual_dbfs", "band_300_2400_db", "band_2400_2850_db", "qrm_suppression_db",
        "off_bit_exact", "output_sha256", "notes",
    ]
    with (EVIDENCE_DIR / "measurements.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=csv_fields, quoting=csv.QUOTE_MINIMAL, lineterminator="\n")
        writer.writeheader()
        for row in rows_a:
            writer.writerow({k: row.get(k) for k in csv_fields})

    agg = aggregate(rows_a)
    agg["status"] = status
    agg["controls"] = ctrl
    agg["determinism"] = det
    agg["clipping_detected"] = clipped
    agg["gate_BASE"] = "BASE_NR_DFL_NOT_MEASURED"
    (EVIDENCE_DIR / "aggregate.json").write_text(json.dumps(agg, indent=2) + "\n", encoding="utf-8")

    commands = [
        "# Comandos realmente ejecutados por run_benchmark.py",
        f"# {time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}",
        f"python3 scripts/run_benchmark.py --corpus-dir {args.corpus_dir} --scratch {scratch} --image {args.image}",
        f"# exit={0 if status_ok else 1}  status={status}",
        f"# corpus md5={manifest['corpus']['archive_md5_computed']} (publicado {CORPUS_MD5})",
        f"# driver sha256={manifest['speex'].get('driver')} source_c={manifest['source_sha256']['audio_enhancement.c']}",
        f"# rows={len(rows_a)} invalid={len(invalid_a)} off_bit_exact={off_ok} determinism={det.get('equal')}",
    ]
    (EVIDENCE_DIR / "commands.log").write_text("\n".join(commands) + "\n", encoding="utf-8")

    print(json.dumps({"status": status, "rows": len(rows_a), "invalid": len(invalid_a), "off_ok": off_ok,
                      "determinism": det, "controls": {k: v for k, v in ctrl.items() if k != "alignment"},
                      "clipping": clipped, "nan": nan_bad}, indent=2))
    return 0 if status_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
