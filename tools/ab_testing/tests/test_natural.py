"""Non-e2e checks of the natural-invalidation bench (UI-07 increment 2).

These tests do not build the harness: they check the committed evidence
(`reports/NATURAL-ECOFINAL/`) with the same checker the local gate runs
(`tests/check_harness_natural.sh` -> `tests/natural_evidence.py`), so the natural
mode's contract — deterministic, profiled against UI-07 increment 1, delimited by
its data drive, and compatible with the existing `metrics`/`benchmark` blocks — is
enforced on every `pytest -m "not e2e"` run. The harness itself is exercised by
the gate, which rebuilds it and runs it twice.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = REPO_ROOT / "tools" / "ab_testing" / "reports" / "NATURAL-ECOFINAL"
CHECKER = REPO_ROOT / "tests" / "natural_evidence.py"

# The keys the `natural` section carries. There is no wall clock in any of them:
# that is what makes two runs write the same bytes (the idempotency self-test),
# so a new timing field must fail here instead of silently breaking it.
NATURAL_KEYS = {
    "enabled",
    "clock",
    "environment",
    "frame_ms",
    "slice_ms",
    "frames",
    "data_drive",
    "seed",
    "spectrum_fps_ms",
    "waterfall_fps_ms",
    "spectrum_frames",
    "waterfall_rows",
    "waterfall_offscreen_redraws",
    "per_frame",
    "totals",
}
PER_FRAME_KEYS = {
    "frame",
    "virtual_end_ms",
    "refresh_cycles",
    "invalidated_px",
    "invalidated_rects",
    "joined_px",
    "flushed_px",
    "flushes",
    "rects_complete",
    "rects",
}

pytestmark = pytest.mark.skipif(
    not (EVIDENCE / "natural.json").is_file(),
    reason="the natural-bench evidence is not present",
)


def _load(name: str) -> dict:
    return json.loads((EVIDENCE / f"{name}.json").read_text())


def test_evidence_is_profiled_delimited_and_deterministic() -> None:
    """The checker the local gate runs, on the committed evidence."""
    result = subprocess.run(
        [sys.executable, str(CHECKER), str(EVIDENCE)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "spectrum 132000 px x 16 frames" in result.stdout
    assert "waterfall 196000 px x 23 rows" in result.stdout
    assert "clock 15840 px x 1 tick(s)" in result.stdout


def test_natural_section_is_extra_and_carries_no_wall_clock() -> None:
    """The natural mode adds a block; it removes or renames nothing."""
    document = _load("natural")
    assert set(document["natural"]) == NATURAL_KEYS
    assert document["natural"]["clock"] == "virtual-time"
    for frame in document["natural"]["per_frame"]:
        assert set(frame) == PER_FRAME_KEYS
        assert frame["flushes"] == frame["invalidated_rects"]
        assert frame["invalidated_px"] == frame["flushed_px"]
    # The pre-existing block keeps its keys and its meaning.
    assert set(document["metrics"]) == {
        "flushes",
        "flushed_pixels",
        "framebuffer_fnv1a",
        "png_path",
        "png_bytes",
    }
    totals = document["natural"]["totals"]
    assert document["metrics"]["flushes"] == 6 + totals["flushes"]
    assert document["metrics"]["flushed_pixels"] == 1127680 + totals["flushed_px"]


def test_forced_mode_is_unchanged_by_the_natural_one() -> None:
    """`--bench` still fabricates a full-screen frame and emits no `natural` block."""
    forced = _load("forced")
    assert "natural" not in forced
    assert all(
        frame["flushes"] == 1 and frame["redrawn_area_px"] == 800 * 480
        for frame in forced["benchmark"]["frames"]
    )


def test_static_control_has_no_data_regions() -> None:
    """With the drive off only the clock/BT/band-info rectangles remain."""
    control = _load("static")["natural"]
    assert control["data_drive"] is False
    assert control["spectrum_frames"] == 0
    assert control["waterfall_rows"] == 0
    rectangles = {tuple(rect) for frame in control["per_frame"] for rect in frame["rects"]}
    assert rectangles.isdisjoint({(0, 0, 800, 165), (0, 235, 800, 245)})


def test_display_off_evidence_renders_nothing() -> None:
    """`--display-off` (issue #13): el panel oscuro no hace trabajo de render.

    La propiedad la mide el `natural` block (todo en cero) y el test nativo del
    backlight; aca queda pineada sobre la evidencia, porque la captura de la
    pantalla apagada es exactamente un repaint de pantalla completa mas liviana
    (1127680 - 384000 = 743680 px) y el mismo PNG que con la pantalla encendida. El
    instrumento del redibujo offscreen de la cascada (src/waterfall.c) queda en 0
    contra una por fila con la pantalla encendida: el panel oscuro saltea el computo
    de la paleta (~196000 px por fila) en vez de hacerlo para nada.
    """
    document = _load("off")
    natural = document["natural"]
    assert natural["data_drive"] is True
    assert natural["spectrum_frames"] > 0 and natural["waterfall_rows"] > 0
    assert natural["waterfall_offscreen_redraws"] == 0
    assert set(natural["totals"].values()) == {0}
    assert document["metrics"]["flushes"] == 6 - 1
    assert document["metrics"]["flushed_pixels"] == 1127680 - 800 * 480
    lit = _load("forced")["metrics"]["framebuffer_fnv1a"]
    assert document["metrics"]["framebuffer_fnv1a"] == lit
