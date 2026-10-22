# Vendored build headers

These headers belong to build dependencies of the firmware that are **not**
part of this repository (they come from the Buildroot build) and that the host
UI harness needs only to *compile* the real GUI sources:

| Path | Origin | SPDX |
|---|---|---|
| `aether_radio/x6200_control/api.h` | `aether-x6200-control` (Buildroot package, https://github.com/iu2frl/AetherX6200) | LGPL-2.1-or-later |
| `aether_radio/x6200_control/control.h` | same | LGPL-2.1-or-later |
| `aether_radio/x6200_control/low/control.h` | same | LGPL-2.1-or-later |
| `aether_radio/x6200_control/low/gpio.h` | same | LGPL-2.1-or-later |
| `ft8lib/constants.h` | `ft8_lib` (LGPL-2.1-or-later) | LGPL-2.1-or-later |

`src/params/params.h`, `src/radio.h`, `src/cfg/*` include
`<aether_radio/x6200_control/control.h>` and `<ft8lib/constants.h>`. The
header declares the GPIO pins the firmware names (`x6200_pin_light` for the
buttons light, `aether_radio/x6200_control/low/gpio.h`) and
`src/backlight.c` includes it; the harness does not link the SDK, so the
pin variables and `gpio_safe_set()` are the explicit host stubs of
`mocks/mock_ui_deps.c`. The
device build gets them from `${CMAKE_SYSROOT}/usr/include`; this host build
does not have the sysroot, so the exact upstream headers are vendored here so
the harness builds from a clean checkout with no external dependency.

They are copied verbatim. If a build-root upgrade changes them, re-copy from
`${CMAKE_SYSROOT}/usr/include/` of the matching image.
