# Máquina de estados y precondiciones

| estado | entrada | salida | escribe |
|---|---|---|---|
| BASE | boot ok (escritura BASE confirmada por bus) | enable con precondiciones | reg12=shadow (sin 0x8) |
| READY | enable solicitado con stream listo (no se usa en pruebas) | RX_LINUX | — |
| RX_LINUX | enable verificado | TX/ATU/sink/generado/PCM falta/lease ⇒ DEGRADED_BASE; bus error ⇒ FAULT | 0x8=1, resto = valor GUI |
| DEGRADED_BASE | restore ejecutado | re-enable sólo con re-init/heartbeat/PCM | 0x8=0, resto preservado |
| FAULT | error de bus/unknown/restore fallido | reset/restart del proceso (no auto) | **no escribe** |

Precondiciones de enable (todas): boot ok, sombra válida, heartbeat fresco ≤100 ms, `write_ok`,
PCM post-Speex válido, sin TX/ATU/SWR/poweroff/modem, sin audio generado, sink/captura ok, no FAULT.
Invariantes: BASE inicial; sin auto-enable; unknown nunca habilita; TX entry disable-first; bits
compartidos preservados; restart invalida sombra; FAULT nunca habilita.
