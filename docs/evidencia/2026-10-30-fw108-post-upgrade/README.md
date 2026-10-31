# Prueba RX post-upgrade 1.0.8 + ROM adaptada (issue #378)

Fecha: 2026-10-30 (America/Buenos_Aires). Epic [#374](https://github.com/iariap/x6200_gui/issues/374).
Issue de esta etapa: [#378](https://github.com/iariap/x6200_gui/issues/378). PR de código:
[#381](https://github.com/iariap/x6200_gui/pull/381) (sin merge) y
[AetherX6200Buildroot #14](https://github.com/iariap/AetherX6200Buildroot/pull/14) (sin merge).

## Estado

**`RX_TRIAL_PASS` con la imagen nueva.** La imagen `1e3a9748…` fue escrita en
la SD de prueba, verificada por lectura de vuelta y **arrancó en la radio**; el
propietario confirmó que funciona OK. La evaluación usa la imagen nueva (no el
binario hotfix del diagnóstico).

### OEM 1.0.8 (fuente de la prueba)

- APP **V1.0.8** (Aug 17 2026, 16:21:06)
- FIRMWARE/BASE **V1.0.8** (Aug 19 2026, 16:13:08)
- FPGA **V1.0.0**; HARDWARE **V1.0.0**
- El propietario confirmó **OEM RX y TX OK** antes de la prueba (la TX fue operada
  por el propietario en firmware OEM; el agente no transmitió).

### Historial real de la prueba

1. **Primer y segundo arranque de la ROM experimental:** `> STARTUP DELAYED_`.
   Causa: `S01create_data` no creó un filesystem en p3 (el chequeo previo a
   `mkfs` aceptaba `PARTUUID` como si fuera filesystem); la GUI moría en
   `params_init` por persistencia no disponible y el supervisor la relanzaba.
   Rollback verificado a OEM 1.0.8 (RX/TX), sin tocar BASE.
2. **Retry con DATA formateada a mano:** GUI operativa; la UI quedó fluida y el
   audio RX OK, pero waterfall/espectro quedaban planos en todos los temas.
   Causa medida: las muestras del flujo 1.0.7/1.0.8 son dB referidos a 126; el
   `-150` inicial daba promedio negativo → `log10` NaN → `grid_min/grid_max`
   NaN. Fix en PR #381 (compatibilidad de flujo, commit `57f671e8`), validado en
   radio con binario temporal.
3. **Imagen nueva** (este PR): rebuilt con el fix de GUI y el fix de
   `S01create_data`; escrita en la SD, verificada por lectura de vuelta y probada
   en la radio. El primer arranque creó/formateó DATA y la GUI quedó operativa;
   el propietario confirmó el funcionamiento. Resultado: **`RX_TRIAL_PASS`**.

## Imágenes

| Artefacto | SHA256 | Estado |
|---|---|---|
| Imagen original de #377 | `7ec5d8cc4cb6cbf98fc92af9cf6489ad1a94f5dce8585d43c93156afde68b5d9` | No incluye el fix de compatibilidad; usada para el diagnóstico |
| **Imagen nueva de #378** | **`1e3a9748754a758d4ac83aab673812a7313371a48ec0793884d2f66717e2339b`** | Construida; pendiente escritura/retest |
| GUI en imagen nueva | `03356797959d21c35500d95c23bbc04d8d1f0ac922a89ec3681215d4fdcb982e` | Verificado `sub #126` + `veor q12` en desensamblado |
| Lib X6200Control en imagen | `e93d79a376a974bea61301ee5c90eb0f951d79c00f06918ccdc7827e59d6cad5` | v0.1.1 + patch A (stripped) |
| `S01create_data` en imagen | `da7d19b13d853cbe6e764f60b62194203b704cb595e5f0f3837a3b8f398c6963` | Con fix de `blkid`/PARTUUID |
| Binario GUI del ensayo hotfix | `f3c4c40c3076e8f68361301c5525c600cc9d6669fda986ab6f18aa5873d71423` | Sólo diagnóstico; no es el de la imagen nueva |

La imagen nueva mide 571.473.920 B, tabla MBR, p1 FAT32 32 MiB (`2048..67583`),
p2 ext4 512 MiB (`67584..1116159`), **sin p3** (la DATA debe crearla el primer
arranque con `S01create_data`).

## Criterio de PASS (resumen; detalle en `rx-checklist.csv`)

`RX_TRIAL_PASS` cuando la imagen nueva arranca en la radio y, durante 2–3
minutos en RX: GUI estable, dial/encoder, waterfall con muestras válidas,
S-meter/batería, teclas y audio RX original; sin TX/PTT/ATU/MUX. Primer fallo
relevante ⇒ rollback a la SD OEM 1.0.8 y `NO_GO`.

## Evidencia ya disponible (binario hotfix)

- GUI fluida; audio RX confirmado por el propietario.
- Waterfall con ruido/trazas y refresco continuo (capturas antes/después en
  `docs/evidencia/2026-10-30-fw108-compat/physical-fix-378.md`, rama PR #381).
- FT8 decodificando con la cadena de captura; CW copiando texto con errores
  (pruebas de audio #351, no requisito de #378).
- Sin TX/PTT/ATU/MUX; `tx=false` en todas las verificaciones.

## Rollback

Ver [`rollback-policy.md`](rollback-policy.md). La eMMC conserva OEM 1.0.8; se
probó el retorno apagando y encendiendo sin SD.

## Resultado final

- Imagen nueva escrita en la SD y verificada de vuelta: SHA256 idéntico al
  artefacto `1e3a9748…`.
- Primer arranque: DATA creada/formateada por `S01create_data`; GUI operativa.
- Propietario: **funciona OK**. Sin TX/PTT/ATU/MUX.
- Retest: **`RX_TRIAL_PASS`** con la imagen nueva.
- Integración limpia publicada en **AetherX6200Buildroot PR #15** (rama
  `integration/x6200-main`, contra `main`): parte de `main`, conserva el port
  X6200 del owner, apunta la GUI a `iariap/x6200_gui@57f671e8`, aplica
  X6200Control v0.1.1 + patch de PR #14 y el fix de `S01create_data`.
- Build desde esa rama: `x6200-gui`/`aether-x6200-control` dirclean + `make -j8`
  exit 0; imagen `ac03c8f12ba19d6f51b2d53d09c8b9be11759c3051b653b7ca96d3e9ca0c42e2`.
- Tests herméticos de `S01create_data`: **17 casos, 0 fallas**, incluidos
  `fresh-first-boot`, `existing-ext4-content`, `legacy-vfat-preserved`.
- Queda para la revisión ordinaria: PR #381, AetherX6200Buildroot #14, PR #15 y
  este PR #382; sin auto-merge.
