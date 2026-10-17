# Spec DRAFT — #119: la curva de ROE sale escalonada (no parábola), la app se cierra al terminar el barrido y hay fantasma de repintado

Fecha: 2026-10-16. Rama: `factory/roe-staircase-119` (base = trunk tip del momento).
Creado por: orquestador (spec stage). Implementación: delegada al coder.

## Contexto (del issue #119, medido en radio)

La radio corre **merge #117** (`fe67ded`) — el que hizo la parábola — y el owner dice **"no es una parábola todavía"**. Medido (6 cuadros únicos del framebuffer leídos en bucle, `docs/evidencia/2026-10-15-validacion-arreglos/frame-*.png`):

1. **Curva escalonada**: mesetas planas (~1,0–1,5) con **escalones verticales** — no hay forma de parábola. La app dibuja una línea, pero escalonada. Entorno: `Scale: Log`, `Span: 50 kHz`, ROM `c640645d810c`, GUI `e294229dd32d` (commit fe67ded), antena 40 m, TX 3 W.
2. **La app se cierra sola al terminar el barrido** (tres capturas a 8/14/22/34 s → pantalla principal, **mismo pid**, sin CRASHLOG, `app` vacío). Antes del #117 la app se quedaba abierta → **posible regresión de nuestra corrida** (o preexistente desatada con el barrido rápido nuevo). Pierde el gráfico justo cuando terminar el barrido.
3. **Fantasma de repintado**: al Run, sobre el gráfico vacío se ve un rectángulo tenue con etiquetas viejas (`180-300`) — región sucia del frame anterior (`25c-roe-instantanea.png`).

**Sin fundar (lo declara el QA):** por qué sale escalonada. Candidatos: datos ya cuantizados/filtrados que el fit no suaviza en el render (¿el draw usa `data_filtered` crudo en vez de la curva del fit?), cuantización del valor de SWR (٪.25?) que el resample no interpola, o que el resample de `roe_curve.c` esté desadaptado del nuevo conteo de puntos (41/51 tras #117). Nota: **`Scale: Log`** — el fit se corre en espacio lineal; en escala log la parábola se deforma: verificar si el dibujo aplica la escala log **después** del fit (correcto: fit lineal → transform log al dibujar) y si el clamp/comprimido del rango 1..99 hace escalones.

## Objetivo (fundar antes de tocar)

1. **Reproducir el escalón nativamente** (TDD RED):pipeline con entrada **cuantizada/escalón** (mock de ADC/filtro con valores de meseta) → `roe_curve_quadratic_fit` → resample → **lo que el draw dibuja** (unidades/space del chart incluido el log/clamp). El test debe PROBAR los escalones dibujados (paso de muestra > ~2 px por segmento, o el draw calcando un camino no suave) y fallar hoy. Pegar la salida RED.
   - Antes, la **lectura del código** del render de `dialog_swrscan.c` (¿dibuja la curva del fit o samples? ¿la geometría nueva 41/51 está coherente con el resample y el width del chart?).
2. **Fix del escalón** según lo fundado (render-only: pipeline de medición intacto — la lectura sigue de las muestras). Criterios: la curva dibujada pasa por cerca de los samples medidos pero **sin mesetas > ~2 px** de alto y sin escalones a 90°; el valle mantenible legible; en forma de parábola (monotonic-ish around the vertex).
3. **Fix del auto-close** (defecto 2): fundar en código por qué la app se cierra al terminar (¿`radio_stop_swrscan` cierra el diálogo? ¿a darka del flag de cierre #85? ¿cambia con muestras completas post-#117?). Fix con TDD: al completar el barrido la app permanece abierta (y cerrarla es decisión del usuario). Si el cierre viviera en archivo compartido (radio.c/dsp), fundar el path exacto antes de tocar y razonar riesgo.
4. **Fix del fantasma** (defecto 3): región del chart unsaid al abrir/Rerun (invalidate full region al abrir o al comenzar barrido — API exacta del invalidate de LVGL a mano; viewport del chart). Verificable en el arnés? (A/B del diálogo → el ghost es visual: si el arnés reproduce el estado pre-Run, el fix aparece en el A/B de forma no-izquierda — declarar; si no, natively testea la invalidación con el stub del chart).

## Invariantes (no romper)

- **Display-only**: el pipeline de medición (`data[]`, `data_filtered[]`, radio flow, TX) intacto; la lectura mínima sigue de las muestras medidas. No tocar RF/TX/DSP ni `lvgl/` (pinned).
- Los conteos/techo de #117 (`41/51/1785 ms`, `_Static_assert`) intactos.
- Una sola escritor por worktree; commits `Pablo Iaria <iariap@gmail.com>`; sin push; `.asf-active` sin commitear; TODO foreground.

## Verificación

- Nativos **con sanitizers si se puede** (la lección de #111: en este host GCC no tiene libasan; usar wrapper clang-18 local si el agente puede — el CI corre con ASan siempre), completos: incluye las suites de SWR Scan (#85, 13 checks) y boost a las nuevas.
- Estáticos 7/7, determinismo, A/B compare (`--b <commit-de-codigo>`, pixel-diferencias esperadas SOLO en los escenarios del diálogo SWRScan si el fix cambia el render; si es pixel-idéntico, explicarlo — el banco no tiene radio), contracts/audit/themes, ruff/ty, pytest.
- **Banco**: humo del diálogo (Run/Stop sin crash) — el escalón real requiere radio (`requires-radio` declarado). Las 3 capturas de evidencia + actualización del README con lo visible.
- Cross-compile ARM del código cambiado.

## Criterios de aceptación

- RED → GREEN del escalón (el test nuevo muestra mesetas en el RED).
- Auto-close imposible: test de fin-de-barrido con la app abierta (y cerrar es manual).
- Fantasma: región limpia al abrir (test si es tauable).
- Gates completos, spec commiteada `docs/roe119-spec.md`, hallazgo(si aporta) en `docs/HALLAZGOS.md` con `audita-hallazgos` exit 0, reporte final con TDD, gates, HEAD+base y PENDIENTES.
