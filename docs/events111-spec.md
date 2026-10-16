# Spec DRAFT — #111: la cola de eventos se satura y tira eventos (flood de Overflow)

Fecha: 2026-10-16. Rama: `factory/events-queue-111` (base `ab7710b3`, trunk tip).
Creado por: orquestador (spec stage). Implementación: delegada al coder.

## Contexto (del issue #111, medido en radio)

- Un run de ~3 h logueó **25.624 `Overflow (events.c line #90)` de 26.313 líneas** (97 %, ~2,4 líneas/s).
- No es constante: en reposo **0** en 10 s; aparece **bajo carga** (radio en uso / apps abiertas).
- `src/events.c` `event_send()`: si la posición siguiente de la circular ya está ocupada, **tira el
  evento** (`return`) y loguea `LV_LOG_ERROR("Overflow")`. Es decir: **se pierden eventos de la UI**.
- Daño: (1) eventos perdidos → "a veces no responde" sin rastro; (2) escritura constante a la SD
  (desgaste) y log ensuciado (tapa avisos útiles); (3) correlación sospechosa con #91 (CPU) y con el
  reinicio del equipo al aplicar NB (#114: el log del run colgado termina en el flood, ~16/s, cortado
  a mitad de línea — precondición probable).
- **Sin fundar** (lo dice el issue): quién produce el flood, qué eventos se pierden, relación con #91.

## Objetivo (orden del pipeline: dar curso al issue; owner sin decidir alcance, decidir con el código)

1. **Fundar**: instrumentar la cola para saber **quién satura** — contadores por tipo de evento (y/o
   productor) de drops y de ocupación pico; sin contenido de eventos (sólo conteos — telemetría sin
   datos privados). La meta es cerrar el "sin fundar" del issue con números.
2. **Mitigar el daño visible**: el log de overflow pasa a **rate-limited** (ej.: primeros N en detalle,
   después un resumen periódico con el total acumulado y los contadores por tipo) — que la SD no
   reciba 2,4 líneas/s eternamente y que el triage siga viendo el problema sin ahogarse.
3. **Fix del cuello de botella** si la fundación lo permite y es seguro sin radio: p. ej. profundidad
   de la cola (hoy cuánta?, medir), o el consumidor (¿la UI consume de a 1 por tick?). Decidir con
   números y justificar en el reporte. Si el fix real requiere la radio para validar, la parte de
   cambio de comportamiento se puede dejar propuesta (no implementada) — declararlo.
   **Cuidado**: no es sólo "agranda la cola" (mascar el síntoma) — si el productor supera
   estructuralmente al consumidor, agrandar sólo atrasa el desborde. Fundar antes de decidir.

## Invariantes (no romper)

- No tocar RF/TX/DSP ni `lvgl/` (pinned 74d0a816). `events.c` es infraestructura de GUI: el cambio
  debe preservar semántica de `event_send`/`event_recv` para todos los callers (grep de callers).
- Commits `Pablo Iaria <iariap@gmail.com>`; sin push (el orquestador publica); `.asf-active` sin
  commitear; una sola tarea escritora por worktree.
- TDD obligatorio donde se pueda (la cola es C plano + pthread: testeable nativamente con stubs del
  log LVGL — `tests/native/`); RED pegado con salida real.
- Gates completos con salida real: nativos (incluye `-fanalyzer`), estáticos 7/7, determinismo,
  A/B compare (pixel-idéntico esperado si el cambio es de lógica no-render), contracts/audit/themes,
  ruff/format/ty, pytest (+e2e).
- Banco QEMU para evidencia (el comportamiento del log puede verse en el arnés); el **síntoma**
  (flood bajo carga con radio real) es `requires-radio` — declararlo si aplica.

## Criterios de aceptación

- Contadores de drops por tipo (+ total, + pico de ocupación) expuestos donde sea razonable (log
  resumido y/o acceso nativo para tests).
- Log de overflow rate-limited con prueba de que el volumen de log acota (test: N drops → ≤ K líneas).
- Fundación en el reporte: quién satura (con evidencia del código/instrumentación), qué se perdió,
  y la decisión de fix con su matemática.
- Spec commiteada como `docs/events111-spec.md`; reporte final con TDD, gates, HEAD+base y PENDIENTES.
