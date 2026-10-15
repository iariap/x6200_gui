# Evidencia: el tema Cathode (THEMES v2, issue #70) — 2026-10-15

Los renders del **arnés de UI** (`tools/ui_harness`, host x86) con el tema **Cathode** en cada
estado que el issue pide: pantalla principal (RX), waterfall en movimiento, TX simulado,
batería baja + ECO, un diálogo de app y Ajustes con el foco sobre la fila del tema. Cada
fixture tiene el mismo estado de radio que su gemela de otro tema, así que **el tema es la
única variable** del render.

**Esto NO es la radio.** Es el render determinista del arnés sobre el mismo código
(`src/theme.c`, `src/styles.c`, `src/main_screen.c`, `src/fonts/`): sirve para medir tipografía,
geometría, contraste y costo de dibujo, y no dice nada del brillo al sol ni del consumo. La
captura del **binario ARM real** (QEMU user-mode / banco) se consolida en el issue de
integración #73, con el banco reservado para esa pasada.

## Cómo se reprodujo

Desde `tools/ab_testing`, en la revisión de la rama del tema (`170ac89`):

```bash
uv run x6200-ab capture --revision HEAD --label cathode70 \
    --scenario main-tema-cathode --scenario main-tema-cathode-agua \
    --scenario main-tema-cathode-tx --scenario main-tema-cathode-eco \
    --scenario bt-dialog-tema-cathode --scenario settings-tema-cathode \
    --out docs/evidencia/2026-10-15-theme-cathode/harness
```

El arnés es determinista (`tests/check_harness_determinism.sh`: dos corridas del mismo
escenario dan el mismo sha256), así que estos PNG se pueden volver a generar byte a byte. El
sha256 corto de cada uno coincide con el que imprime el gate de determinismo para esa fixture.

## Los seis estados

| Fixture | Estado | sha256 (16) | flushes | px dibujados |
|---|---|---|---|---|
| `main-tema-cathode` | principal, RX: frecuencia DSEG14, modo, VFO, S-meter, band plan, cascada | `50fbfb03309a6f34` | 6 | 1 126 520 |
| `main-tema-cathode-agua` | waterfall en movimiento (peaks, línea central, espectro lleno) | `fcc0f2eaf4c392f0` | 6 | 1 126 520 |
| `main-tema-cathode-tx` | TX simulado (visor TX, split, `tx_bg`/`tx_box`) | `34babf3cec40f654` | 6 | 1 126 520 |
| `main-tema-cathode-eco` | batería baja (10.6 V, reloj en %) + política en ECO | `1d11566a42766541` | 5 | 994 520 |
| `bt-dialog-tema-cathode` | diálogo de app (`dialog_bt.c`): selección, texto atenuado, acción | `b17ebb882c142786` | 10 | 1 568 082 |
| `settings-tema-cathode` | Ajustes con el foco sobre la fila del tema (`TC(focus)`) | `b37b2bd58d75d3b8` | 6 | 1 002 010 |

Cada PNG viene con su JSON del arnés (cobertura por componente —`theme.c`, `styles.c`,
`fonts/` y `main_screen.c` en REAL—, métricas de dibujo, `radio_calls` y el bloque `theme`, que
reporta `requested`/`params_name`/`active` = `Cathode`: el nombre sobrevive el round-trip del
store de 31 caracteres).

## WCAG (medido)

Estático (`tests/check_wcag.py`, 128 pares aplicables de Cathode, **0 fallas**):

| par | ratio | mínimo |
|---|---:|---:|
| `text` / `bg` | 14.48:1 | 4.5 |
| `text` / `panel` | 14.24:1 | 4.5 |
| `text_value` / `bg` | 16.25:1 | 4.5 |
| `btn_text` / `btn` | 10.45:1 | 4.5 |
| `btn_active_text` / `btn_active` | 6.05:1 | 4.5 |
| `focus_text` / `focus` | 11.00:1 | 4.5 |
| `sel_text` / `sel_bg` | 8.06:1 | 4.5 |
| `text_head` / `bar` | 12.13:1 | 4.5 |
| `mode_dig` / `bar` | 14.33:1 | 4.5 |
| `tx` / `tx_bg` | 5.75:1 | 4.5 |
| `control_border` / `dialog` | 4.26:1 | 3.0 |
| `bar_border` / `bar` | 3.14:1 | 3.0 |
| `btn_border` / `btn` | 3.67:1 | 3.0 |
| `sel_border` / `sel_bg` | 5.80:1 | 3.0 |
| `card` / `bg` | 4.90:1 | 3.0 |

No-texto por píxeles (`x6200-ab themes`, WCAG 1.4.11, sobre
`tools/ab_testing/golden/main-tema-cathode.png`): el arco `meter_over` del S-meter **8.11:1** y
la columna de señal de la cascada **6.82:1** (mínimo 3:1). Los pares que sólo existen en
Ajustes: 9/9 OK. `x6200-ab themes` cierra con 15 temas servidos, 135 pares y **0 fallas**.

## Costo de dibujo (x86, no el Cortex-A7)

`x6200-ab benchmark --revision HEAD --scenario main-tema-cathode --scenario main-tema-classic`
(plan del presupuesto: 5 repeticiones, 4 rondas, 20 frames; mismo estado de radio, sólo cambia
el tema):

| escenario | p50 | p95 | p99 | fps | cv | flushes |
|---|---:|---:|---:|---:|---:|---:|
| `main-tema-classic` | 3.248 ms | 3.281 ms | 3.289 ms | 307.8 | 0.4% | 1.0 |
| `main-tema-cathode` | 3.248 ms | 3.307 ms | 3.310 ms | 307.8 | 1.4% | 1.0 |

Sin regresión medible: p50 0.0%, p95 +0.8%, fps 0.0%. El aislamiento de la familia
segmentada (que agrega la capa del "8 fantasma" en la fila de frecuencia) no muestra costo
propio: `main-foco` vs `main-frecuencia-dseg`, p50 3.243 vs 3.247 ms. Los reportes crudos quedan
en `tools/ab_testing/reports/BENCH-CATHODE70*`.

## Lo que esta evidencia NO cubre

- **La radio**: ni el banco ni la pantalla física (issue #73). Nada acá afirma brillo,
  legibilidad al sol, consumo ni comportamiento en el Cortex-A7.
- **Las apps que el arnés no abre** (JS8/FT8, cluster, WeFax, finder): sus roles (`row_*`,
  `ft8_*`, `age_*`, `finder_*`, `alert_*`) están escritos en el JSON y son coherentes con la
  paleta, pero ninguna fixture los rinde, así que no hay medición por píxeles de ellos.
- **La barra de botones inferior** se dibuja con el bitmap de la ROM (`theme_image()` no tiene
  llamadores en esta revisión), así que se ve igual en todos los temas; de Cathode llegan ahí el
  recolor de botón activo (`btn_mark_active`) y el de deshabilitado (`bg` + `text_mute`).
