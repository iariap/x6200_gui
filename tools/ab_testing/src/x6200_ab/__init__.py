"""A/B testing framework for the X6200 UI harness.

The framework lives on the host: it never ships in the firmware or the rootfs.
It drives the C harness (`tools/ui_harness`) per git revision, diffs the PNG
captures, runs quality contracts and known-issue detectors, and turns all of
that into a PASS/FAIL/BLOCKED/INCONCLUSIVE/REVIEW REQUIRED verdict with
evidence. See `docs/AB_TESTING_PLAN.md`.
"""

__all__ = ["__version__"]

__version__ = "0.1.0"
