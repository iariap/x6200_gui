# ABI audit — muestras de `x6200_flow_t` en la GUI (issue #377, parte B)

Alcance: el lado **GUI** de `iariap/x6200_gui`. La librería `aether-x6200-control`
(decoder RX) es la parte A del issue y va en un repo/PR distinto. Todo lo de abajo
es estático/host: sin radio, sin I2C/UART/MUX, sin TX/PTT/ATU, sin flash.

Base: `main` `6850d671d430a1693dbcaf593579a3d98b037995`. Fix: commit `4ff076ba`.

## 1. Contrato del header X6200Control v0.1.1 (esperado)

Tag `v0.1.1` = commit `0ee1ae3d4d9d74cbac7e349147ebc440eed8fad8`
(`Fix flow on 1.0.7 base`, 2026-05-08) del repo `gdyuldin/X6200Control`, checkout
local `/home/pablo/desa/x6200/refs/X6200Control` (verificado con
`git describe --tags --exact-match` → `v0.1.1`).

`include/aether_radio/x6200_control/low/flow.h`
sha256 `272028947ba84f327d56f03c1af8a1abdd9f19dc9137e81cb7a673634d4b9094`:

| offset | campo | tipo | bytes |
|---|---|---|---|
| 0x000 | `magic` | `uint32_t` | 4 |
| **0x004** | **`samples`** | **`uint8_t[512]`** | **512** |
| 0x204 (516) | `flag` | `x6200_flow_flags_t` (bitfield) | 4 |
| 0x208 (520) | `dbm`, `tx_power`, `vswr`, `alc_level` | `uint8_t` ×4 | 4 |
| 0x20C (524) | `vext`, `vbat`, `batcap`, `reserved_2` | `uint8_t` ×4 | 4 |
| 0x210 (528) | `atu_params` | `uint32_t` | 4 |
| 0x214 (532) | `reserved_3` | `uint32_t[3]` | 12 |
| 0x220 (544) | `hkey` | `uint32_t` | 4 |
| 0x224 (548) | `crc` | `uint32_t` | 4 |
| **0x228 (552)** | **`sizeof(x6200_flow_t)`** | | **552** |

El CRC legacy corre sobre `payload[0..548)` y se compara contra `payload[548..552)`
(RE estática de #376/PR #380; la APP 1.0.8 acepta 552 B decodificados y CRC sobre
548 B). El layout del payload decodificado de 1.0.8 coincide **offset por offset**
con este header (PR #380, sección A: `samples u8[512] 0x004`, `flags 0x204`,
`crc 0x224`, `sizeof 552`).

## 2. Contrato anterior v0.1.0 (lo que compila hoy el build tree del owner)

`float samples[512]` en el mismo offset. Medido con un dump host contra el header
del staging real (`/home/pablo/desa/x6200/rom-build/clone-buildroot/build/staging/usr/include/...`,
sha256 `29cfa6b449beb20ee2dcda33aefe5a0f51805b5fa2b9ab844acee8df8333c580`):

| métrica | v0.1.1 | v0.1.0 |
|---|---|---|
| `sizeof(x6200_flow_t)` | **552** | **2088** |
| `offsetof(x6200_flow_t, samples)` | 4 | 4 |
| `sizeof(((x6200_flow_t *)0)->samples)` | **512** (u8) | **2048** (float) |
| `offsetof(flag)` / `offsetof(crc)` | 516 / 548 | 2052 / 2084 |

El paquete del Buildroot **main** declara `AETHER_X6200_CONTROL_VERSION = v0.1.1`
(verificado con `gh api ...?ref=main`); el clone del owner está en la rama
`feat/kernel-hidraw-unifying` con `v0.1.0` y su staging tiene ese header. La
integración debe reconstruir la librería con v0.1.1+patch antes de la GUI.

## 3. Antes del fix (código base `6850d671`)

`src/radio.c` (sha256 `2d9206eb15a281cb4ded49aa9d3029966439fd89f80ae9e8d669720ed4f2631e`):

```c
float *samples = (float*)((char *)pack + offsetof(x6200_flow_t, samples));
...
dsp_samples(samples, RADIO_SAMPLES, pack->flag.tx, -(int16_t)pack->dbm);
```

`dsp_samples()` (`src/dsp.cpp:232`) ignora el `size` y alimenta
`AveragedPSD<RADIO_SAMPLES, …>::add_samples(buf_samples)`, que hace
`liquid_vectorf_add(psd.data(), samples, psd.size(), …)` con
`psd.size() == RADIO_SAMPLES == 512` (`src/dsp.cpp:43-84`, `src/radio.h:16`).

Con v0.1.1: se pasan **512 `uint8_t` leídos como 512 float = 2048 B** desde una
asignación de 552 B (`malloc(sizeof(x6200_flow_t))` en `radio_init`), o sea una
**sobrelectura de 1496 B** además de contenido inválido. Reproducción ASan del
patrón pre-fix (`fw108-scratch/gui377/asan-old-cast.c`): `heap-buffer-overflow`
(ver `commands.log` §6). En v0.1.0 el cast era correcto (float[512]).

## 4. Después del fix (commit `4ff076ba`)

> **Corrección posterior (#378):** la conversión con `-150` de este apartado era
> la hipótesis inicial; la prueba física mostró que el flujo 1.0.7/1.0.8 usa
> `-126` y que `dsp.cpp` debe convertir dB→potencia. El código vigente del PR
> aplica esa corrección; ver [physical-fix-378.md](physical-fix-378.md).

`src/radio_samples.h` (nuevo; sha256
`d2ff0bf3ad195cdac6195cb462181e1854d69d7ad734903f59e9eb571b4c8144`):

```c
#define RADIO_FLOW_SAMPLES      (512)
#define RADIO_FLOW_SAMPLE_BIAS  (126)   /* flujo 1.0.7/1.0.8: dB referidos a 126 */

static inline float radio_flow_sample_to_float(uint8_t raw) {
    return (float) ((int) raw - RADIO_FLOW_SAMPLE_BIAS);
}

static inline void radio_flow_samples_to_float(float *out, const uint8_t *raw, uint16_t count);
```

`src/radio.c` (sha256 `6be56a90054dc42c83d9fba40119adc6250d065f318a00acc54f7e69d8a9362f`):

```c
float samples[RADIO_SAMPLES];                                    /* vida hasta el retorno */
radio_flow_samples_to_float(samples, pack->samples, RADIO_FLOW_SAMPLES);
x6200_mode_t mode = subject_get_int(cfg_cur.mode);
dsp_samples(samples, RADIO_SAMPLES, pack->flag.tx, -(int16_t)pack->dbm);
```

- No cambia la firma ni el uso de `dsp_samples()`; `src/dsp.cpp` queda **idéntico**
  (sha256 `7b5563b32afeb03907d5e2a0322683fdc1f652efcb1640b70f4b41d73437a954` en base
  y en el fix).
- No se toca TX, S-meter, waterfall, ajustes, modos ni lógica de radio.

## 5. Static asserts: gate `BLOCKED_GUI_SAMPLE_ABI`

En `src/radio.c`, junto a la declaración de `pack`:

| assert | esperado |
|---|---|
| `_Static_assert(sizeof(pack->samples) == RADIO_FLOW_SAMPLES, …)` | 512 |
| `_Static_assert(offsetof(x6200_flow_t, samples) == 4, …)` | 4 |
| `_Static_assert(sizeof(x6200_flow_t) == 552, …)` | 552 |
| `_Static_assert(RADIO_SAMPLES == RADIO_FLOW_SAMPLES, …)` | 512 == 512 |

Cualquier falla emite un mensaje que empieza con `BLOCKED_GUI_SAMPLE_ABI` y el
build se detiene: no hay camino que reinterprete floats ni que trunque paquetes.

## 6. Verificaciones reales (host y cross)

| # | Verificación | Resultado | Evidencia |
|---|---|---|---|
| V1 | Dump ABI host contra header v0.1.1 | **PASS** — 552 / 4 / 512 | `commands.log` §5, `abi-dump-v011` |
| V2 | Dump ABI host contra header v0.1.0 del staging | **PASS (informativo)** — 2088 / 4 / 2048 | `commands.log` §5, `abi-dump-v010` |
| V3 | Cross compile `src/radio.c` con toolchain Buildroot + headers v0.1.1 | **PASS exit 0** (asserts incluidos) | `commands.log` §7 |
| V4 | Control negativo: mismo compile contra header v0.1.0 del staging | **FAIL esperado** — `static assertion failed: "BLOCKED_GUI_SAMPLE_ABI…"` | `commands.log` §7 |
| V5 | Test host conversión/512/ASan-UBSan | **PASS** — exit 0, 0 fallos | `commands.log` §3-4 |
| V6 | Control negativo del ASan (1 muestra de más) | **FAIL esperado** — `heap-buffer-overflow` | `commands.log` §6 |
| V7 | Reproducción ASan del cast pre-fix | **FAIL esperado** — `heap-buffer-overflow` | `commands.log` §6 |
| V8 | Suite nativa completa | **PASS exit 0**, stage #377 533 checks | `commands.log` §2 |
| V9 | `bash -n` de los scripts tocados | **PASS** | `commands.log` §8 |
| V10 | `git diff --check` | **PASS exit 0** | `commands.log` §9 |
| V11 | ASF `check-matrix.sh` | **exit 1 por gates de ENTORNO** (uv cache read-only, `lvgl/` ausente, banco QEMU sin `tools/remote/x6200-remote`); todas las gates que cubren el diff: **ok** | `commands.log` §10 |
| V12 | Build completo GUI+lib+imagen | **NOT_RUN** — alcance B; lo hace la integración (task-16) | — |
| V13 | Prueba física RX | **NOT_RUN** — requiere instalación/autorización del dueño (#378) | — |

## 7. Por qué no se reinterpreta ni se trunca

- **No reinterpretar**: en v0.1.1 el payload de muestras son bytes; leerlos como
  `float` es UB además de sobrelectura (V7). La conversión por byte es la del
  formato de flujo de BASE 1.0.7/1.0.8: **`byte - 126`** y luego el DSP convierte
  dB→potencia (GUI upstream v0.1.2, `c74df4a3`). El offset `150` que se había
  usado antes era de otro payload y produjo `NaN`/waterfall invisible en la
  prueba física; ver [physical-fix-378.md](physical-fix-378.md).
- **No truncar el paquete**: el paquete son 552 B y no se recorta; lo que cambia
  es cómo se **proyectan** las 512 muestras al DSP (512 floats).
- **Stop explícito**: si el header no coincide (p. ej. v0.1.0 en el build tree de
  hoy), el compile falla con `BLOCKED_GUI_SAMPLE_ABI` (V4) en lugar de compilar
  una GUI que lea memoria de más.

## 8. UNKNOWNs y límites

- `payload_authenticity = UNVERIFIED`; `RELEASE_NOTE_HASH_MISMATCH` abierto
  (declarado `29620674020e748ffd70173f0b2920dc0174d2733232d92a2a512111e5228698`
  ≠ calculado `a968d2786d5003012bcc8485d287fcde93739e018b22aeb93a89dbc695d0bc3f`).
  Esto no se resuelve en este PR.
- Bytes reales del wire de BASE 1.0.8: `UNKNOWN` (sin captura; prohibido). El
  layout viene de la RE estática de PR #380 sobre un payload no autenticado.
- El valor 150 se había tomado de otro payload de la APP vendor; **no es** el
  contrato del flujo 1.0.7/1.0.8. La prueba física #378 midió el efecto y el
  código correcto se alineó con la GUI upstream `c74df4a3` (`byte - 126` y
  dB→potencia); ver [physical-fix-378.md](physical-fix-378.md).
- El build tree del owner no se tocó: sigue compilando v0.1.0 (con el guard, la GUI
  **no compilaría** hasta actualizar la librería). El link/build final es de la
  integración.
- No hay validación física, de audio RX ni de MUX. `production_ready=false`.

## 7. Verificación en la integración (parte C, task-16)

Build real en el build tree del owner (`make aether-x6200-control-dirclean &&
make x6200-gui-dirclean && make -j8`, exit 0) con el contenedor
`x6200-buildroot:latest`:

| Check | Resultado |
|---|---|
| Patch de la parte A aplicado por Buildroot | **PASS** — `Applying 0001-flow-support-SLIP-RX-framing-for-BASE-1.0.8.patch using patch:` / `patching file src/low/flow.c` |
| Header v0.1.1 realmente usado en staging | `low/flow.h` sha256 `272028947ba84f327d56f03c1af8a1abdd9f19dc9137e81cb7a673634d4b9094` (= v0.1.1 esperado de §1), `uint8_t samples[512]`, `sizeof(x6200_flow_t)=552` |
| `.so` compilado (sin strip) | sha256 `f2eb4fb1b291c6dd76a0a46a5e326eefd9ad87fe46abe6fe0c1a373fc4dd1138` — idéntico al gate ARM de la parte A (PR #14) |
| `.so` en la imagen (stripped) | sha256 `e93d79a376a974bea61301ee5c90eb0f951d79c00f06918ccdc7827e59d6cad5` |
| ABI exportado | **PASS** — 100/100 símbolos dinámicos idénticos al v0.1.0 previo; `x6200_flow_init/read/restart` exportados; SONAME `libaether_x6200_control.so.0` |
| Static asserts de la GUI con el header real | **PASS** — el build de la GUI compila si y sólo si `sizeof(pack->samples)==512`, `offsetof==4`, `sizeof==552` |
| GUI en la imagen | `usr/sbin/x6200_gui` sha256 `6a068db278c72986967bbbc0d4a12d9f71202507054fbbd3c986746eb40b7c1d`; `src/radio.c`/`radio_samples.h` idénticos al worktree del PR #381 |

La imagen de prueba y su SHA256 están fuera de Git (`fw108-scratch/fw108-image-377/`);
no fue flasheada ni instalada (`physical_rx_test = NOT_RUN`, #378).
