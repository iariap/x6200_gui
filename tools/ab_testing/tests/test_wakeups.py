"""Non-e2e checks of the wakeup profile (issue #13, UI-07 remainder).

These tests do not build the harness: they check the committed evidence
(`reports/WAKEUPS-ECOFINAL/`) with the same checker the local gate runs
(`tests/check_harness_wakeups.sh` -> `tests/wakeup_evidence.py`), so the wakeup
mode's contract — deterministic, measured on the ROM's own refresh period and
tick mode, delimited by its data drive, and additive (a plain capture carries
no `wakeups` block) — is enforced on every `pytest -m "not e2e"` run. The checker
re-reads `lv_conf.h`, `src/main.c` and `src/dsp.cpp` from the repository, so the
evidence cannot drift from the firmware it describes. The harness itself is
exercised by the gate, which rebuilds it and runs it twice.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = REPO_ROOT / "tools" / "ab_testing" / "reports" / "WAKEUPS-ECOFINAL"
CHECKER = REPO_ROOT / "tests" / "wakeup_evidence.py"

# The keys the `wakeups` section carries. There is no wall clock in any of them:
# that is what makes two runs write the same bytes (the idempotency self-test),
# so a new timing field must fail here instead of silently breaking it.
WAKEUP_KEYS = {
    "enabled",
    "clock",
    "environment",
    "window_ms",
    "refresh_period_ms",
    "slice_ms",
    "tick_custom",
    "tick_thread_ms",
    "tick_thread_wakeups",
    "spectrum_fps_ms",
    "waterfall_fps_ms",
    "with_drive",
    "static",
}
PASS_KEYS = {
    "wakeups",
    "with_flush",
    "immediate",
    "no_timer",
    "sleep_ms_min",
    "sleep_ms_max",
    "sleep_ms_total",
    "flushed_px",
}

pytestmark = pytest.mark.skipif(
    not (EVIDENCE / "wakeups.json").is_file(),
    reason="the wakeup-profile evidence is not present",
)


def _load(name: str) -> dict:
    return json.loads((EVIDENCE / f"{name}.json").read_text())


def test_evidence_is_sourced_delimited_and_deterministic() -> None:
    """The checker the local gate runs, on the committed evidence."""
    result = subprocess.run(
        [sys.executable, str(CHECKER), str(EVIDENCE), str(REPO_ROOT)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    # Con LV_TICK_CUSTOM en el lv_conf.h de la ROM el hilo del tick no existe: el
    # perfil declara 0 wakeups de tick y el checker lo cruza contra esa fuente.
    assert "tick thread: none" in result.stdout
    assert "window override: --bench-wakeups 3000" in result.stdout


def test_wakeups_section_is_extra_and_carries_no_wall_clock() -> None:
    """The wakeup mode adds a block; it removes or renames nothing."""
    document = _load("wakeups")
    assert set(document["wakeups"]) == WAKEUP_KEYS
    assert document["wakeups"]["clock"] == "virtual-time"
    for name in ("with_drive", "static"):
        assert set(document["wakeups"][name]) == PASS_KEYS
    # The pre-existing block keeps its keys.
    assert set(document["metrics"]) == {
        "flushes",
        "flushed_pixels",
        "framebuffer_fnv1a",
        "png_path",
        "png_bytes",
    }


def test_the_tick_thread_is_gone_the_loop_remains() -> None:
    """El titular de #13, pineado sobre la evidencia: el hilo del tick se elimino
    con LV_TICK_CUSTOM (0 wakeups) y el lazo de la ROM queda como la unica fuente."""
    wakeups = _load("wakeups")["wakeups"]
    assert wakeups["tick_custom"] is True
    assert wakeups["tick_thread_ms"] == 0
    assert wakeups["tick_thread_wakeups"] == 0
    assert wakeups["with_drive"]["wakeups"] == 14
    assert wakeups["static"]["wakeups"] == 11
    # Sin vueltas sin sueño: cada vuelta contada es un ciclo dormir -> despertar.
    assert wakeups["with_drive"]["immediate"] == 0
    assert wakeups["with_drive"]["no_timer"] == 0
    # La delimitacion: el camino de datos agrega vueltas y trabajo de render.
    assert wakeups["static"]["flushed_px"] < wakeups["with_drive"]["flushed_px"]


def test_plain_capture_has_no_wakeups_block() -> None:
    """El modo es aditivo: la captura comun no lleva la seccion."""
    assert "wakeups" not in _load("plain")
