# Protocolo físico (NO ejecutado) — validación de ruta Linux DSP y restore

**Estado:** PENDING / BLOCKED por autorización. No se conectó la radio ni se activó MUX. Este
documento es el protocolo exacto para una sesión autorizada por el owner; no es ejecutable por el agente.

## 0. Precondiciones y seguridad

- Autorización específica del owner por escrito para la sesión y para la build a probar (branch build, no
  oficial); firmware oficial 1.0.8 y GUI original respaldadas; rollback conocido (`/mnt/ui-bak-*`,
  microSD/OEM); operador presente para escuchar y cortar.
- Radio en RX; frecuencia/modo/volumen anotados; sin TX/PTT/ATU en las fases 1–4. Sin USB/BT/audio
  externo. Una sola sesión, un solo `x6200_gui`.
- Criterios de aborto: audio ausente/incorrecto, GUI colgada, radio en TX no intencional, MUX no
  restaura audible, cualquier crash; ⇒ restaurar GUI original y volver a BASE; no reintentar sin diagnóstico.

## 1. Fase F0 — BASE inicial y verdad de control

1. Snapshot: GET (`tx=false`), `lastbreath`, sha256 GUI/lib, procesos (`pidof x6200_gui`), ALSA/PulseAudio
   (solo lectura), `/proc/*/fd` dueño de `/dev/i2c-0`.
2. Confirmar audio nativo audible por parlante (RX nativo, BASE).
3. Confirmar que el router está **OFF/default** y el árbitro en BASE; verificar estado/status del
   servicio (IPC/`status`), no por readback del MCU (no existe).
4. Registro: `SPEAKER_AUDIO_HARDWARE_PRESENT`, `ROUTE_BASE_ACTIVE`.

## 2. Fase F1 — activación controlada y audio post-Speex

1. Activar ruta desde la UI/CLI de prueba (sin menús nuevos en producción): precondiciones verificadas
   (heartbeat, stream READY, PCM post-Speex, sin TX/ATU/generado).
2. Confirmar que suena RX procesado por parlante y que sigue siendo el RX (no TTS/eco).
3. A/B con el owner: OFF vs preset SSB voice; mismas condiciones/volumen; ciego simple.
4. Registrar: `SPEAKER_AUDIO_HEARD_LINUX`, `PRESET_DIFFERENCE_HEARD`, latencia percibida, clipping/
   dropouts (escucha + grabación externa si el owner autoriza), continuidad de RX.

## 3. Fase F2 — recuperación y fallos (sin TX en estas fases)

| escenario | acción | verificación |
|---|---|---|
| GUI colgada | suspender GUI (SIGSTOP) o heartbeat-stop | ≤250 ms: audio vuelve a BASE; estado DEGRADED_BASE; no queda DAC |
| GUI exit/crash | matar GUI (el supervisor la relanza) | restore independiente ≤250 ms desde el árbitro antes del relanzamiento; después GUI en BASE |
| sink caído | pausar/kill PulseAudio del sink o desconectar stream | DEGRADED_BASE, BASE audible, sin crash |
| PCM worker muerto | detener captura/worker (test hook) | suspensión y restore |
| bus/I2C error | simulado en build de prueba (fault injection) | FAULT, no enable, audio nativo |
| underrun/overflow | inyectar bloque/oscilación de carga | no alloc/bloqueo; contadores; audio se degrada a BASE sin colgar |

Registrar `RESTORE_LATENCY_MS` (instrumentación del árbitro) para `RESTORE_250MS_PHYSICAL`.

## 4. Fase F3 — TX/PTT/ATU (sólo con autorización explícita adicional)

1. Con RX Linux activo, solicitar TX (PTT) y verificar que el bit 0x8 se limpia **antes** de PTT, que
   los bits PTT/ATU se preservan y que no hay audio Linux durante TX.
2. Igual con ATU/SWR/poweroff (sin completar transmisión física si el owner no lo autoriza).
3. Verificar que al volver a RX el router no se re-activa solo (no auto-enable) y el estado es BASE/
   DEGRADED_BASE.

## 5. Criterios de PASS/PENDING

- `SPEAKER_LINUX_AUDIO_PHYSICAL=PASS` sólo con F1 audible confirmado por el owner y F2 restore medido
  ≤250 ms en hardware; `RESTORE_250MS_PHYSICAL=PASS` sólo con el valor medido del árbitro.
- F3 da `TX_PTT_ATU_ISOLATION` físico PASS; offline ya tiene PASS de modelo.
- Sin estas mediciones: PENDING/BLOCKED (este PR no declara PASS físico).

## 6. Capacidad faltante para ejecutar

- Owner autorizando sesión y build de prueba; **build integrada** del árbitro/proxy (no existe aún en
  este PR); instrumentación mínima para medir latencia de restore y audio (grabación externa/loopback
  autorizado); opcional: sonda de línea/altavoz. No hay readback del MCU: la verdad física del MUX sigue
  siendo la limitación declarada y se valida por **efecto audible** de BASE/RX (el operador oye la
  diferencia) y por ausencia de audio Linux en BASE.
