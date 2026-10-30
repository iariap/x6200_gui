# Compat FW 1.0.8 — lado GUI e integración: ABI de muestras del flujo RX (issue #377, partes B y C)

Fecha: 2026-10-30 (America/Buenos_Aires). **Epic:** [#374](https://github.com/iariap/x6200_gui/issues/374)
· **Fase A:** [#375](https://github.com/iariap/x6200_gui/issues/375) / [PR #379](https://github.com/iariap/x6200_gui/pull/379)
· **Fase B (RE):** [#376](https://github.com/iariap/x6200_gui/issues/376) / [PR #380](https://github.com/iariap/x6200_gui/pull/380)
· **Issue:** [#377](https://github.com/iariap/x6200_gui/issues/377) FAST-TRACK.
**PR de esta parte:** [#381](https://github.com/iariap/x6200_gui/pull/381) (sin merge).

Rama `feat/x6200-fw108-gui-samples`, worktree `/home/pablo/desa/x6200/x6200-fw108-compat-qa`,
base GUI `main` = `6850d671d430a1693dbcaf593579a3d98b037995`.
**Parte C (integración, task-16): build real de librería v0.1.1+patch A + GUI,
imagen de prueba con SHA256 fuera de Git y restore del build tree del owner**;
ver [Parte C — integración](#parte-c--integración-build-real-imagen-de-prueba-restore).

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
| `full_gui_lib_image_build` | **PASS** — build real lib v0.1.1+patch A + GUI PR B en el build tree del owner, exit 0 (parte C) |
| `image_test_build` | **PASS** — `sdcard.img` sha256 `7ec5d8cc…b5d9` (571 473 920 B), fuera de Git, no flasheada |
| `build_tree_restore` | **PASS** — `git status --porcelain` idéntico antes/después del build |
| `tests_in_integration_env` | **PASS** — parser C 198/0 (release+ASan/UBSan) y suite nativa GUI exit 0 (stage #377 533/0) |
| `physical_rx_test` | **NOT_RUN** — requiere instalación/autorización del dueño (#378) |
| `payload_authenticity` | **`UNVERIFIED`** |
| `release_note_hash_mismatch` | **registrado** (declarado `29620674…8698` ≠ calculado `a968d278…bc3f`) |
| `production_ready` | **`false`** |
| `final_state` | **`HOST_BUILD_PASS / READY_FOR_SHORT_RX_TRIAL`** (prueba física `NOT_RUN`, #378) |

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

## Parte C — integración: build real, imagen de prueba, restore

Ejecutada el 2026-10-30 (task-16) sobre el build tree del owner
`/home/pablo/desa/x6200/rom-build/clone-buildroot/build` con el contenedor
`x6200-buildroot:latest` (Buildroot 2023.02.11-dirty, gcc 13.3 host / ARM gcc
12.3.0), con backup/restore exacto. Comando:

```sh
make aether-x6200-control-dirclean && make x6200-gui-dirclean && make -j8   # exit 0
```

Log completo: `fw108-scratch/integration-377/build-integration.log`
(sha256 `f7ef9550…e5764`). Duración 12:20:40→12:21:31 (America/Buenos_Aires).

### Librería v0.1.1 + patch A (parte A) dentro del build

| Check | Resultado |
|---|---|
| Buildroot aplica el patch | **PASS** — log `Applying 0001-flow-support-SLIP-RX-framing-for-BASE-1.0.8.patch using patch:` + `patching file src/low/flow.c` |
| `src/low/flow.c` compilado | sha256 `77c4fd931b9f5cd0796d12ce3fc85b29efdf43524dd566a4215b02792cabaf2a` |
| header v0.1.1 en staging | sha256 `272028947ba84f327d56f03c1af8a1abdd9f19dc9137e81cb7a673634d4b9094` (`uint8_t samples[512]`, `sizeof(x6200_flow_t)=552`) |
| `.so` del build dir (sin strip) | sha256 `f2eb4fb1b291c6dd76a0a46a5e326eefd9ad87fe46abe6fe0c1a373fc4dd1138` — **idéntico** al gate ARM de la parte A (PR #14) |
| `.so` en la imagen (stripped) | sha256 `e93d79a376a974bea61301ee5c90eb0f951d79c00f06918ccdc7827e59d6cad5` |
| ABI vs v0.1.0 previo | **PASS** — 100/100 símbolos dinámicos idénticos; `x6200_flow_init/read/restart` exportados; SONAME `libaether_x6200_control.so.0` igual |

### GUI PR B dentro del build

| Check | Resultado |
|---|---|
| Sync desde el worktree del PR B | **PASS** — log `>>> x6200-gui v0.1.0-fw108 Syncing from source dir /home/pablo/desa/x6200/x6200-fw108-compat-qa` |
| `src/radio.c` / `src/radio_samples.h` compilados | sha256 `6be56a90…a9362f` / `d2ff0bf3…b4c8144` (idénticos al worktree del PR #381) |
| `_Static_assert` de ABI v0.1.1 | **PASS** — el build sólo compila con header 552 B / `offsetof 4` / `samples[512] u8`; con v0.1.0 falla `BLOCKED_GUI_SAMPLE_ABI` |
| `usr/sbin/x6200_gui` en la imagen | sha256 `6a068db278c72986967bbbc0d4a12d9f71202507054fbbd3c986746eb40b7c1d`; sin strip `64d18becdbf2cee9a477ba30cc704bfd3753b1fb0b6d4e9329948888ec68ffdc` |

### Imagen de prueba (fuera de Git, no flasheada)

Directorio: `/home/pablo/desa/x6200/fw108-scratch/fw108-image-377/`.

| Archivo | Bytes | SHA256 |
|---|---|---|
| `sdcard.img` | 571 473 920 | `7ec5d8cc4cb6cbf98fc92af9cf6489ad1a94f5dce8585d43c93156afde68b5d9` |
| `rootfs.tar` | 244 336 640 | `a2eb1d6582e766a38df0ea7db9b49e958063713d6dd10d7cb96d0621215d171e` |
| `boot.vfat` | 33 554 432 | `1960b896e510dfe8d619a14c207ad86209a2e31bbe310ed9a26c4ba136d5b99e` |

La partición 2 de `sdcard.img` (ext4, LBA 67584) tiene SHA256
`bbd9d172c44746cd4dfdf28ec4e0f2657770c0d23f3ddfe07816fb65ec9d9bc5`, idéntico a
`rootfs.ext2` → la imagen contiene este rootfs con la lib v0.1.1+patch
(`e93d79a3…`) y la GUI PR B (`6a068db2…`). Provenance completa en
`fw108-image-377/PROVENANCE.md` y `manifest.json` (ambos fuera de Git).

### Tests en el entorno de integración

| Check | Entorno | Resultado |
|---|---|---|
| Host parser C real (parte A) | contenedor build (gcc 13.3), release + ASan/UBSan | `198 checks, 0 failures` ×2 → `HOST_TESTS_PASS` exit **0** |
| Suite nativa GUI `tests/native/run_native_tests.sh` | host integración (gcc 16.2.1) | exit **0**; 26 stages, todos 0 fallos; stage #377 `533 comprobaciones, 0 fallos` |
| Suite nativa GUI dentro del contenedor | contenedor build | `NOT_RUN` por entorno (la imagen Docker no trae `sqlite3.h`/`png.h` de desarrollo); se re-corrió en el host con exit 0 |

### Restore del build tree (sin destruir al owner)

- `git status --porcelain` **idéntico antes/después**: sha256
  `a301221fcd4cb913e9744a2cdb27f7d3d41a5dd3253e6ab4555e4735599585c0` en ambos
  (11 líneas del owner intactas, incluidas sus 2 tracked modificadas y los 8
  untracked previos).
- Paquete `aether-x6200-control` restaurado al v0.1.0 de HEAD (sin patch/tests);
  `x6200_gui.mk` restaurado desde backup (sha256 `7b17f036…e554`).
- Build dirs nuevos (`…-control-v0.1.1`, `…-gui-v0.1.0-fw108`) eliminados; los
  dirs v0.1.0 del owner quedaron intactos (se usaron versiones distinguibles
  a propósito).
- `images/` restaurada byte a byte (sdcard.img `0ed4cd32…37cf7`, mtime original).
- Artefactos de `target/` regenerados (GUI/lib) restaurados con hashes idénticos
  a la imagen previa; auditoría de los 4214 archivos del rootfs anterior: sólo 3
  diferencias en `/etc` (`group` pre-existente; `passwd`/`shadow` re-generados por
  `target-finalize` con la misma password configurada `123` y salt nuevo).
- Residuales no versionados documentados: cache `dl` gana el tarball v0.1.1
  (35 972 B, reutilizable), entradas nuevas de ccache, y `staging` de GUI queda
  con los artefactos del PR B (se regenera en el próximo
  `make x6200-gui-dirclean && make` del owner).
- Detalle completo: `fw108-scratch/integration-377/restore-report.md`.

**Gate final: `HOST_BUILD_PASS / READY_FOR_SHORT_RX_TRIAL`** — imagen apta **sólo**
para la prueba corta RX supervisada de #378. `physical_rx_test = NOT_RUN` (la
ejecuta el operador), `production_ready = false`, `payload_authenticity = UNVERIFIED`.

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
  v0.1.1 tag `0ee1ae3`). La integración (parte C) aplicó ese paquete v0.1.1+patch
  en el build tree del owner, confirmó `Applying 0001-…patch` y lo restauró al
  estado v0.1.0 al terminar (ver Parte C).
- RE del payload 1.0.8: [PR #380](https://github.com/iariap/x6200_gui/pull/380)
  (`wire_contract_for_received_payload = PARTIAL`, payload `UNVERIFIED`).
- Procedencia/hashes: [PR #379](https://github.com/iariap/x6200_gui/pull/379).

## Límites y UNKNOWNs

- `payload_authenticity = UNVERIFIED`; `RELEASE_NOTE_HASH_MISMATCH` sin resolver.
- Bytes reales del wire de BASE 1.0.8: `UNKNOWN` (no hay captura y está prohibido
  capturarlos). El offset 150 de las muestras es el de la APP vendor, no un valor
  medido en esta GUI.
- El build tree del owner quedó restaurado a v0.1.0 (su estado previo); con ese
  header el guard falla **a propósito** (`BLOCKED_GUI_SAMPLE_ABI`). La integración
  compiló y verificó con v0.1.1+patch A (parte C).
- Sin radio, sin I²C/UART/MUX, sin TX/PTT/ATU, sin flash. La imagen de prueba
  existe fuera de Git y **no** fue flasheada/instalada: `physical_rx_test = NOT_RUN`
  (la ejecuta el operador en #378).

## Trazabilidad del patch A (corrección documental posterior a la integración)

El coordinador corrigió el PR A (AetherX6200Buildroot #14): el encabezado del patch afirmaba erróneamente que el BASE oficial 1.0.6 enviaba tramas RAW de 552 B. Corrección **sólo de comentarios/README**:

- RAW 552 B = contrato de **X6200Control v0.1.1** (`uint8_t samples[512]`), **no** el wire del BASE oficial 1.0.6 (APP 1.0.6 documentada: RAW 2088 B, `float samples[512]`, ABI v0.1.0).
- SLIP 552 B = contrato reconstruido para **BASE 1.0.8** (RE #376/PR #380, payload `UNVERIFIED`).
- Retrocompatibilidad física con BASE 1.0.6: **NOT_DEMONSTRATED / NOT_SUPPORTED / NOT_TESTED** (requeriría ABI 2088 B; fuera de scope FAST-TRACK).
- Los tests "RAW legacy" usan fixtures sintéticos de 552 B v0.1.1, no capturas de 1.0.6.

Prueba de que el código ejecutable no cambió: 37 líneas modificadas, todas comentarios; fuente preprocesada idéntica (`29f2c1ff…`); objeto ARM idéntico (`a25bbd71…`); `objdump -d` diff vacío; librería LTO sin strip idéntica `f2eb4fb1…` — **la misma que se construyó en la imagen**. Por eso la imagen `sdcard.img` sha256 `7ec5d8cc…b5d9` (construida con el patch viejo `46b4e7f3…`) **sigue válida sin reconstruir**. Patch nuevo: `bc64b21d…`. El Lead reprodujo el build del patch nuevo y obtuvo la misma librería `f2eb4fb1…`.
