# Evidencia: el tema Sprint (THEMES v2, issue #69) — 2026-10-15

Los renders del **arnés de UI** (`tools/ui_harness`, host x86) con el tema **Sprint** en cada
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

Desde `tools/ab_testing`, en la revisión de la rama del tema (`46195bf`):

```bash
uv run x6200-ab capture --revision HEAD --label sprint69 \
    --scenario main-tema-sprint --scenario main-tema-sprint-agua \
    --scenario main-tema-sprint-tx --scenario main-tema-sprint-eco \
    --scenario bt-dialog-tema-sprint --scenario settings-tema-sprint \
    --out docs/evidencia/2026-10-15-theme-sprint/harness
```

El arnés es determinista (`tests/check_harness_determinism.sh`: dos corridas del mismo
escenario dan el mismo sha256), así que estos PNG se pueden volver a generar byte a byte. El
sha256 corto de cada uno coincide con el que imprime el gate de determinismo para esa fixture.

## Los seis estados

| Fixture | Estado | sha256 (16) | flushes | px dibujados |
|---|---|---|---|---|
| `main-tema-sprint` | principal, RX: frecuencia DSEG14, modo, VFO, S-meter, band plan, cascada | `fdc2d45624707937` | 6 | 1 126 520 |
| `main-tema-sprint-agua` | waterfall en movimiento (peaks, línea central, espectro lleno) | `20d54f32bace5d01` | 6 | 1 126 520 |
| `main-tema-sprint-tx` | TX simulado (visor TX, split, `tx_bg`/`tx_box`) | `5b10bfa6bb61bb7e` | 6 | 1 126 520 |
| `main-tema-sprint-eco` | batería baja (10.6 V, reloj en %) + política en ECO | `b077c4be5bc50695` | 5 | 994 520 |
| `bt-dialog-tema-sprint` | diálogo de app (`dialog_bt.c`): selección, texto atenuado, acción | `f21c5d041335ed93` | 10 | 1 566 813 |
| `settings-tema-sprint` | Ajustes con el foco sobre la fila del tema (`TC(focus)`) | `011f9021cc04f4f1` | 6 | 1 002 010 |

Cada PNG viene con su JSON del arnés (cobertura por componente —`theme.c`, `styles.c`,
`fonts/` y `main_screen.c` en REAL—, métricas de dibujo, `radio_calls` y el bloque `theme`, que
reporta `requested`/`params_name`/`active` = `Sprint`: el nombre sobrevive el round-trip del
store de 31 caracteres).

## WCAG (medido)

Estático (`tests/check_wcag.py`, 128 pares aplicables de Sprint, **0 fallas**):

| par | ratio | mínimo |
|---|---:|---:|
| `text` / `bg` | 17.44:1 | 4.5 |
| `text` / `panel` | 17.03:1 | 4.5 |
| `btn_text` / `btn` | 10.60:1 | 4.5 |
| `btn_active_text` / `btn_active` | 9.78:1 | 4.5 |
| `focus_text` / `focus` | 11.79:1 | 4.5 |
| `sel_text` / `sel_bg` | 6.92:1 | 4.5 |
| `text_head` / `bar` | 10.98:1 | 4.5 |
| `mode_dig` / `bar` | 8.15:1 | 4.5 |
| `control_border` / `dialog` | 4.78:1 | 3.0 |
| `bar_border` / `bar` | 3.53:1 | 3.0 |
| `btn_border` / `btn` | 3.94:1 | 3.0 |
| `sel_border` / `sel_bg` | 4.68:1 | 3.0 |
| `card` / `bg` | 3.14:1 | 3.0 |

No-texto por píxeles (`x6200-ab themes`, WCAG 1.4.11, sobre
`tools/ab_testing/golden/main-tema-sprint.png`): el arco `meter_over` del S-meter **5.46:1** y
la columna de señal de la cascada **8.31:1** (mínimo 3:1). Los pares que sólo existen en
Ajustes: 9/9 OK. `x6200-ab themes` cierra con 10 temas servidos, 90 pares y **0 fallas**.

## Costo de dibujo (x86, no el Cortex-A7)

`x6200-ab benchmark --revision HEAD --scenario main-tema-sprint --scenario main-tema-classic`
(plan del presupuesto: 5 repeticiones, 4 rondas, 20 frames; mismo estado de radio, sólo cambia
el tema):

| escenario | p50 | p95 | p99 | fps | cv | flushes |
|---|---:|---:|---:|---:|---:|---:|
| `main-tema-classic` | 3.310 ms | 3.420 ms | 3.693 ms | 302.1 | 1.7% | 1.0 |
| `main-tema-sprint` | 3.262 ms | 3.399 ms | 4.346 ms | 306.5 | 1.4% | 1.0 |

Sin regresión medible: p50 −1.5%, p95 −0.6%, fps +1.5%; el p99 de Sprint es un frame de cola
suelto (cv 1.4%). El aislamiento de la familia segmentada (que agrega la capa del "8 fantasma"
en la fila de frecuencia) no muestra costo propio: `main-foco` vs `main-frecuencia-dseg`, 80
frames, p50 5.743 vs 5.876 ms. Los reportes crudos quedan en
`tools/ab_testing/reports/BENCH-SPRINT69*`.

## Lo que esta evidencia NO cubre

- **La radio**: ni el banco ni la pantalla física (issue #73). Nada acá afirma brillo,
  legibilidad al sol, consumo ni comportamiento en el Cortex-A7.
- **Las apps que el arnés no abre** (JS8/FT8, cluster, WeFax, finder): sus roles (`row_*`,
  `ft8_*`, `age_*`, `finder_*`, `alert_*`) están escritos en el JSON y son coherentes con la
  paleta, pero ninguna fixture los rinde, así que no hay medición por píxeles de ellos.
- **La barra de botones inferior** se dibuja con el bitmap de la ROM (`theme_image()` no tiene
  llamadores en esta revisión), así que se ve igual en todos los temas; de Sprint llegan ahí el
  recolor de botón activo (`btn_mark_active`) y el de deshabilitado (`bg` + `text_mute`).
