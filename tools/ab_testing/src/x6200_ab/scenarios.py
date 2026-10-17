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
        # La familia segmentada de los paneles de dígitos (fila de frecuencia y
        # reloj): un id FONT_DIGIT_* del arnés (src/fonts/font_families.h,
        # `--digits` en tools/ui_harness/README.md). Vacío o `none` = la familia
        # por defecto.
        "digits",
        # La vista del reloj de la barra superior (src/clock.c): `time`, `power`
        # (el defecto del arnés) o `time-power`. La fixture de la familia de
        # dígitos pide `time` porque es la vista que lleva el "8 fantasma".
        "clock_view",
        "spectrum_filled",
        "spectrum_peak",
        "waterfall_center_line",
        "waterfall_battery",
        "band_plan",
        # Las dos ganancias de Ajustes -> Audio (la fila "Play,Rec gain"): el
        # fixture de audio las fija para poder distinguir un slider del otro.
        "play_gain_db",
        "rec_gain_db",
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
        # Natural-bench-only knob (ignored outside `--bench-natural`; never
        # changes a pixel either).
        "natural_frames",
        # Which APP page an `apps-menu` fixture opens (1..6, UI-002).
        "apps_page",
        # El backend fake de adquisicion de ROE del arnes (issue #125, la pantalla
        # `swrscan`): `swr_dataset` es uno de los datasets de
        # `tests/fixtures/swrscan/`, `swr_passes` la cantidad de pasadas que pide el
        # fixture (1, 3 o 5; la ventana decide igual si la sesion arranca),
        # `swr_feed` cuantos paquetes del flow se entregan antes de cortar (0 = la
        # sesion entera) y `swr_stop` aprieta Stop al final. El centro y el span de
        # la grilla NO son claves: el backend los saca del dataset.
        "swr_dataset",
        "swr_passes",
        "swr_feed",
        "swr_stop",
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


def split_by_fixtures_dir(
    suite: ScenarioSuite, fixtures_dir: Path
) -> tuple[ScenarioSuite, tuple[str, ...]]:
    """Split a suite into the scenarios a fixtures dir ships and the names it does not.

    The comparison suite comes from A. A change may legitimately drop a fixture in B (a
    screen B no longer ships), and then that scenario cannot be rendered on B: it is
    reported and left out of the run instead of aborting the whole comparison on a
    missing file. A suite that no longer has anything to render is an error.
    """
    kept: list[Scenario] = []
    removed: list[str] = []
    for scenario in suite.scenarios:
        if (fixtures_dir / scenario.filename).is_file():
            kept.append(scenario)
        else:
            removed.append(scenario.name)
    if not kept:
        raise HarnessError(
            f"none of the {len(suite.scenarios)} scenario(s) of the suite exist in {fixtures_dir}"
        )
    return ScenarioSuite(scenarios=tuple(kept)), tuple(removed)
