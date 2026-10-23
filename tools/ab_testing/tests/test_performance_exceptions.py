"""Performance exceptions: a dated, metric-bounded acceptance of a deviation.

`budgets.yaml` fixes the global render-cost budgets and is never edited to make a
run pass (rule 5 of `docs/AB_TESTING_PLAN.md`). A change that *knows* it moves the
deterministic counters — measured, understood and approved — is accepted through
the registry instead: one entry per scenario set, a ceiling per metric, a reason,
an author and an expiry. The gate reads an entry only while it is current; an
expired entry fails closed, a delta above the ceilings fails, and a scenario with
no entry keeps the global budget.

The numbers below are the real ones of `AB-SETEN109` (issue #109): reordering the
Settings tabs moved the dialog's first focus to the Freq-acceleration dropdown,
whose 80 ms theme transition costs a constant +8 flushes and +132610 flushed
pixels on the first open of every fixture that renders the window.
"""

from __future__ import annotations

import hashlib
from datetime import date
from pathlib import Path

import pytest

from helpers import QUALITY_DIR, blank_image, budgets, contracts, make_capture, revision
from x6200_ab.errors import ConfigError
from x6200_ab.evaluator import ComparisonData, evaluate
from x6200_ab.quality_config import (
    PerformanceCeilings,
    PerformanceException,
    PerformanceExceptions,
    load_budgets,
    load_performance_exceptions,
)
from x6200_ab.quality_gates import GateName, GateProfile, performance_gate
from x6200_ab.regions import RegionMap
from x6200_ab.reports import ReportMetadata, ReportPaths, render_markdown
from x6200_ab.status import Status
from x6200_ab.visual_diff import compute_diff

# The two magnitude bands of AB-SETEN109, exactly as measured: the three
# fixtures whose base is 7 flushes and the seven whose base is 6.
BAND_7 = ("settings-home", "settings-audio", "settings-wfbat")
BAND_6 = (
    "settings-perfil",
    "settings-tema-aurora",
    "settings-tema-cathode",
    "settings-tema-redshift",
    "settings-tema-sprint",
    "settings-tema-sunlit",
    "settings-tema-trail",
)
# AB-HOME250 (issue #250): el reloj permanente y su banda de estado de dos
# grupos, en la unica fixture con familia segmentada y con el reloj viejo en
# vista de hora.
BAND_250 = ("main-frecuencia-dseg",)

# scenario -> (flushed_pixels A, flushed_pixels B, flushes A, flushes B)
MEASURED: dict[str, tuple[int, int, int, int]] = {
    **dict.fromkeys(BAND_7, (1135170, 1267780, 7, 15)),
    **dict.fromkeys(BAND_6, (1003170, 1135780, 6, 14)),
    **dict.fromkeys(BAND_250, (1096000, 1100352, 4, 6)),
}

# La ficha que cada entrada tiene que nombrar en su `reason` (la procedencia de
# la aceptacion): una entrada sin issue no se puede revisar.
ISSUE_OF_AUTHOR = {
    "ajustes-tabs-109": "#109",
    "home-clock-battery-250": "#250",
}

# Cuantos flushes mas costaria que el MISMO cambio volviera a ocurrir encima del
# medido: es el "segundo efecto" con el que el techo se prueba acotado. Para
# AB-SETEN109 es otro cambio de foco animado (+8 flushes, el propio #109); para
# AB-HOME250, una banda de estado mas invalidando sus dos grupos (+2).
SECOND_EFFECT_FLUSHES = {
    "ajustes-tabs-109": 8.0,
    "home-clock-battery-250": 2.0,
}

# The digest of `quality/budgets.yaml` at the moment the registry was added. The
# budget file's own rule is that it is never edited to make a run pass, so a
# change to it must be a conscious, reviewed decision — this test makes the edit
# impossible to slip through.
BUDGETS_SHA256 = "b7e4630659f6689f1cbd97f5196169744f64d325e0c193f31a17e38fd0875ef5"


def _entry(
    scenarios: tuple[str, ...],
    *,
    pixels: float = 15.0,
    flushes: float = 150.0,
    expires: str = "2027-03-31",
) -> PerformanceException:
    return PerformanceException(
        scenarios=scenarios,
        ceilings=PerformanceCeilings(flushed_pixels_delta_pct=pixels, flushes_delta_pct=flushes),
        reason="AB-SETEN109 (#109): the Settings dialog's first focus is the dropdown",
        author="test",
        expires=expires,
    )


def _capture(tmp_path: Path, label: str, flushes: int, pixels: int):
    return make_capture(
        tmp_path / label,
        "settings-home",
        blank_image(),
        rev=revision(label, label * 40),
        metrics={
            "flushes": flushes,
            "flushed_pixels": pixels,
            "framebuffer_fnv1a": "0x0",
            "png_path": "",
            "png_bytes": 0,
        },
    )


def _gate(
    tmp_path: Path,
    *,
    flushes: tuple[int, int],
    pixels: tuple[int, int],
    exceptions: PerformanceExceptions,
    today: date | None = None,
):
    a = _capture(tmp_path, "a", flushes[0], pixels[0])
    b = _capture(tmp_path, "b", flushes[1], pixels[1])
    return performance_gate(
        {"settings-home": a},
        {"settings-home": b},
        max_flushed_pixels_delta_pct=10.0,
        max_flushes_delta_pct=20.0,
        exceptions=exceptions,
        today=today or date(2026, 10, 16),
    )


# --- the gate's verdict ------------------------------------------------------


def test_an_active_exception_inside_its_ceilings_passes_with_the_breach_shown(
    tmp_path: Path,
) -> None:
    """The breach is not hidden: it stays in the evidence with reason and expiry."""
    gate = _gate(
        tmp_path,
        flushes=(7, 15),
        pixels=(1_135_170, 1_267_780),
        exceptions=PerformanceExceptions((_entry(("settings-home",)),)),
    )
    assert gate.status is Status.PASS
    assert "accepted by a dated exception" in gate.summary
    joined = " ".join(gate.evidence)
    assert "flushes +114.3%" in joined
    assert "flushed_pixels +11.7%" in joined
    assert "2027-03-31" in joined
    record = gate.details["by_exception"][0]
    assert record["scenario"] == "settings-home"
    assert record["expires"] == "2027-03-31"
    assert record["reason"].startswith("AB-SETEN109 (#109)")
    # The global budget is untouched and still reported.
    assert gate.details["budget"]["flushes_delta_pct"] == 20.0


def test_an_expired_exception_fails_closed(tmp_path: Path) -> None:
    gate = _gate(
        tmp_path,
        flushes=(7, 15),
        pixels=(1_135_170, 1_267_780),
        exceptions=PerformanceExceptions((_entry(("settings-home",), expires="2020-01-01"),)),
        today=date(2026, 10, 16),
    )
    assert gate.status is Status.FAIL
    assert any("expired 2020-01-01" in reason for reason in gate.evidence)
    assert not gate.details.get("by_exception")


def test_a_delta_over_the_ceiling_fails(tmp_path: Path) -> None:
    """7 -> 22 flushes is +214.3%, above the 150% ceiling: FAIL, not a free pass."""
    gate = _gate(
        tmp_path,
        flushes=(7, 22),
        pixels=(1_135_170, 1_267_780),
        exceptions=PerformanceExceptions((_entry(("settings-home",)),)),
    )
    assert gate.status is Status.FAIL
    assert any("exception ceilings" in reason for reason in gate.evidence)
    assert not gate.details.get("by_exception")


def test_the_exception_must_cover_every_breached_metric(tmp_path: Path) -> None:
    """Flushes inside the ceiling is not enough if flushed_pixels is over its own."""
    gate = _gate(
        tmp_path,
        flushes=(7, 15),
        pixels=(1_135_170, 1_500_000),
        exceptions=PerformanceExceptions((_entry(("settings-home",)),)),
    )
    assert gate.status is Status.FAIL
    assert not gate.details.get("by_exception")


def test_a_scenario_without_an_entry_keeps_the_global_budget(tmp_path: Path) -> None:
    """An entry for another scenario does not excuse this one: FAIL, as today."""
    gate = _gate(
        tmp_path,
        flushes=(7, 15),
        pixels=(1_135_170, 1_267_780),
        exceptions=PerformanceExceptions((_entry(("settings-audio",)),)),
    )
    assert gate.status is Status.FAIL
    assert not gate.details.get("by_exception")
    assert any("budget 20.0%" in reason for reason in gate.evidence)


def test_a_glob_is_not_a_waiver() -> None:
    """Matching is exact (the house pattern of `RuleException`): a wildcard entry
    never matches a real scenario, so a waiver can never widen silently."""
    registry = PerformanceExceptions((_entry(("settings-tema-*",)),))
    assert registry.covering("settings-tema-aurora") is None


def test_without_a_registry_the_gate_is_unchanged(tmp_path: Path) -> None:
    a = _capture(tmp_path, "a", 7, 1_135_170)
    b = _capture(tmp_path, "b", 15, 1_267_780)
    gate = performance_gate(
        {"settings-home": a},
        {"settings-home": b},
        max_flushed_pixels_delta_pct=10.0,
        max_flushes_delta_pct=20.0,
    )
    assert gate.status is Status.FAIL
    assert gate.details["by_exception"] == []


# --- the evaluator wiring ----------------------------------------------------


def test_evaluate_applies_the_registry_to_the_performance_gate(
    tmp_path: Path, region_map: RegionMap
) -> None:
    a = _capture(tmp_path, "a", 7, 1_135_170)
    b = _capture(tmp_path, "b", 15, 1_267_780)
    diff = compute_diff(a, b, region_map, pixel_tolerance=12, min_box_pixels=4)
    data = ComparisonData(
        revision_a=a.revision,
        revision_b=b.revision,
        scenario_names=("settings-home",),
        a_captures={"settings-home": a},
        b_captures={"settings-home": b},
        diffs=(diff,),
        issues=(),
        contract_findings=(),
    )
    evaluation = evaluate(
        data,
        region_map=region_map,
        contracts=contracts(),
        budgets=budgets(),
        profile=GateProfile.host_ui(),
        experiment_id="AB-test",
        today=date(2026, 10, 16),
        performance_exceptions=PerformanceExceptions((_entry(("settings-home",)),)),
    )
    assert evaluation.status is Status.PASS
    assert evaluation.status.exit_code == 0
    gate = next(gate for gate in evaluation.gates if gate.name is GateName.PERFORMANCE)
    assert gate.status is Status.PASS
    assert gate.details["by_exception"][0]["scenario"] == "settings-home"


def test_the_report_shows_the_accepted_breach(tmp_path: Path, region_map: RegionMap) -> None:
    """The reason and the expiry are visible in `report.md`, not only in results.json."""
    a = _capture(tmp_path, "a", 7, 1_135_170)
    b = _capture(tmp_path, "b", 15, 1_267_780)
    diff = compute_diff(a, b, region_map, pixel_tolerance=12, min_box_pixels=4)
    data = ComparisonData(
        revision_a=a.revision,
        revision_b=b.revision,
        scenario_names=("settings-home",),
        a_captures={"settings-home": a},
        b_captures={"settings-home": b},
        diffs=(diff,),
        issues=(),
        contract_findings=(),
    )
    evaluation = evaluate(
        data,
        region_map=region_map,
        contracts=contracts(),
        budgets=budgets(),
        profile=GateProfile.host_ui(),
        experiment_id="AB-test",
        today=date(2026, 10, 16),
        performance_exceptions=PerformanceExceptions((_entry(("settings-home",)),)),
    )
    metadata = ReportMetadata(
        experiment_id="AB-test",
        date=evaluation.date,
        docker_image="x6200-uiharness",
        scenario_fixtures={"settings-home": "settings-home.scn"},
    )
    root = tmp_path / "report"
    paths = ReportPaths(
        root=root,
        markdown=root / "report.md",
        html=root / "report.html",
        results=root / "results.json",
        metadata=root / "metadata.json",
        screenshots=root / "screenshots",
        diffs=root / "diffs",
        metrics=root / "metrics",
        logs=root / "logs",
    )
    markdown = render_markdown(evaluation, metadata, paths)
    assert "PASS (by exception)" in markdown
    assert "+114.3%" in markdown
    assert "2027-03-31" in markdown


# --- the registry loader -----------------------------------------------------


def test_the_loader_reads_a_registry(tmp_path: Path) -> None:
    path = tmp_path / "performance_exceptions.yaml"
    path.write_text(
        "exceptions:\n"
        "  - scenarios: [settings-home, settings-audio]\n"
        "    ceilings:\n"
        "      flushed_pixels_delta_pct: 15.0\n"
        "      flushes_delta_pct: 130.0\n"
        "    reason: AB-SETEN109 (#109)\n"
        "    author: ajustes-tabs-109\n"
        "    expires: 2027-03-31\n"
    )
    registry = load_performance_exceptions(path)
    entry = registry.entries[0]
    assert entry.scenarios == ("settings-home", "settings-audio")
    assert entry.ceilings.flushes_delta_pct == 130.0
    assert entry.active(date(2027, 3, 31)) is True
    assert entry.active(date(2027, 4, 1)) is False


@pytest.mark.parametrize(
    "body",
    [
        "exceptions: not-a-list\n",
        (
            "exceptions:\n"
            "  - scenarios: [a]\n"
            "    reason: r\n"
            "    author: a\n"
            "    expires: 2027-03-31\n"
        ),
        (
            "exceptions:\n"
            "  - scenarios: []\n"
            "    ceilings: {flushed_pixels_delta_pct: 15.0, flushes_delta_pct: 150.0}\n"
            "    reason: r\n"
            "    author: a\n"
            "    expires: 2027-03-31\n"
        ),
        (
            "exceptions:\n"
            "  - scenarios: [a]\n"
            "    ceilings: {flushed_pixels_delta_pct: -1.0, flushes_delta_pct: 150.0}\n"
            "    reason: r\n"
            "    author: a\n"
            "    expires: 2027-03-31\n"
        ),
        (
            "exceptions:\n"
            "  - scenarios: [a]\n"
            "    ceilings: {flushed_pixels_delta_pct: 15.0, flushes_delta_pct: 150.0}\n"
            "    reason: r\n"
            "    author: a\n"
            "    expires: soon\n"
        ),
        (
            "exceptions:\n"
            "  - scenarios: [a]\n"
            "    ceilings: {flushed_pixels_delta_pct: 15.0, flushes_delta_pct: 150.0}\n"
            "    reason: r\n"
            "    author: a\n"
            "    expires: 2027-03-31\n"
            "  - scenarios: [a]\n"
            "    ceilings: {flushed_pixels_delta_pct: 15.0, flushes_delta_pct: 150.0}\n"
            "    reason: r\n"
            "    author: a\n"
            "    expires: 2028-03-31\n"
        ),
    ],
)
def test_the_loader_rejects_malformed_entries(tmp_path: Path, body: str) -> None:
    path = tmp_path / "performance_exceptions.yaml"
    path.write_text(body)
    with pytest.raises(ConfigError):
        load_performance_exceptions(path)


# --- the repository's own registry -------------------------------------------


def test_the_repo_registry_is_scoped_dated_and_names_the_issue() -> None:
    registry = load_performance_exceptions(QUALITY_DIR / "performance_exceptions.yaml")
    assert registry.entries
    for entry in registry.entries:
        assert entry.scenarios
        assert entry.reason.strip()
        assert ISSUE_OF_AUTHOR[entry.author] in entry.reason, (
            f"{entry.scenarios[0]}: el reason tiene que nombrar el issue de la entrada"
        )
        assert entry.author
        assert date.fromisoformat(entry.expires) >= date(2026, 10, 16)
        assert entry.ceilings.flushed_pixels_delta_pct > 0
        assert entry.ceilings.flushes_delta_pct > 0


def test_the_repo_registry_admits_the_measured_breach_and_stays_bounded() -> None:
    registry = load_performance_exceptions(QUALITY_DIR / "performance_exceptions.yaml")
    covered = {name for entry in registry.entries for name in entry.scenarios}
    assert covered == set(MEASURED)
    for name, (pixels_a, pixels_b, flushes_a, flushes_b) in MEASURED.items():
        pixels_delta = (pixels_b - pixels_a) * 100.0 / pixels_a
        flushes_delta = (flushes_b - flushes_a) * 100.0 / flushes_a
        entry = registry.covering(name)
        assert entry is not None
        assert entry.allows(pixels_delta, flushes_delta), f"{name}: measured delta is not admitted"
        # Not a free pass: the same effect happening once more lands above the
        # ceiling (see SECOND_EFFECT_FLUSHES).
        second_effect = SECOND_EFFECT_FLUSHES[entry.author] * 100.0 / flushes_a
        assert not entry.allows(pixels_delta, flushes_delta + second_effect), (
            f"{name}: the ceiling is unbounded"
        )


def test_the_global_budget_file_is_untouched() -> None:
    path = QUALITY_DIR / "budgets.yaml"
    assert hashlib.sha256(path.read_bytes()).hexdigest() == BUDGETS_SHA256
    loaded = load_budgets(path)
    assert loaded.performance.max_flushed_pixels_delta_pct == 10.0
    assert loaded.performance.max_flushes_delta_pct == 20.0
