# Análisis de recuperación (restore sin readback)

## Problema del diseño A′ (2026-10-29)

El guardián independiente A′ exige readback real para armarse y para restaurar (`rxmux_restore_apply`
hace read→plan→write→read-verify; sin `read_reg12` devuelve `BLOCKED_NO_READBACK` y no escribe).
R′ (2026-10-30, PR #370) probó el único camino de lectura disponible y devolvió `00000000` estable
(`R_INCONCLUSIVE`): **no hay readback del registro 12**. Por eso:
- el guardián A′ está **INACTIVO** (`RXMUX_HELPER_READBACK_AVAILABLE 0`),
- `x6200_daemon` no lo lanza,
- y el restore independiente ≤250 ms no se ejerce en producción.

Su G2 host (bus mock, procesos reales) había medido p50 101,7 ms / p99 103,3 ms / max 103,3 ms; es
evidencia **host**, no hardware (sha256 en `logs/prior-evidence-sha256.txt`).

## Solución: verdad de árbitro (sin readback del MCU)

Si un único proceso es el **único escritor** del registro 12 y conoce el último valor aplicado, entonces
la verdad lógica es su **sombra autoritativa**; no hace falta leer el MCU. El restore se ejecuta como
`write(sombra & ~voice_rec)`:

- es una operación de **limpieza**: si el bit ya era 0, es idempotente y segura;
- preserva PTT/ATU/resto porque la sombra la mantiene el árbitro (con todas las escrituras proxeadas);
- requiere una única suposición: **el MCU aplica las escrituras exitosas** y ningún otro proceso escribe
  el registro. Bajo dueño único y protocolo, esto se cumple por construcción; la verdad **física**
  sigue sin medirse (limitación declarada, cierra con #363/#365 físico).

## Presupuesto de latencia (modelo)

Constantes: heartbeat ≤50 ms; lease 100 ms; el arbitro consulta el lease cada ≤10 ms (modelo);
restore = 1 escritura I2C (≈ms) + margen. Modelo medido: **restore a 110 ms desde el último
heartbeat**, con presupuesto 250 ms (`logs/native-tests.log`). A′ host previo: p99 103,3 ms con
readback. La diferencia de 100-110 ms corresponde a la ventana de lease; no se acerca a 250 ms.

## Modos de fallo cubiertos por el harness

| fallo | comportamiento esperado | test |
|---|---|---|
| GUI colgada / sin heartbeat | DEGRADED_BASE, BASE escrito, ≤250 ms | `test_restore_on_heartbeat_loss`, `test_gui_exit_restore` |
| GUI exit / SIGKILL (modelo: gui_exit) | restore desde el árbitro | `test_gui_exit_restore` |
| sink/stream perdido | suspende y restaura | `test_sink_lost_restores` |
| PCM stale | suspende y restaura | `test_pcm_stale_restores` |
| audio generado (TTS/recorder) | suspende y restaura | `test_generated_audio_restores` |
| TX/ATU | disable-first; bus fail ⇒ FAULT | `test_tx_prepare_*`, `test_atu_prepare_same_order` |
| bus error en enable/restore | FAULT, nunca habilita | `test_tx_prepare_bus_failure_fault`, `test_restore_bus_failure_fault`, `test_boot_bus_failure_fault` |
| estado desconocido | nunca habilita | `test_boot_base_no_auto_enable`, `test_boot_bus_failure_fault` |
| PCM inválido/duplicado | rechazo, no double NR | `test_pcm_*` |
| ring overflow | descarta más viejo, cuenta | `test_pcm_overflow_bounded` |

## Riesgos residuales (declarados, no ocultos)

1. **Verdad física**: sin readback del MCU, el árbitro no puede probar que el MUX está en BASE; sólo
   garantiza que **intentó** la transición segura. Cierra con instrumentación/prueba física (#363/#365)
   o con readback futuro del firmware.
2. **Escrituras silenciosamente fallidas**: se mitiga exigiendo `write_ok` en heartbeat/enable y
   FAULT ante error; un fallo no detectado del bus seguiría siendo invisible sin readback.
3. **Cambios internos del MCU** al registro 12 fuera de I2C (p.ej. PTT hardware): desconocidos; se
   preservan bits por sombra, pero el hardware puede cambiar bits sin pasar por el árbitro. Requiere
   verificación física; es el mismo límite que A′/QA363.
4. **Integración**: el proxy de I2C y el lanzamiento por supervisor **no** están implementados; sin
   ellos este diseño no es producción.
