"""WCAG 2.1 relative luminance and contrast ratio.

One implementation for the two families that need it: the rendered typography
contract (`typography_checks.py`, colours read from the live LVGL object tree)
and the static contract over the theme JSON files the ROM ships
(`theme_files.py`). Two copies of the formula could silently disagree, and a
contrast threshold that disagrees with itself is worse than no threshold.

Colours are `#rrggbb`; `hex_colour()` converts a `0xRRGGBB` value (the form the
built-in palettes in `src/theme.h` use).
"""

from __future__ import annotations


def luminance(colour: str) -> float:
    """Relative luminance of `#rrggbb`, as WCAG 2.1 defines it."""
    value = colour.lstrip("#")
    channels = [int(value[i : i + 2], 16) / 255.0 for i in (0, 2, 4)]
    linear = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in channels]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast_ratio(first: str, second: str) -> float:
    """Contrast ratio between two `#rrggbb` colours, always >= 1."""
    high, low = luminance(first), luminance(second)
    if high < low:
        high, low = low, high
    return (high + 0.05) / (low + 0.05)


def hex_colour(value: int) -> str:
    """`0xRRGGBB` as the `#rrggbb` string the rest of the framework uses."""
    return f"#{value & 0xFFFFFF:06x}"
