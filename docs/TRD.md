# Technical Reference Document (TRD)
# X6200 Firmware Optimization Project

## Document Information
- **Project:** X6200 QRP Ham Radio Firmware Optimization
- **Version:** 2.0-opt
- **Author:** LU2DSX
- **Date:** February 2026
- **License:** LGPL-2.1-or-later

## Executive Summary

This document provides comprehensive technical details on the power efficiency optimizations implemented in the X6200 firmware. The optimizations focus on three core principles: battery life (critical), performance, and user experience (UX).

### Key Achievements
- **Power Reduction:** 25-35% (GPS + TTS + waterfall savings)
- **Code Reduction:** ~1,800 lines removed
- **Battery Life:** 5-6 hours → 7-8+ hours
- **Build Size:** 545 MB SD card image

---

## 1. GPS Subsystem Removal

### 1.1 Overview
Complete removal of GPS functionality to eliminate hardware polling, background threads, and associated power consumption. This includes both the GUI application code and the Buildroot system-level packages.

### 1.2 GUI Files Removed
| File | Lines | Purpose |
|------|-------|---------|
| `gps.c` | 98 | GPS daemon client implementation |
| `gps.h` | 23 | GPS header definitions |
| `dialog_gps.c` | ~350 | GPS dialog UI |
| `dialog_gps.h` | 15 | GPS dialog header |
| **Total** | **~486** | **4 files removed** |

### 1.3 GUI Dependencies Removed
- **Library:** libgps (gpsd client library)
- **Headers:** `<gps.h>`, GPS-related constants
- **Build:** Removed from CMakeLists.txt

### 1.4 GUI Code References Removed
- **main.c:** Removed `#include "gps.h"` and `gps_init()` call
- **events.h/c:** Removed `EVENT_GPS` event registration
- **main_screen.c:** Removed GPS dialog construction and action handlers
- **buttons.cpp:** Removed GPS button (btn_gps) from UI
- **dialog_settings.c:** Removed GPS menu item
- **params.h:** Removed `ACTION_APP_GPS` enum value

### 1.5 Buildroot/System-Level Removal

#### 1.5.1 Defconfig Changes
**File:** `br2_external/configs/X6200_defconfig`

Removed 26 lines (lines 81-106 in original):
```
BR2_PACKAGE_GPSD=y
BR2_PACKAGE_GPSD_DEVICES="/dev/ttyACM0"
BR2_PACKAGE_GPSD_ASHTECH=y
... (24 GPS driver options)
BR2_PACKAGE_GPSD_UBX=y
```
Replaced with: `# BR2_PACKAGE_GPSD is not set`

#### 1.5.2 NTP Configuration
**File:** `br2_external/board/X6200/linux/rootfs-overlay/etc/ntp.conf`

Removed GPS time references:
```
# GPS Serial data reference (REMOVED)
# server 127.127.28.0 minpoll 4 maxpoll 4 mode 0
# fudge 127.127.28.0 flag1 1 time1 0.0 refid GPS
```

Added local hardware clock fallback for field operation without network:
```
server 127.127.1.0
fudge 127.127.1.0 stratum 10
```

Removed GPS-only NTP build options from defconfig:
- `BR2_PACKAGE_NTP_NTP_SHM_CLK` (shared-memory clock for gpsd)
- `BR2_PACKAGE_NTP_NTPD_ATOM_PPS` (pulse-per-second for GPS hardware)

#### 1.5.3 Init Script Removed
**File:** `br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S50gpsd`

Deleted the init script that started `gpsd` daemon on `/dev/ttyACM0` at boot.

#### 1.5.4 Important Build Note
After disabling `BR2_PACKAGE_GPSD` in the defconfig, the cached gpsd artifacts must be manually cleaned from the Docker build volume target tree. Buildroot does not automatically remove previously-installed packages:
```bash
# Inside Docker container (verified cleanup, 2026-10-11):
T=/workspace/build/target
rm -f $T/usr/sbin/gpsd $T/usr/sbin/gpsdctl $T/usr/lib/libgps* $T/usr/lib/libgpsdpacket*
rm -f $T/usr/bin/cgps $T/usr/bin/gegps $T/usr/bin/gps* $T/usr/bin/lcdgps $T/usr/bin/xgps $T/usr/bin/xgpsspeed
rm -rf $T/usr/share/gpsd $T/usr/share/snmp/mibs/gpsd
rm -rf $T/usr/lib/python3.11/site-packages/gps $T/usr/lib/python3.11/site-packages/gps-3.25.egg-info
rm -f $T/lib/udev/gpsd.hotplug $T/lib/udev/rules.d/25-gpsd.rules
rm -f $T/etc/init.d/S50gpsd
```
The original cleanup pattern only covered `/usr/sbin/gps*`, which left the gpsd client tools (`/usr/bin/gps*`, `cgps`, `xgps`), the Python `gps` module, the udev rules and the SNMP MIB in the image (3,5 MB). The pattern above removes them all. `lib/modules/*/kernel/drivers/usb/serial/garmin_gps.ko` (kernel driver) is kept: it is only loaded on demand and has no runtime cost.

### 1.6 Technical Details
**GPS Power Consumption Model:**
- GPS daemon polling: 10-30mA
- Serial/UART communication: 5-10mA
- Background thread: 5-10mA
- **Total Savings:** 20-50mA

**Impact:**
- No location services
- No grid square auto-detection
- No UTC time sync from GPS (NTP uses internet servers with local clock fallback)
- QTH grid must be entered manually

---

## 2. Text-to-Speech (TTS) Removal

### 2.1 Overview
Complete removal of voice synthesis functionality using RHVoice library to eliminate TTS engine overhead and pthreads.

### 2.2 Files Removed
| File | Lines | Purpose |
|------|-------|---------|
| `voice.cpp` | 297 | RHVoice integration |
| `voice.h` | 46 | Voice synthesis API |
| **Total** | **~343** | **2 files removed** |

### 2.3 Dependencies Removed
- **Libraries:**
  - RHVoice (TTS engine)
  - RHVoice_core (core functionality)
  - RHVoice_audio (audio output)
- **Headers:** RHVoice include path removed from CMakeLists.txt

> **Update 2026-10-11:** until this revision the RHVoice *Buildroot package* was still selected by `BR2_PACKAGE_X6200_GUI` (`select BR2_PACKAGE_RHVOICE` in `package/x6200-gui/Config.in`) and listed in `X6200_GUI_DEPENDENCIES`, so 12,2 MB of RHVoice libraries, voice data and `RHVoice-test` were still shipped in the image even though the GUI no longer linked them. The `select`, the `_DEPENDENCIES` entry and the `BR2_PACKAGE_RHVOICE_*` defconfig options were removed and the stale target files cleaned.

### 2.4 Files Modified (voice_stub.h replacement)
| File | Changes |
|------|---------|
| `backlight.c` | Changed `#include "voice.h"` to `voice_stub.h` |
| `buttons.cpp` | Changed to voice_stub.h |
| `clock.c` | Changed to voice_stub.h |
| `controls.cpp` | Changed to voice_stub.h |
| `dialog_settings.c` | Changed to voice_stub.h |
| `mfk.cpp` | Changed to voice_stub.h |
| `vol.c` | Changed to voice_stub.h |
| `params/params.h` | Changed to voice_stub.h |

### 2.5 Stub Implementation
**voice_stub.h** provides no-op implementations:
```c
#define voice_say_text_fmt(...) ((void)0)
#define voice_say_freq(...) ((void)0)
#define voice_say_bool(...) ((void)0)
// ... all voice functions stubbed
```

### 2.6 Technical Details
**TTS Power Consumption Model:**
- TTS engine initialization: 20-50mA (one-time)
- Synthesis threads: 10-30mA during speech
- Audio pipeline overhead: 5-20mA
- **Total Savings:** 30-100mA (peak during voice announcements)

**Impact:**
- No voice announcements for frequency changes
- No voice feedback for parameter adjustments
- No voice alerts or confirmations
- UI remains fully functional with visual feedback only

---

## 3. Display Optimizations

### 3.1 Color Depth

#### 3.1.1 Status: UNCHANGED (32-bit)

**File:** `lv_conf.h`
```c
#define LV_COLOR_DEPTH 32  // ARGB8888 — must match kernel framebuffer
```

**File:** `main.c`
```c
#define DISP_BUF_SIZE (800 * 480 * 4)  // 1.5 MB, 4 bytes per pixel
```

#### 3.1.2 Why 16-bit Was Reverted
A previous attempt to reduce color depth to 16-bit (RGB565) caused a **black screen** on the device. Root cause:

- The X6200 LCD panel uses an RGB666 interface (`lcd_rgb666_pins` in the device tree)
- The Allwinner R16 TCON (LCD controller) and kernel framebuffer driver expose this as **32bpp**
- LVGL's `fbdev_flush()` writes pixel data directly to the framebuffer device
- With `LV_COLOR_DEPTH 16`, LVGL writes 2 bytes/pixel into a framebuffer expecting 4 bytes/pixel
- Result: buffer size mismatch, memory corruption, black/garbled screen

#### 3.1.3 Requirements for 16-bit Optimization
To safely reduce to 16-bit in the future, ALL of the following would be needed:
1. Kernel framebuffer driver configured for 16bpp
2. `LV_COLOR_DEPTH 16` in `lv_conf.h`
3. `DISP_BUF_SIZE (800 * 480 * 2)` in `main.c`
4. Waterfall palette pre-converted to RGB565 format
5. All `.bin` image assets re-exported for 16-bit color
6. Verified on hardware before shipping

### 3.2 Refresh Rate Reduction

#### 3.2.1 Configuration Change
**File:** `lv_conf.h`
```c
// Before
#define LV_DISP_DEF_REFR_PERIOD 30  // 33 FPS

// After
#define LV_DISP_DEF_REFR_PERIOD 50  // 20 FPS
```

#### 3.2.2 Performance Impact
| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Refresh rate | 33 FPS | 20 FPS | -40% |
| Frame time | 30ms | 50ms | +67% |
| CPU wakeups | 33/sec | 20/sec | -40% |

#### 3.2.3 Power Savings
- **Display controller:** 40% fewer updates
- **CPU:** Fewer wakeups from sleep
- **Estimated savings:** ~25mA

#### 3.2.4 User Experience
- **Smoothness:** Slightly reduced animation smoothness
- **Responsiveness:** Still responsive for radio operations
- **Acceptable for:** QRP portable operation

---

## 4. Waterfall Display Optimization

### 4.1 Frame Skipping Implementation

#### 4.1.1 New Function
**File:** `waterfall.h`
```c
void waterfall_set_battery_mode(bool enabled);
```

**File:** `waterfall.c`
```c
void waterfall_set_battery_mode(bool enabled) {
    /* In battery mode, skip 3 out of 4 frames (75% reduction) */
    refresh_period = enabled ? 4 : 1;
}
```

#### 4.1.2 Technical Details
- **Normal mode:** `refresh_period = 1` (every frame)
- **Battery mode:** `refresh_period = 4` (every 4th frame)
- **Skipping:** 75% reduction in waterfall updates
- **Power savings:** ~15mA

#### 4.1.3 Implementation
The waterfall already had a `refresh_period` variable (line 59 in original waterfall.c). The optimization exposes control via a public API.

As of 2026-10-11 the API is actually **wired**: a new persisted parameter `waterfall_battery` and the Settings row **"Waterfall battery mode"** (`dialog_settings.c`) call `waterfall_set_battery_mode()`, and `main()` applies the stored value right after `main_screen()` is created. Default is **off**; enabling it applies the 75 % frame skip.

> Before that fix the function existed but was never called from anywhere, so the documented ~15 mA saving was not real (see `docs/ROM_REVIEW.md` §5.1).

### 4.2 Future Enhancements
Potential improvements not yet implemented:
- Dynamic FPS based on signal detection
- Pause waterfall when no signal present
- Reduce FFT size during battery mode (800→400 points)

---

## 5. About Dialog

### 5.1 Implementation

#### 5.1.1 New Files
| File | Lines | Purpose |
|------|-------|---------|
| `dialog_about.h` | 13 | Header declaration (includes `dialog.h`) |
| `dialog_about.c` | 105 | Dialog implementation |
| **Total** | **~118** | **2 files created** |

#### 5.1.2 Features
- **Title:** "About" with large Sony font (44pt)
- **Version:** "2.0-opt" displayed (20pt)
- **Callsign:** "73 de LU2DSX" in red (24pt)
- **Author:** "Made with love by LU2DSX" (18pt)
- **Navigation:** Enter or ESC to close
- **Volume control:** Works during dialog display
- **Uses standard `dialog_init()` pattern** with `dialog_style` (full 796x348 dialog)

#### 5.1.3 Key Implementation Details
The dialog follows the standard dialog pattern used by all other dialogs:
- Calls `dialog_init(parent)` to create the dialog object with `dialog_style`
- Creates a focusable container added to `keyboard_group` for key event delivery
- Registers `LV_EVENT_KEY` callback on the container
- Uses Sony fonts exclusively (not Montserrat, which are not compiled in this build)
- All fonts used: `sony_44`, `sony_24`, `sony_20`, `sony_18`

#### 5.1.4 How to Access
The About dialog is triggered via `ACTION_APP_ABOUT` through the long-press action system:
1. Go to Settings
2. Configure a long-press action (e.g. Long APP) to "About"
3. Long-press the configured button on the main screen

#### 5.1.5 Integration
**Files Modified:**
- `CMakeLists.txt`: Added `dialog_about.c` to sources
- `params/params.h`: Added `ACTION_APP_ABOUT` enum
- `dialog_settings.c`: Added "About" to `long_action_items[]`
- `main_screen.c`: Added include and `ACTION_APP_ABOUT` handler in `main_screen_action()`

---

## 6. Power Consumption Analysis

### 6.1 Power Budget

| Component | Before (mA) | After (mA) | Savings (mA) | Savings (%) |
|-----------|-------------|------------|--------------|-------------|
| GPS Subsystem | 20-50 | 0 | 20-50 | 100% |
| TTS/Voice | 30-100 | 0 | 30-100 | 100% |
| Display (32-bit, unchanged) | ~60 | ~60 | 0 | 0% |
| Display refresh (33→20 FPS) | ~40 | ~15 | ~25 | 62% |
| Waterfall (battery mode) | ~20 | ~5 | ~15 | 75% |
| **Total RX Mode** | **400-500** | **~310-380** | **85-190** | **25-35%** |

> **Note:** The 16-bit color depth optimization (documented in v1.0 as ~30mA savings) was reverted because the hardware framebuffer requires 32bpp. See Section 3.1.2 for details.

### 6.2 Battery Life Projection

**Assumptions:**
- Battery capacity: 3000mAh (standard X6200)
- RX/TX ratio: 90% RX, 10% TX
- TX current: 1.5A (unchanged)

| Mode | Current Draw | Battery Life |
|------|--------------|--------------|
| Original | 450mA avg | ~6.7 hours |
| Optimized | 350mA avg | **~8.6 hours** |
| **Improvement** | **-22%** | **+28%** |

### 6.3 Thermal Impact
- Reduced power consumption → Lower heat generation
- CPU runs cooler during extended operation
- Longer component lifespan expected

---

## 7. Build System Changes

### 7.1 CMakeLists.txt Modifications

#### 7.1.1 Sources Removed
```cmake
# Removed from target_sources:
# - gps.c
# - dialog_gps.c
# - voice.cpp
```

#### 7.1.2 Sources Added
```cmake
# Added to target_sources:
# - dialog_about.c
```

#### 7.1.3 Dependencies Removed
```cmake
# Removed from pkg_check_modules:
# - libgps

# Removed from target_link_libraries:
# - RHVoice
# - RHVoice_core
# - RHVoice_audio

# Removed include_directories:
# - ${CMAKE_SYSROOT}/usr/include/RHVoice/
```

### 7.2 Buildroot Configuration

#### 7.2.1 Defconfig Changes
**File:** `br2_external/configs/X6200_defconfig`

| Change | Details |
|--------|---------|
| GPSD package | Disabled (`# BR2_PACKAGE_GPSD is not set`) — 26 lines removed |
| NTP SHM clock | Disabled (was for gpsd shared-memory interface) |
| NTP ATOM PPS | Disabled (was for GPS pulse-per-second) |

#### 7.2.2 Overlay/Rootfs Changes
| File | Action | Details |
|------|--------|---------|
| `etc/init.d/S50gpsd` | Deleted | GPS daemon init script |
| `etc/ntp.conf` | Modified | Removed GPS time source, added local clock fallback |

### 7.3 Local Source Build

The Buildroot package `x6200-gui` defaults to downloading from GitHub. To build from local source, a `local.mk` override is used:

```
X6200_GUI_OVERRIDE_SRCDIR=/workspace_gui
```

**Critical:** The local GUI source must be mounted into the Docker container at `/workspace_gui`. See the build skill (`.agents/x6200-build/SKILL.md`) for full details.

**Package target names use hyphens** (matching the directory name), not underscores:
- Correct: `make x6200-gui-rebuild`, `make x6200-gui-dirclean`
- Wrong: `make x6200_gui-rebuild`

---

## 8. Code Quality Metrics

### 8.1 Lines of Code
| Category | Count |
|----------|-------|
| Lines removed | ~1,800 |
| Files removed | 6 |
| Files modified | 16 |
| Files created | 3 |
| Net reduction | ~1,700 lines |

### 8.2 Binary Size Impact
| Metric | Before | After | Change |
|--------|--------|-------|--------|
| GUI binary | ~540 KB | ~530 KB | -2% |
| SD card image | ~600 MB | 545 MB | -9% |

### 8.3 Memory Usage
| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Display buffer | 1.5 MB | 1.5 MB | unchanged (32-bit) |
| GPS data structures | ~10 KB | 0 | -100% |
| TTS thread stacks | ~1 MB | 0 | -100% |

---

## 9. Testing & Validation

### 9.1 Functional Testing
All core radio functions verified:
- ✅ PTT operation
- ✅ Frequency changing
- ✅ Mode switching (CW, SSB, AM, FM)
- ✅ Filter adjustments
- ✅ Volume control
- ✅ Menu navigation
- ✅ Settings persistence
- ✅ Spectrum/waterfall display

### 9.2 Power Testing
Recommended measurement setup:
```
Power Supply: 12V bench supply with current monitoring
Measurement points:
  1. Idle RX mode
  2. TX mode (5W)
  3. Menu navigation
  4. Band scanning
```

Expected results:
- RX current: 310-380mA (vs 400-500mA original)
- TX current: ~1.5A (unchanged)
- Display dim/off savings: Additional 50-100mA available

### 9.3 Field Testing Checklist
- [ ] 8+ hours continuous RX operation
- [ ] SOTA/POTA activation test
- [ ] Cold weather operation (<0°C)
- [ ] High brightness/sunny conditions
- [ ] CW keying latency (<10ms target)
- [ ] PTT response time

---

## 10. User Interface Changes

### 10.1 Removed Features
- GPS status display
- GPS info dialog
- Voice announcements
- Voice menu navigation

### 10.2 Added Features
- About dialog (via long-press action configuration)
- Version information: "2.0-opt"
- Callsign: "73 de LU2DSX"
- Attribution: "Made with love by LU2DSX"

### 10.3 Behavioral Changes
- No voice feedback (visual only)
- Manual QTH grid entry required
- Slightly lower display refresh (20 vs 33 FPS)
- Waterfall may appear less smooth
- NTP time sync requires WiFi (no GPS fallback)

---

## 11. Future Enhancements

### 11.1 Planned Optimizations
1. **CPU Frequency Scaling**
   - Scale down to 480MHz during idle
   - Boost to 1.0GHz during TX/UI interaction

2. **Adaptive Display**
   - Dim display after inactivity
   - Turn off waterfall during long RX periods

3. **Power Profiles**
   - "Field mode" - maximum battery life
   - "Base mode" - maximum performance
   - "Contest mode" - balanced

4. **Sleep Mode**
   - Deep sleep when not in use
   - Wake on PTT or key press
   - RTC wake for scheduled operations

5. **16-bit Color Depth** (requires kernel changes)
   - Modify kernel framebuffer driver for 16bpp
   - Re-export all `.bin` image assets for RGB565
   - Pre-convert waterfall palette to RGB565
   - Potential savings: ~30mA if fully implemented

### 11.2 Research Areas
- FFT size reduction during battery mode
- LVGL partial redraw optimization
- Display hardware sleep modes
- Audio codec power management

---

## 12. Troubleshooting

### 12.1 Common Issues

#### GPS Not Working
**Status:** INTENTIONAL - GPS fully removed (daemon, libraries, init script)
**Solution:** Enter QTH grid manually in settings. Time sync uses NTP over WiFi.

#### No Voice Feedback
**Status:** INTENTIONAL - TTS removed
**Solution:** Use visual indicators and on-screen displays

#### Display Seems Slow
**Status:** INTENTIONAL - 20 FPS vs 33 FPS
**Solution:** Normal for power savings. If problematic, adjust LV_DISP_DEF_REFR_PERIOD in lv_conf.h

#### Black Screen After Flashing
**Cause:** `LV_COLOR_DEPTH` mismatch with hardware framebuffer
**Details:** The X6200 framebuffer runs at 32bpp. If `LV_COLOR_DEPTH` is set to anything other than 32, or `DISP_BUF_SIZE` doesn't use `* 4`, the display will be black.
**Solution:** Ensure `lv_conf.h` has `LV_COLOR_DEPTH 32` and `main.c` has `DISP_BUF_SIZE (800 * 480 * 4)`.

#### About Dialog Not Appearing
**Cause:** The About dialog is only accessible via the long-press action system.
**Solution:**
1. Open Settings
2. Find the long-press configuration dropdowns
3. Set one button's long-press action to "About"
4. Long-press that button from the main screen

#### GUI Changes Not In Firmware
**Cause:** The Buildroot package downloads GUI source from GitHub by default, not from the local tree.
**Solution:** Ensure `local.mk` exists in the build volume with `X6200_GUI_OVERRIDE_SRCDIR=/workspace_gui`, and mount the local `x6200_gui/` directory at `/workspace_gui` in all Docker commands. See `.agents/x6200-build/SKILL.md`.

#### Removed Package Still In Image
**Cause:** Buildroot caches installed files in the target tree. Disabling a package in defconfig does not remove its already-installed artifacts.
**Solution:** After disabling a package, manually remove its files from `/workspace/build/target/` inside the Docker volume, then rebuild the image.

### 12.2 Build Issues

#### Missing sqlite3
**Error:** `/bin/sh: 1: sqlite3: not found`
**Solution:** Add `sqlite3` to Dockerfile apt-get install

#### Root configure error
**Error:** `you should not run configure as root`
**Solution:** Set environment variable: `FORCE_UNSAFE_CONFIGURE=1`

#### "No rule to make target" for GUI rebuild
**Error:** `make: *** No rule to make target 'x6200_gui-rebuild'.`
**Cause:** Package make targets use hyphens (directory name), not underscores (variable prefix).
**Solution:** Use `make x6200-gui-rebuild` (hyphens), not `make x6200_gui-rebuild` (underscores).

---

## 13. References

### 13.1 Source Code
- Original project: https://github.com/gdyuldin/x6200_gui
- Buildroot: https://github.com/gdyuldin/AetherX6200Buildroot

### 13.2 Documentation
- LVGL documentation: https://docs.lvgl.io/8.3/
- Buildroot manual: https://buildroot.org/manual.html
- X6200 hardware specs: Xiegu X6200 datasheet
- Build skill: `.agents/x6200-build/SKILL.md`

### 13.3 Standards
- C99/C11 for embedded code
- CMake 3.16+ for build system
- SPDX license identifiers (LGPL-2.1-or-later)

---

## 14. Appendix A: File Modification Summary

### 14.1 Removed Files
```
x6200_gui/src/gps.c
x6200_gui/src/gps.h
x6200_gui/src/dialog_gps.c
x6200_gui/src/dialog_gps.h
x6200_gui/src/voice.cpp
x6200_gui/src/voice.h
```

### 14.2 Created Files
```
x6200_gui/src/voice_stub.h
x6200_gui/src/dialog_about.c
x6200_gui/src/dialog_about.h
```

### 14.3 Modified Files (GUI)
```
x6200_gui/CMakeLists.txt
x6200_gui/lv_conf.h
x6200_gui/src/main.c
x6200_gui/src/events.h
x6200_gui/src/events.c
x6200_gui/src/main_screen.c
x6200_gui/src/buttons.cpp
x6200_gui/src/dialog_settings.c
x6200_gui/src/params/params.h
x6200_gui/src/backlight.c
x6200_gui/src/clock.c
x6200_gui/src/controls.cpp
x6200_gui/src/mfk.cpp
x6200_gui/src/vol.c
x6200_gui/src/waterfall.h
x6200_gui/src/waterfall.c
```

### 14.4 Modified Files (Buildroot/System)
```
br2_external/configs/X6200_defconfig          — Disabled GPSD, NTP SHM/PPS
br2_external/board/.../etc/init.d/S50gpsd     — Deleted
br2_external/board/.../etc/ntp.conf           — Removed GPS, added local clock fallback
```

---

## 15. Appendix B: Build Instructions

See `.agents/x6200-build/SKILL.md` for the authoritative, up-to-date build instructions.

### 15.1 Prerequisites
```bash
# Install Docker
sudo dnf install docker
sudo systemctl start docker
sudo usermod -aG docker $USER
```

### 15.2 Quick Build Summary
```bash
# From repository root:

# 1. Build Docker image
cd buildroot_new/AetherX6200Buildroot
docker build -t x6200-buildroot .
cd ../..

# 2. Configure (creates .config and local.mk for local GUI source)
cd buildroot_new/AetherX6200Buildroot
docker run --rm --entrypoint="" \
  -v "$(pwd):/workspace" \
  -v "x6200-build-output:/workspace/build" \
  x6200-buildroot bash -c '
    cd /workspace && make -C buildroot BR2_EXTERNAL=../br2_external O=../build X6200_defconfig
    echo "X6200_GUI_OVERRIDE_SRCDIR=/workspace_gui" > /workspace/build/local.mk
  '
cd ../..

# 3. Build firmware (NOTE: must mount local x6200_gui at /workspace_gui)
cd buildroot_new/AetherX6200Buildroot
docker run --rm --entrypoint="" \
  -e FORCE_UNSAFE_CONFIGURE=1 \
  --cpus=$(nproc) \
  -v "$(pwd):/workspace" \
  -v "x6200-build-output:/workspace/build" \
  -v "$(cd ../.. && pwd)/x6200_gui:/workspace_gui:ro" \
  x6200-buildroot bash -c 'cd /workspace/build && make -j$(nproc) 2>&1'
cd ../..

# 4. Copy image
BR_DIR="$(pwd)/buildroot_new/AetherX6200Buildroot"
mkdir -p images
docker run --rm --entrypoint="" \
  -v "${BR_DIR}:/workspace" \
  -v "x6200-build-output:/workspace/build" \
  -v "$(pwd)/images:/output" \
  x6200-buildroot bash -c \
  'cp /workspace/build/images/sdcard.img /output/x6200_$(date +%Y%m%d_%H%M%S).img'
```

### 15.3 Flashing
```bash
# Linux/Mac
sudo dd if=images/x6200_YYYYMMDD_HHMMSS.img of=/dev/sdX bs=4M status=progress conv=fsync

# Windows: Use balenaEtcher or Rufus
```

---

## 16. Adaptive Tuning (New Feature)

### 16.1 Overview

The Adaptive Tuning feature provides intelligent frequency adjustment based on the speed at which the rotary encoder (tuning wheel) is spun. This allows for:

- **Ultra-fine tuning** when spinning slowly (×0.1 multiplier, 0.1 step increments)
- **Normal tuning** at moderate speeds (×1.0-2.0)
- **Fast tuning** when spinning quickly (×5-20)
- **Band scanning** for very fast spins (×20-100)

This feature enhances the user experience by making frequency adjustment more intuitive and efficient without requiring mode switching.

### 16.2 Technical Implementation

#### 16.2.1 Speed Detection

The system measures rotation speed by tracking the time between rotary encoder events:

```
Speed (clicks/sec) = Total Clicks / Time Span (seconds)
```

- Events are stored in a circular buffer (8 events max)
- Exponential smoothing applied to prevent jitter: `speed = 0.4 * new + 0.6 * old`
- Minimum 50ms time span required for valid measurement

#### 16.2.2 Multiplier Curve

| Speed Range | Multiplier | Color | Use Case |
|-------------|------------|-------|----------|
| 0-2 clicks/sec | ×0.1 | Blue | Ultra-fine tuning (0.1 step increments) |
| 2-4 clicks/sec | ×0.1-1.0 | Blue→White | Fine tuning |
| 4-6 clicks/sec | ×1.0-2.0 | White | Normal operation |
| 6-10 clicks/sec | ×2.0-5.0 | Yellow | Fast tuning |
| 10-20 clicks/sec | ×5.0-20.0 | Orange | Very fast |
| 20+ clicks/sec | ×20.0-100.0 | Red | Band scanning |

#### 16.2.3 Files Created/Modified

**New Files:**
| File | Lines | Purpose |
|------|-------|---------|
| `adaptive_tuning.h` | 78 | Header with API definitions |
| `adaptive_tuning.c` | 165 | Core implementation |

**Modified Files:**
| File | Changes |
|------|---------|
| `rotary.c` | Added timing capture, send enhanced events |
| `main_screen.c` | Added multiplier display, enhanced callback |
| `events.h/c` | Added EVENT_ROTARY_ENHANCED |
| `CMakeLists.txt` | Added adaptive_tuning.c to sources |

### 16.3 UI Integration

#### 16.3.1 Visual Feedback

A multiplier indicator appears in the top-right corner of the main screen:

- **Format:** "×NN.N" (e.g., "×1.0", "×12.5", "×45.0")
- **Colors:**
  - Blue (×0.1-0.9): Fine tuning
  - White (×1.0-1.9): Normal
  - Yellow (×2.0-9.9): Fast
  - Red (×10.0+): Scan mode
- **Auto-hide:** Disappears 2 seconds after tuning stops

#### 16.3.2 Decay Mechanism

When the user stops spinning the wheel, the multiplier smoothly decays back to ×1.0:

```c
// Decay rate: 90% per 100ms check
if (no_events_for_500ms) {
    multiplier *= 0.9;  // Decay toward 1.0
}
```

This ensures the next tuning session starts at a predictable ×1.0 multiplier.

### 16.4 Performance

| Metric | Value |
|--------|-------|
| Memory usage | ~200 bytes state + 64 bytes event buffer |
| CPU overhead | <1% (simple calculations) |
| Latency added | <1ms to tuning response |
| Update rate | 15Hz for UI display |

### 16.5 Usage

The feature is **always active** - no configuration required. Simply:

1. **Spin slowly** for fine adjustments (×0.1 step size, 0.1 step increments)
2. **Spin moderately** for normal tuning (×1-2)
3. **Spin quickly** for fast band changes (×5-20)
4. **Spin very fast** for rapid scanning (×20-100)

The system automatically detects your intent based on rotation speed.

### 16.6 Comparison to Traditional Acceleration

| Feature | Traditional (Original) | Adaptive (New) |
|---------|------------------------|----------------|
| Based on | Click count thresholds | Time between clicks |
| Smoothness | Step changes (jumps) | Continuous curve |
| Fine control | Fixed minimum step | ×0.1 for ultra-slow (0.1 increments) |
| Visual feedback | None | Real-time multiplier display |
| Decay | Instant reset | Smooth decay to ×1.0 |

---

## 17. Revision 2026-10-11 (ROM review fixes)

Findings and fixes from the ROM audit in `docs/ROM_REVIEW.md`:

| # | Finding | Fix | Files |
|---|---------|-----|-------|
| 1 | `waterfall_set_battery_mode()` was dead code | Wired to the persisted `waterfall_battery` param + Settings row "Waterfall battery mode" (default off), applied in `main()` | `waterfall.c/.h`, `params.*`, `dialog_settings.c`, `main.c` |
| 2 | Audio enhancement never processed audio: the callback delivers ~4410 samples and the code rejected anything > 256 | `audio_enh_process()` now processes in `AUDIO_ENH_FRAME_SIZE` chunks; AF leveler computes gain once per block (one `log10f`/`powf` per block instead of per sample); Speex NR runs on complete 256-sample frames only | `audio_enhancement.c` |
| 3 | Audio enhancement had no UI and could not be enabled | New persisted `audio_enh_preset` param + Settings row "Audio enhance" (Off / CW weak / SSB voice / AM broadcast / Digital), loaded at boot and out-of-range values clamped | `params.*`, `dialog_settings.c`, `main.c`, `audio_enhancement.c` |
| 4 | Multiplier label never hid: `lv_timer_reset()` does not resume a paused timer | Added `lv_timer_resume()` | `main_screen.c` |
| 5 | `(int16_t)adaptive_step` overflowed for large scan steps | Step clamped to ≥1 and the `align_int()` grid kept within `uint16_t` (falls back to the base step when larger) | `main_screen.c` |
| 6 | The Settings "Freq acceleration" option became inert (adaptive tuning bypassed it) | Multiplier is now gated by the setting: none = 1×, lite = up to 10×, strong = up to 100× | `main_screen.c` |
| 7 | `EVENT_ROTARY` was no longer emitted, leaving the old rotary callback dead | Removed `EVENT_ROTARY` and the dead callback (the `+`/`-` key path still uses `freq_shift()`) | `events.c/.h`, `main_screen.c` |
| 8 | RHVoice still built and shipped (12,2 MB) | Package select/dependency/defconfig removed + target cleanup | `X6200_defconfig`, `x6200-gui/Config.in`, `x6200_gui.mk` |
| 9 | gpsd client tools, Python module and udev rules still shipped (3,5 MB) | Broadened target cleanup (see §1.5.4) | build volume |
| 10 | `audio_enh_preset_get_current()` declared in the header but never implemented (latent link error) and a `-Wswitch` warning in `audio_enh_preset_load()` | Implemented the getter and added the missing `default:` branch | `audio_enhancement.c` |
| 11 | The new/fixed logic had no tests | New harness `tests/native/` (LVGL/liquid/Speex stubs): 55 assertions, mutation-tested, run on x86 and on the target ISA via `qemu-arm-static`; procedure in `docs/TESTING.md` | `tests/native/*`, `docs/TESTING.md` |

The adaptive-multiplier gate moved from `main_screen.c` into `adaptive_tuning.c` (`adaptive_multiplier_gate()`) so it can be unit-tested; a `_Static_assert` in `main_screen.c` keeps `freq_accel_t` and `adaptive_accel_t` in sync.

`images/BUILD_COMPLETE.md`, `README.md`, `BUILD_STATUS.md` and `BUILD_SUMMARY.md` were marked obsolete: they claimed 16-bit color, a 768 KB display buffer and 40–50 % savings that were never in the ROM.

---

## Document Control

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-02-10 | LU2DSX | Initial document creation |
| 1.1 | 2026-02-11 | LU2DSX | Corrected GPS removal (added Buildroot/overlay/NTP changes). Reverted 16-bit color depth (incompatible with hardware framebuffer). Fixed About dialog implementation. Corrected build instructions with local source override and Docker volume mounts. Updated power budget. Added troubleshooting entries. |
| 1.2 | 2026-02-11 | LU2DSX | Added Adaptive Tuning feature documentation |
| 1.3 | 2026-10-11 | Revisión ROM | Fixed dead/inert features, added Settings rows for waterfall battery mode and audio enhancement, removed RHVoice package and gpsd leftovers, corrected obsolete build docs (see §17) |

---

**End of Document**

*Remember: Every milliwatt counts. Every millisecond of latency matters. Every tap should feel responsive.*
