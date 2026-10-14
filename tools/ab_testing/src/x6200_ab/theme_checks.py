"""The theme the capture rendered must be one the firmware can keep.

`theme_list()` (`src/theme.c`) offers theme names up to 31 characters — the
built-ins and the JSON files — and the choice travels to the next boot inside
`params.theme_name`, a `params_str_t` (`src/params/types.h`). A name longer than
the store is truncated by `params_str_set()` and the next
`theme_init(params.theme_name.x)` (`src/main.c`) falls back to IU1WXY without
telling anyone: the user picked a theme and got another one. That was UI-013:
the store held 15 characters and "Outdoor-Negative" (16) did not fit. The fix
gave the store the 31 characters `theme_list()` admits (`params_str_t.x` is 32
bytes, one for the NUL).

The harness writes both names in the capture's `theme` block (`requested`, the
name that was rendered, and `params_name`, what the firmware's own store holds),
so this rule compares a measurement against a measurement — never a guess about
which one is "right". With no waiver left in `contracts.yaml`, it is the
permanent guard: a store that truncates again fails the fixture.
"""

from __future__ import annotations

from .capture import Capture
from .findings import ContractFinding

RULE_THEME_NAME = "theme_name_fits_params"


def run_theme_checks(capture: Capture) -> list[ContractFinding]:
    """One finding per capture: does the rendered theme fit `params.theme_name`?"""
    theme = capture.report.get("theme")
    if not isinstance(theme, dict):
        return [
            ContractFinding(
                rule=RULE_THEME_NAME,
                fixture=capture.name,
                ok=True,
                detail="no `theme` block in this capture (harness without the instrumentation)",
            )
        ]
    requested = str(theme.get("requested", ""))
    params_name = str(theme.get("params_name", ""))
    max_chars = int(theme.get("params_max_chars", 0) or 0)
    ok = bool(requested) and requested == params_name
    if ok:
        detail = f"theme {requested!r} fits params.theme_name ({max_chars} chars)"
    else:
        detail = (
            f"theme {requested!r} ({len(requested)} chars) does not fit "
            f"params.theme_name ({max_chars} chars): the store keeps {params_name!r} "
            f"and theme_init() falls back to IU1WXY (src/main.c)"
        )
    return [ContractFinding(rule=RULE_THEME_NAME, fixture=capture.name, ok=ok, detail=detail)]
