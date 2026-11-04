# Protocolo físico (NO ejecutado) — hardening

Precondiciones: autorización explícita del owner; build feature-ON patcheada + QA gate; firmware
oficial 1.0.8 respaldado; radio RX, sin TX/PTT/ATU hasta fase autorizada; sin deploy automático.
F1 BASE: audio nativo BASE audible; sin servicio/IPC activo; estado lógico BASE; `physical_confirmed`
false (sin readback). F2 activación QA: verificar disable-first y audio post-Speex por parlante; gate
`PCM_TO_SPEAKER_PHYSICAL`. F3 fallos: SIGSTOP/kill GUI, worker/sink, arbiter SIGKILL y bloqueado,
IPC timeout — medir restore real (≤250 ms) y comprobar BASE audible; `RESTORE_250MS_PHYSICAL` y
`MUX_PHYSICAL_STATE_VERIFICATION` requieren instrumentación/efecto audible. F4 parada segura
(SIGTERM): un write BASE, físico no confirmado. Rollback a GUI/firmware oficial y feature OFF.
Sin PASS físico en este PR.
