# Compat FW 1.0.8 — lado GUI: ABI de muestras del flujo RX (issue #377, parte B)

Fecha: 2026-10-30 (America/Buenos_Aires). **Epic:** [#374](https://github.com/iariap/x6200_gui/issues/374)
· **Fase A:** [#375](https://github.com/iariap/x6200_gui/issues/375) / [PR #379](https://github.com/iariap/x6200_gui/pull/379)
· **Fase B (RE):** [#376](https://github.com/iariap/x6200_gui/issues/376) / [PR #380](https://github.com/iariap/x6200_gui/pull/380)
· **Issue:** [#377](https://github.com/iariap/x6200_gui/issues/377) FAST-TRACK.
**PR de esta parte:** [#381](https://github.com/iariap/x6200_gui/pull/381) (sin merge).

Rama `feat/x6200-fw108-gui-samples`, worktree `/home/pablo/desa/x6200/x6200-fw108-compat-qa`,
base GUI `main` = `6850d671d430a1693dbcaf593579a3d98b037995`.

## Qué corrige

`x6200_flow_t` de **X6200Control v0.1.1** declara `uint8_t samples[512]`
(`sizeof(x6200_flow_t) == 552`), mientras que v0.1.0 declaraba `float samples[512]`
(`sizeof == 2088`). La GUI de este repo hacía, en `src/radio.c`:

```c
float *samples = (float*)((char *)pack + offsetof(x6200_flow_t, samples));
dsp_samples(samples, RADIO_SAMPLES /* 512 */, ...);
```

Con v0.1.1 eso lee **2048 B desde un payload de 552 B** y le pasa al DSP bytes de
muestra tratados como `float` (sobrelectura + contenido inválido).

El fix (diff acotado a la ruta de muestras):

- `src/radio_samples.h` (nuevo, C puro sin SDK/LVGL): conversión por muestra
  `(float)((int)byte - 150)` — offset `0x96` de la APP vendor, sin otro escalado.
- `src/radio.c`: buffer local `float samples[RADIO_SAMPLES]` con vida hasta el
  retorno de `dsp_samples()`; misma llamada y mismo uso del DSP (no se toca
  `src/dsp.cpp`, ni TX, ni S-meter, ni waterfall, ni ajustes, ni modos).
- `_Static_assert` del ABI v0.1.1 (`sizeof(pack->samples)==512`,
  `offsetof(x6200_flow_t,samples)==4`, `sizeof(x6200_flow_t)==552`,
  `RADIO_SAMPLES==RADIO_FLOW_SAMPLES`): si el header con el que se compila no es
  ese, el build se detiene con **`BLOCKED_GUI_SAMPLE_ABI`** (no se reinterpretan
  floats ni se truncan paquetes).
- `tests/native/test_radio_samples.c` + stage nuevo en `run_native_tests.sh`:
  conversión determinista, 512 muestras, sin acceso fuera de rango, ASan/UBSan
  cuando hay runtime.

Detalle completo en [`abi-audit.md`](abi-audit.md) y matriz de casos en
[`contract-test-matrix.csv`](contract-test-matrix.csv).

## Estado del gate (lado GUI)

| Campo | Valor |
|---|---|
| `sample_abi` | **`FIXED_TO_V0_1_1_GUI`** |
| `abi_static_asserts` | **PASS** (`offsetof 4`, `samples 512 u8`, `sizeof 552`) |
| `host_tests` | **PASS** — `tests/native/run_native_tests.sh` exit 0; stage #377 533 checks, 0 fallos |
| `sanitizers` | **PASS manual (clang-18 ASan+UBSan)**; runner con gcc: runtime no disponible → fallback sin sanitizador (registrado) |
| `cross_compile_check_arm_v0_1_1` | **PASS** — `src/radio.c` compila con el toolchain del build tree |
| `negative_control_v0_1_0` | **EXPECTED FAIL** — `BLOCKED_GUI_SAMPLE_ABI` (el guard funciona) |
| `full_gui_lib_image_build` | **NOT_RUN** — fuera del alcance B; lo ejecuta la integración (task-16) con el build tree del owner |
| `physical_rx_test` | **NOT_RUN** — requiere instalación/autorización del dueño (#378) |
| `payload_authenticity` | **`UNVERIFIED`** |
| `release_note_hash_mismatch` | **registrado** (declarado `29620674…8698` ≠ calculado `a968d278…bc3f`) |
| `production_ready` | **`false`** |
| `gate final (build/imagen/GO)` | pendiente de la integración (task-16) |

`RELEASE_NOTE_HASH_MISMATCH`: el `sdcard.img` del paquete 1.0.8 no coincide con el
SHA256 que declara su propio release note ([PR #379](https://github.com/iariap/x6200_gui/pull/379),
`INTEGRITY_MISMATCH_VS_VENDOR_PUBLISHED_HASH`). Este PR **no** acredita integridad
criptográfica del paquete ni que BASE 1.0.8 esté validada en hardware. La
autorización FAST-TRACK permite construir y probar con esa incertidumbre
declarada; nada de eso convierte el payload en autenticado.

## Contenido de este directorio

| Archivo | Contenido |
|---|---|
| `README.md` | este resumen y estado del gate |
| `abi-audit.md` | antes/después, offsets, tamaños, static asserts y verificaciones |
| `contract-test-matrix.csv` | casos de contrato/conversión/ABI con resultado real |
| `commands.log` | comandos ejecutados y exit codes (sin binarios ni datos sensibles) |
| `gate.json` | gate del lado GUI en formato máquina |

La integración (parte C / task-16) añade en este mismo directorio el build real
de librería+GUI, la imagen de prueba con SHA256 y el gate final. Los estados de
este PR son deliberadamente parciales: `HOST_PASS`, no `READY_FOR_SHORT_RX_TRIAL`.

## Reproducir el chequeo host (sin SDK ni radio)

```sh
cd /home/pablo/desa/x6200/x6200-fw108-compat-qa
tests/native/run_native_tests.sh            # suite completa; el stage #377 imprime sus 533 checks
git diff --check 6850d671 HEAD              # exit 0
```

Compile-check cross (sólo lectura sobre el build tree del owner; sale a
`fw108-scratch/gui377/cross-check/`): ver `commands.log` §7.

## Enlaces

- **Parte A (librería RX SLIP, repo Buildroot):**
  [AetherX6200Buildroot PR #14](https://github.com/iariap/AetherX6200Buildroot/pull/14)
  (`feat/x6200-fw108-slip-compat`, HEAD `afb7a3cd8b7edb5ee2ba3b58596dbd8c762b8eee`,
  base `8b7ca9d6`). Patch `0001-flow-support-SLIP-RX-framing-for-BASE-1.0.8.patch`
  sha256 `46b4e7f36e25e3db6b53de057d037a11a72ee198fe5500f2ce126996e193838f`
  (verificado de forma read-only desde el branch del PR; aplica a X6200Control
  v0.1.1 tag `0ee1ae3`). El build tree del owner compila hoy
  `aether-x6200-control` v0.1.0 (staging `float samples[512]`) y debe pasar a
  v0.1.1+patch para integrar esta GUI.
- RE del payload 1.0.8: [PR #380](https://github.com/iariap/x6200_gui/pull/380)
  (`wire_contract_for_received_payload = PARTIAL`, payload `UNVERIFIED`).
- Procedencia/hashes: [PR #379](https://github.com/iariap/x6200_gui/pull/379).

## Límites y UNKNOWNs

- `payload_authenticity = UNVERIFIED`; `RELEASE_NOTE_HASH_MISMATCH` sin resolver.
- Bytes reales del wire de BASE 1.0.8: `UNKNOWN` (no hay captura y está prohibido
  capturarlos). El offset 150 de las muestras es el de la APP vendor, no un valor
  medido en esta GUI.
- El build tree del owner hoy usa v0.1.0; con ese header el guard falla **a
  propósito** (`BLOCKED_GUI_SAMPLE_ABI`). La integración debe usar v0.1.1+patch.
- Sin radio, sin I²C/UART/MUX, sin TX/PTT/ATU, sin flash, sin imagen. No hay
  validación física ni de audio RX.
