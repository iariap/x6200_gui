#!/usr/bin/env python3
"""Placeholder-preservation check for the copy-only change (issue #288).

The contract allows rewording user-visible literals but forbids changing the
format specifiers. For every string literal of the touched sources, this script
compares the multiset of `%` specifiers against `git show HEAD:<file>`: any
literal whose specifier sequence changed (count, order or conversion) is a
failure. It is a multiset over the whole file, which is enough because the
change only rewrites the text around the specifiers.

Usage: python3 verify_placeholders.py [--base <rev>]
       (run from the repository root; default base revision: HEAD)
"""

from __future__ import annotations

import argparse
import collections
import re
import subprocess
import sys

LITERAL = re.compile(r'"(?:\\.|[^"\\])*"')
SPEC = re.compile(r"%[-+ #0]*[0-9*]*(?:\.[0-9*]+)?(?:hh|h|ll|l|j|z|t|L)?[diouxXeEfgGaAcspn%]")
FILES = ("src/dialog_propagation.c", "src/dialog_swrscan.c", "src/dialog_field.c")


def specifiers(text: str) -> collections.Counter[tuple[str, ...]]:
    counter: collections.Counter[tuple[str, ...]] = collections.Counter()
    for literal in LITERAL.findall(text):
        found = tuple(SPEC.findall(literal))
        if found:
            counter[found] += 1
    return counter


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="HEAD", help="revision to compare against")
    args = parser.parse_args()

    failed = False
    for path in FILES:
        before = specifiers(
            subprocess.run(
                ["git", "show", f"{args.base}:{path}"],
                capture_output=True,
                text=True,
                check=True,
            ).stdout
        )
        after = specifiers(open(path, encoding="utf-8").read())

        removed, added = before - after, after - before
        literals = sum(before.values())
        specs = sum(len(key) * count for key, count in before.items())

        if removed or added:
            failed = True
            print(f"FAIL {path}")
            for key, count in removed.items():
                print(f"  removed x{count}: {key}")
            for key, count in added.items():
                print(f"  added   x{count}: {key}")
        else:
            print(f"OK   {path}: {literals} format literals, {specs} specifiers preserved")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
