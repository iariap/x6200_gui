# Spec: onboarding-x6200 — add x6200_gui to the app registry

**Date**: 2026-10-14 · **Run**: `factory/onboarding-x6200` · **App**: asf
(harness)

## What is wanted and why (outcome)

Bring the Xiegu X6200 ROM (`iariap/x6200_gui`, trunk `lu2dsx/x6200-rom`) into the
harness as a first-class application, so `/factory-build|review|deploy` runs
operate on it with the same guarantees as mi-mesa and radio-contest: a
resolvable registry, path-based risk policy, and a check matrix that reuses the
**existing LVGL 8 UI A/B harness** (merged into `lu2dsx/x6200-rom` through
PR #1: `tools/ui_harness/`, `tools/ab_testing/`, native and static `tests/`).

The work is **harness governance** (a high-risk change under policy; requires
human approval at merge) plus the metadata files in the app repo.

## Scope

1. **Harness pointer**: `config/apps/x6200.json` (minimal, following the other
   apps: id/repo/remote/branch/checkout/note). Base checkout:
   `work/x6200_gui`; trunk: `lu2dsx/x6200-rom`; remote:
   `https://github.com/iariap/x6200_gui.git`.
2. **App metadata**: `.factory/app.json` + `.factory/policy.json` in the app
   repo (overlay: wins over the pointer; `checks` lives there, mi-mesa pattern).
   Path policy with a ROM focus: firmware `src/**` = high; pinned `lvgl`
   submodule v8.3.11 = high (do not update); `lv_conf.h`, drivers, and buildroot
   = high/medium; A/B harness (`tools/ab_testing/**`, `tests/**`, `docs/**`) =
   low; `tools/ui_harness/**` = medium (C + Docker).
3. **Check matrix**: `scripts/apps/x6200/check-matrix.sh <base> <head>
   [checkout]` (mi-mesa interface), path-selective: firmware changes → native
   tests (`tests/run_native_tests.sh`) + repo static checks; framework Python
   changes → ruff + pytest `-m "not e2e"` (uv); whenever the harness touches the
   render path → determinism gate
   (`tests/check_harness_determinism.sh`). No X11/GPU or heavy e2e fixtures in
   the local matrix.
4. **Transitional fallback**: `config/policies/x6200.json` with the same policy
   (AGENTS.md: apps without a synced checkout fall back here).
5. **PRs**: (a) harness `factory/onboarding-x6200` → `main` with 1–4 + green
   harness tests; (b) app `factory/onboarding-x6200` → `lu2dsx/x6200-rom` in the
   fork with 2. Review/merge: human.

## Do not

- Touch firmware `src/` or the `lvgl` submodule (pinned `74d0a816`).
- Create Cloud Run environments or `metrics`: the app runs on hardware; there is
  no real health_URL (if anything, document it as PENDING).
- Promise M7 (hardware/power) coverage: onboarding covers what already exists in
  the merged ROM.
- Implement in `work/x6200_gui` (base checkout, sync only).

## Acceptance criteria (verifiable finish condition)

From the harness root with `config/apps/x6200.json` in place:

```bash
# 1. The registry resolves the app (absolute paths, trunk, checks).
python3 scripts/app_config.py x6200 --sh          # exit 0, exports APP_*

# 2. The app overlay wins over the pointer (with a synced checkout).
scripts/sync-work.sh x6200 && python3 scripts/app_config.py x6200 --print checks
#   → scripts/apps/x6200/check-matrix.sh

# 3. The matrix runs and is green over lu2dsx/x6200-rom.
scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD "$APP_CHECKOUT"

# 4. Harness gates with no new broken frame.
uv run ruff check scripts/ && uv run ty check scripts/
python3 scripts/test_harness.py                    # full suite, 0 failures
```

Duration does not count as finish. Everything above passes or fails.
