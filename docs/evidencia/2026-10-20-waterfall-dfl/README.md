# Capturas del waterfall con distintas configuraciones de filtro DFL

Pedido del dueño (2026-10-20): **5 capturas del waterfall con configuraciones de filtro DFL bien
variadas** para rediseñar la UX. Las capturas muestran la pantalla principal completa (waterfall +
espectro + S-meter) con la **página DFL** cargada abajo, así se ven los valores:

`Filter low` · `Filter high` · `Filter BW` · `Eq` (botones de `page_dfl_1`, `src/buttons.cpp:478`).

## Las 5 configuraciones

| # | Archivo | Modo | Frecuencia | Filter BW | Filter low / high | Para qué sirve en el rediseño |
|---|---|---|---|---|---|---|
| 1 | `01-cw-50.png` | CW | 14.030 MHz (20 m) | **50 Hz** (mínimo) | 1575 / 1625 Hz | Filtro "hilo": el más angosto que acepta el control remoto (rango 50..6000) |
| 2 | `02-cw-500.png` | CW | 7.030 MHz (40 m) | **500 Hz** | 500 / 1000 Hz | CW típico de concurso |
| 3 | `03-ssb-1950.png` | LSB | 7.120 MHz (40 m) | **1950 Hz** | 625 / 2575 Hz | SSB angosto (el que usa el dueño: su equipo estaba en 1950 al empezar) |
| 4 | `04-ssb-3300.png` | USB | 14.200 MHz (20 m) | **3300 Hz** | 400 / 3700 Hz | SSB ancho, para ver el límite práctico antes de AM |
| 5 | `05-am-6000.png` | AM | 7.200 MHz (40 m) | **6000 Hz** (máximo) | -1825 / 4175 Hz | AM: el filtro más ancho y **asimétrico** (low negativo) |

Variedad buscada: **dos modos de demodulación distintos** (CW y SSB/AM), **dos bandas** (20 m y 40 m),
**cuatro valores de BW** (50 → 6000 Hz, tres órdenes de magnitud) y **filtros simétricos y asimétricos**.

## Cómo se hicieron (reproducible)

```sh
# 1) cargar la página DFL y fijar modo/frecuencia/filtro por el socket remoto
echo '[{"cmd":"page","text":"dfl"},{"cmd":"mode","val":"CW"},
       {"cmd":"freq","val":14030000},{"cmd":"filter","val":50}]' > cmds.json
./.dfl_run.sh cmds.json          # helper local: manda los comandos al socket /tmp/x6200_remote.sock
# 2) esperar 4 s (que el waterfall junte tramas) y capturar el framebuffer
sleep 4
tools/captura-radio.sh docs/evidencia/2026-10-20-waterfall-dfl/01-cw-50.png
```

- El comando `filter` acepta **50..6000 Hz** (`src/remote_server.cpp:767`); `filter low/high` los recalcula
  la GUI (no se pueden fijar por separado por socket).
- Las capturas son el framebuffer **480x800x32 BGRA** rotado 270° (`tools/captura-radio.sh`), o sea
  800x480 apaisado, colores reales.

## Entorno y estado

- Build en la radio: **`90548cbc2926e7539c1d78bc0b7cba2f`** (tip `lu2dsx/x6200-rom` al momento de capturar).
- Radio en RX, sin transmisión, 40 m/20 m, sin tocar potencia ni ajustes de audio.
- **Estado del dueño restaurado al terminar**: 7.119.914 kHz, LSB, Filter BW 1950, ATT on, página GEN.
- Antes de capturar hubo que **relanzar la GUI** (kill -9, el daemon la relanzó) porque el socket remoto
  había quedado muerto tras el arranque: dos instancias de GUI se pisaron el archivo del socket
  ( apuntaba a un inodo sin listener). Queda reportado como issue.

## Uso en la wiki

Estas 5 imágenes son la base de la página **Waterfall y filtros DFL** de la wiki del repo
(`https://github.com/iariap/x6200_gui/wiki`), pensada para rediseñar cómo el waterfall comunica el
filtro de recepción.
