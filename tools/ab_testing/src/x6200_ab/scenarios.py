"""Scenario parser and suite for the harness fixtures (`tools/ui_harness/fixtures`).

A scenario is the `key value` file `tools/ui_harness/src/scenario.c` parses:
unknown keys are an error there, so a fixture the parser does not understand is
reported (BLOCKED) rather than silently skipped. The suite is every `*.scn`
file in the fixtures directory, or an explicit subset by name.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .errors import HarnessError

# Keys scenario.c knows about (see the README table); kept in sync with it.
KNOWN_KEYS = frozenset(
    {
        "name",
        "screen",
        "seed",
        "theme",
        "spectrum_filled",
        "spectrum_peak",
        "waterfall_center_line",
        "band_plan",
        "frequency_hz",
        "tx_frequency_hz",
        "fft_width_hz",
        "mode",
        "vfo",
        "agc",
        "split",
        "att",
        "pre",
        "atu",
        "ant",
        "smeter_db",
        "battery_v",
        "external_v",
        "battery_cap",
        "charging",
        "rxtx",
        # Benchmark-only knobs (ignored outside `--bench`; never change a pixel).
        "bench_warmup",
        "bench_repeats",
        "bench_redraw_passes",
    }
)


@dataclass(frozen=True)
class Scenario:
    """One fixture: its name, screen, seed and file name inside a revision."""

    name: str
    screen: str
    filename: str
    seed: int | None
    raw: dict[str, str]


def parse_scenario(path: Path) -> Scenario:
    """Parse one `.scn` file, rejecting unknown keys and missing name/screen."""
    values: dict[str, str] = {}
    for lineno, raw_line in enumerate(path.read_text().splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            raise HarnessError(f"{path}:{lineno}: expected `key value`, got {raw_line!r}")
        key, value = parts[0], parts[1].strip()
        if key not in KNOWN_KEYS:
            raise HarnessError(f"{path}:{lineno}: unknown scenario key `{key}`")
        values[key] = value
    for required in ("name", "screen"):
        if required not in values:
            raise HarnessError(f"{path}: missing required key `{required}`")
    seed = None
    if "seed" in values:
        try:
            seed = int(values["seed"])
        except ValueError as exc:
            raise HarnessError(f"{path}: seed is not an integer: {values['seed']!r}") from exc
    return Scenario(
        name=values["name"],
        screen=values["screen"],
        filename=path.name,
        seed=seed,
        raw=values,
    )


@dataclass(frozen=True)
class ScenarioSuite:
    """The set of fixtures to capture for a revision."""

    scenarios: tuple[Scenario, ...]

    @property
    def names(self) -> list[str]:
        return [scenario.name for scenario in self.scenarios]

    @classmethod
    def from_fixtures_dir(cls, fixtures_dir: Path, names: list[str] | None = None) -> ScenarioSuite:
        """Load all fixtures, or the subset named in `names`.

        An unknown name is an error: a typo must not silently drop a fixture
        from the comparison.
        """
        if not fixtures_dir.is_dir():
            raise HarnessError(f"fixtures directory not found: {fixtures_dir}")
        available = {
            parsed.name: parsed
            for parsed in (parse_scenario(p) for p in sorted(fixtures_dir.glob("*.scn")))
        }
        if names:
            missing = [name for name in names if name not in available]
            if missing:
                raise HarnessError(f"unknown scenario(s) {missing}; available: {sorted(available)}")
            selected = tuple(available[name] for name in names)
        else:
            selected = tuple(available[name] for name in sorted(available))
        if not selected:
            raise HarnessError(f"no scenarios found in {fixtures_dir}")
        return cls(scenarios=selected)
