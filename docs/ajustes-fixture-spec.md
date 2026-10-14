# Spec: ajustes-fixture — corrida dedicada: fixture del diálogo de Ajustes en el arnés

**Fecha**: 2026-10-14 · **Corrida**: `factory/ajustes-fixture` · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-`f360f92` · **Issue**: #11 (semillas) + desbloqueo de #14 incr. 5 y #10/#12 restante

## Por qué (el eslabón grande pendiente)
UI-02 declaró `Ajustes`/`dialog_settings` como **UNSUPPORTED** porque el arnés no compila/dialog_field+decenas de deps. Ese es el bloqueo de: #14 incr. 5 (settings UI/persistencia de la política), #11 (semillas UI-002/004/005/007/008), y el resbalón restante de #10/#12 (settings rows). Según el plan, esa dep-declaración vale — pero merece un **intento dedicado** con los skills del arnés actuales (mocks más maduros,subjects/scheduler ya probados en main_screen, dashboard del banco #24) antes de dar el bloqueo por definitivo.

## Alcance
1. **Investigar** el camino real del arnés para compilar `src/dialog_settings.c` (+deps reales en el bloque del main_screen de hoy). Identificar DIámetro cada dep que crashea/falla y qué mocking mínimo honesto necesita (no "stub total": mis los servicios). Listar todas las colisiones.
2. **Si viable**: fixture `settings-home.scn` (Abrir Ajustes) + fixture adicional `settings-wfbat.scn` (Ajustes con el toggle del waterfall_battery activado desde la policy conf.), screens builders, coverage honesto (REAL/PARTIAL per componente).
3. **Gate del fixture** (si viable): determinismo, contracts (regiones de Ajustes: qué elementos miden ahí), audit; `compare` con 20+22 fixtures → la cobertura total de las 22 en `docs/AB_TESTING.md`.

## Exclusiones
**NO implementes races fixes (visual) en Ajustes en esta corrida** — el objetivo es que el arnés pueda **renderizar** Ajustes. Los fixes de los issues UI-002..008 siguiente corrida independiente.

## Criterios (finish condition)
- SI viable: new fixture(s) corridos en determinismo 22×2 y `compare` + `contracts`/`audit`/benchmark, en PR.
- SI NO viable tras investigación: report de BLOCKED con la lista REAL de problemas de deps y lo que CADA UNA requeriría (sin inventar). (Los issues declaran la no-viabilidad y ## cierran el registro con la consecuencia declarada.)

## Riesgos
El diálogo de Ajustes referencia **todo** el árbol de widgets del firmware (toggles con handlers que llaman radio/sonido/backlight, sliders, spinners, band selector, semáforos). Mockear los side-effects honestamente puede ser grande. Si el "decenas de deps" se confirma, el provecho del framework queda con el banco (PR #24) como único canal — y la física final lo convalida — CERO fake A/B.
