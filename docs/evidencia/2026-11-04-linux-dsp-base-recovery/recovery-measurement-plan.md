# Medición ≤250 ms de recuperación a BASE

## Herramienta
`tools/linux-dsp-base-recovery/recovery_measure.py`:
- Entrada `--events CSV` (`t_ms,event=detection|command`) tomada de logs/observador y `--audible CSV`
  (`t_ms,audio=base`) con **timestamps de grabación externa**.
- Requiere `--external-reference`; sin referencia externa devuelve **BLOCKED** (no inventa medición).
- Calcula detección→command y command→audible por repetición; reporta worst-case, n y objetivo 250 ms;
  verdict PASS/FAIL/BLOCKED.
- `--self-test` sintético sin hardware (PASS sintético + BLOCKED sin referencia) — no es evidencia física.

## Procedimiento futuro (requiere autorización)
1. Grabadora externa + reloj visible; fuente de referencia estable; un solo preset nativo congelado.
2. Capturar `events` (detección/command desde logs del árbitro/supervisor) y `audible` (transición a
   BASE audible marcada en la grabación externa).
3. Repetir 5-10 veces, incluyendo peor caso; reportar medido vs estimado; software timestamps sólo
   complementarios.
4. No provocar fallos destructivos con RX_LINUX activo hasta tener un método de BASE return probado
   independiente; si no existe, STOP y permanecer en `PENDING/BLOCKED`.

## Estado actual
Harness y modelo offline listos; **ninguna medición física ejecutada**. `RESTORE_250MS_PHYSICAL=PENDING`.
