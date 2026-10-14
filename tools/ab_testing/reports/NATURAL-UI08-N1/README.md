# NATURAL-UI08-N1 — el modo Ahorro del waterfall, medido por perfil

Evidencia cruda del incremento 2 de UI-08 (issue #14): el perfil de política de
batería conectado al modo Ahorro del waterfall (`refresh_period = 4` en ECO /
CRITICAL) y medido con el banco natural de UI-07 incr. 2.

Los números esperados se declararon **antes** de correr en
[`docs/AB_TESTING.md`](../../../docs/AB_TESTING.md) (sección *Wiring de la
política de batería al waterfall*, "Declarado ANTES de correr").

## Cómo se reproduce

```sh
cd tools/ui_harness
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD/../..:/src" -w /src/tools/ui_harness x6200-uiharness -lc '
    cd build && ./ui_harness --scenario ../fixtures/main-natural.scn \
        --png out/quality.png --json out/quality.json --bench-natural &&
    ./ui_harness --scenario ../fixtures/main-natural-eco.scn \
        --png out/eco.png --json out/eco.json --bench-natural &&
    ./ui_harness --scenario ../fixtures/main-natural.scn \
        --png out/plain-natural.png --json out/plain-natural.json &&
    ./ui_harness --scenario ../fixtures/main-natural-eco.scn \
        --png out/plain-eco.png --json out/plain-eco.json'
```

## Qué dicen los cuatro JSON

`main-natural` (12.1 V + 13.8 V de fuente externa -> **QUALITY**, `refresh_period = 1`)
y `main-natural-eco` (11.2 V sin fuente externa -> **ECO**, `refresh_period = 4`) tienen
el mismo seed (13), 40 frames y 24 filas de cascada: lo único que cambia es el perfil.

| medición | `quality.json` (QUALITY) | `eco.json` (ECO) |
|---|---|---|
| rectángulos de cascada `0,235 800x245` | 23 | **5** (+1 fusionado con el band-info) |
| redibujos de cascada | 24 (uno por fila) | 6 (1 de cada 4) = **1176000 px** |
| px invalidados de cascada | 23 x 196000 = 4508000 | 5 x 196000 = **980000** |
| cadencia de cascada por fila (24 filas) | 196000 | **49000** (196000/4) |
| `natural.totals.invalidated_px` | 6745720 | **3474360** |
| `natural.totals.invalidated_rects` | 45 | 33 |
| `natural.totals.joined_px` | 784000 | 196000 |
| `natural.totals.refresh_cycles` | 37 | 27 |
| `metrics.flushes` | 51 | 39 |
| `metrics.flushed_pixels` | 7873400 | 4602040 |
| `frames_with_flush` | 37/40 | 27/40 |

Los timers de espectro/reloj/band-info/BT disparan igual en las dos corridas (17
frames de FFT, 24 filas, 2 ticks de reloj), pero **los rectángulos que el banco
reporta cambian de conteo** (espectro 16 -> 17, band-info 4 -> 8, reloj 1 -> 2):
LVGL fusiona los rectángulos de cada ciclo de refresh, y con la cascada
redibujando 1 de cada 4 ciclos hay 10 frames sin cascada donde las regiones
vecinas dejan de fusionarse. Los eventos del firmware son los mismos; el reparto
en ciclos, no. Es el hallazgo honesto que se anotó en `docs/AB_TESTING.md`.

`plain-*.json` (captura común) prueban que el cambio de cadencia **no mueve la
captura**:

| medición | `plain-natural.json` | `plain-eco.json` |
|---|---|---|
| `metrics.flushes` | 6 | 6 |
| `metrics.flushed_pixels` | 1127680 | 1127680 |

Y el pixel-diff entre los dos PNG de captura común da **1136 px cambiados, con
bounding box (649,10,93,43)** — adentro del reloj de potencia — y **0 px
cambiados dentro del rectángulo de la cascada (`0,235 800x245`)**: la cascada es
pixel-idéntica entre los dos perfiles en la captura común, porque el último
redibujo de la columna (200 filas, múltiplo de 4) ocurre igual.

## Delimitación: el efecto sigue al pack, no al fixture

Dos corridas más del mismo modo, sobre fixtures que ya existían y con **otro
seed** (`baja.json`, `ext.json`), fijan de qué depende la cadencia:

| fixture | pack | perfil | rectángulos de cascada | `totals.invalidated_px` |
|---|---|---|---|---|
| `main-natural-eco` | 11.2 V, sin fuente | ECO | 5 (+1 fusionado) | 3474360 |
| `main-bateria-baja` (`baja.json`) | 10.6 V, sin fuente | CRITICAL | 5 (+1 fusionado) | 3474360 |
| `home-bateria-externa` (`ext.json`) | 12.1 V + 13.8 V fuente | QUALITY | 23 | 6745720 |
| `main-natural` (`quality.json`) | 12.1 V + 13.8 V fuente | QUALITY | 23 | 6745720 |

Dos fixtures distintos con el pack bajo dan **exactamente** el mismo perfil
reducido, y dos fixtures distintos con fuente externa dan **exactamente** el
perfil completo: la cadencia la decide el estado del pack que la política
evalúa, no el fixture.

## sha256

Ver `sha256.txt`. La idempotencia del modo natural en el fixture nuevo está
comprobada: dos corridas con el mismo seed y el mismo conteo escriben el PNG y el
JSON **byte a byte** iguales.
