# Contrato de seguridad propuesto (diseño árbitro; offline)

Producto: el RX procesado por Linux se oye por el parlante; BASE nativo es fallback. Seguridad:
**nunca** dejar el parlante en una ruta no intencional ni bits compartidos alterados.

## Estados

| estado | significado | permite entrar RX_LINUX |
|---|---|---|
| BASE | ruta nativa, bit voice_rec=0, sombra conocida | sólo tras precondiciones |
| READY | precondiciones ok, stream listo, sin DAC | no (transición) |
| RX_LINUX | bit 0x8=1, PCM post-Speex escribiéndose | es el estado activo |
| DEGRADED_BASE | falló/terminó la ruta; se escribió BASE; no se declara "base confirmada" sin verificación | sólo tras re-init |
| FAULT | error de bus/restore/unknown; no se escribe más hasta reset | **nunca** |

## Invariantes (modelo y tests)

1. **BASE inicial** al crear/arrancar; **sin auto-enable**; `enabled=false` por defecto.
2. **Verdad de árbitro**: el árbitro es el único escritor del registro 12; su sombra es el último
   valor escrito con éxito. Antes de la primera escritura BASE exitosa: `UNKNOWN` ⇒ nunca habilita.
3. **Dueño único**: sólo el árbitro escribe. La GUI/worker envían heartbeats/comandos por IPC; no abren
   `/dev/i2c-0`. `x6200_control_idle` deja de escribir directo (proxy). No hay I2C concurrente.
4. **Enable sólo si**: truth=ARBITER, heartbeat fresco (≤100 ms), `last_write_ok`, PCM post-Speex
   válido, sin TX/ATU/SWR/poweroff/modem, sin audio generado, no FAULT. El enable cambia **sólo 0x8**;
   los demás bits se copian de la sombra de la GUI.
5. **TX/PTT/ATU**: antes de cualquier bit de TX, el árbitro escribe `sombra & ~0x8` y exige éxito; recién
   entonces permite los bits de TX. Si la escritura falla ⇒ FAULT y **no** se permite PTT.
6. **Restore independiente**: heartbeat/lease vencido, GUI exit/crash, worker/sink/capture/PCM stale ⇒
   el árbitro (independiente de la GUI) escribe BASE y entra DEGRADED_BASE; si falla ⇒ FAULT.
   Presupuesto objetivo ≤250 ms desde el último heartbeat válido.
7. **PCM**: sólo PCM **post-SpeexDSP una vez**; 44.1 kHz mono S16, bloque ≤4410; bloque duplicado se
   rechaza (no double NR); ring fijo 4 bloques, overflow descarta el más viejo y cuenta;
   underrun/stale no inventan audio y suspenden. No bloquea el callback, no asigna memoria en él.
8. **No regresiones**: recorder/decoders/TTS/CW siguen recibiendo el mismo PCM; no se agregan
   menús/settings/defaults; no se toca `audio_play_en` como ruta RX permanente.
9. **Fallback**: cualquier duda/fault ⇒ native BASE (bit 0x8=0) y supervisor relanza GUI si hace falta.

## Integración (branch-local, no implementada en producción)

`x6200_route_arbiter` (proceso único, lanzado por `x6200_daemon` antes de la GUI) abre `/dev/i2c-0` y
expone socket `/run/x6200_route.sock` (0600) para: `apply_reg(reg,value)` desde libaether proxy,
`route_acquire(lease)`/`route_release`, `heartbeat(reg12,write_ok,seq)`, `tx_prepare`, `status`.
La GUI/libaether no abre I2C. El router #364 usa el backend `mux_request` → arbitro.
Este PR **no** implementa ese proxy en `src/` (ver `executive-report.md`): queda contract+modelo.
