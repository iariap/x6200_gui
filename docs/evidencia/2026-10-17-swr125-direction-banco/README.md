# Banco QEMU + arnés — la parábola de ROE invertida (issue #125-fix)

**Fecha**: 2026-10-17 · Rama `factory/swr125-direction` · base `37de9937` · HEAD `eeb6d3a2`
**Binario probado (01/02)**: `md5 b1ba67520079` (13.753.408 B, "es el firmware del port", 3/3 marcas)
**Fuente**: `src/ + lvgl/src` → `md5 20d38ed8f352` · `QEMU_CPU=cortex-a7`
**Chroot**: `/home/pablo/desa/x6200/.vfix3` · `BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot`

Orden del owner (2026-10-17): *"la parábola está invertida: base abajo y las puntas tienen que tender
a infinito positivo"*. La captura del domo que la disparó es
`docs/evidencia/2026-10-15-validacion-arreglos/26-roe-multipasada-final.png` (commit `b39d6014`):
readout `min 1.0 @ 6.891.334 kHz`, banner `Error: timeout de RF`, `n=44/51`, `Span: 200 kHz`.

## Qué muestra cada captura

| Captura | Qué es |
|---|---|
| `01-home-40m.png` | el banco QEMU (binario ARM real del port) con el dial en el **centro del domo del owner** (`{"cmd":"freq","val":6991334}`) — el firmware reconstruido desde estos fuentes arranca y dibuja la home |
| `02-swrscan-abierto.png` | la **ventana real de SWR Scan** de la ROM abierta por el control remoto (`{"cmd":"app","text":"SWRScan"}`): estado `Idle`, readout `sin datos medidos`, `Scale: Log`, `Span: 50 kHz`, `Passes: 1`. La curva **no aparece** (ver abajo) |
| `03-harness-domo-valle.png` | el **arnés de UI** con el domo del owner (`swrscan-owner-40m-domo`): la curva dibujada es el **valle** (base abajo, puntas arriba). Es el GREEN de la forma |
| `04-harness-valle-real.png` | el **arnés de UI** con el valle real de la antena del owner (`swrscan-owner-40m`, seed del #125): sigue siendo un valle, el fix no lo toca |

## Por qué la curva del valle solo la pinta el arnés (03/04) y no el ROM del banco (02)

El banco QEMU corre el **firmware de la ROM**, que **no tiene radio** (no hay paquetes del flow) y
**no compila el backend fake** (`SWRSCAN_MULTIPASS_TX_ENABLED = 0` en `src/CMakeLists.txt`): la ventana
de la ROM no puede dibujar la curva (por eso `02` sale en `Idle` con `sin datos medidos`). La única
superficie que renderiza la curva real es el **arnés de UI** (`tools/ui_harness`), que compila la
ventana REAL (`src/dialog_swrscan.c` + `src/swr_session.c` + `src/roe_curve.c`) sobre el backend fake
con los datos del owner. Por eso `03`/`04` son capturas del arnés.

El banco `02` es el humo del firmware: prueba que el fix (render-only en `src/roe_curve.c`) **compila
dentro del binario de la ROM** y que la ventana abre sin regresión. El `03` prueba la **forma**.

## La medición (03): el domo del owner, ahora valle

`03-harness-domo-valle.png` (fixture `swrscan-owner-40m-domo`, dataset `owner_40m_domo` embebido en
`src/swr_backend.c` a partir de la captura `26-roe-multipasada-final.png`):

- estado **`Completed`**, `Span: 200 kHz`, `Scale: Linear`, `Passes: 1`;
- readout **`min 1.0 @ 6.891.334 kHz (n=44/51)`** — los **mismos números** que la captura del owner
  (mismo mínimo medido, mismo `n=44/51` del timeout de RF);
- la curva dibujada es una **U**: base en el piso (`y=398` en `x=284`), borde izquierdo `y=191`
  (sube **207 px**), borde derecho `y=121` (sube **277 px**).

Números medidos por `tests/swrscan_curve_evidence.py` (banda del chart `y=69..399`, cuarto inferior
`y>=316`). Con el `roe_curve.c` de la base (37de9937) la MISMA fixture daba el domo invertido: valle
en el **borde** `x=10`, bordes subiendo `0 px` → 3 FAIL (el RED del arnés).

## Gates (esta corrida)

Ver `gates.txt`. El arnés y los tests nativos cubren la forma; el banco cubre el firmware; la
confirmación final del U **en la radio** es del owner (`requires-radio`): el mecanismo en hardware que
produjo un domo no se puede fundar desde el repo (el host no transmite).
