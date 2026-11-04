# Ciclo de vida (ROM init/x6200_daemon, sin systemd)

- `x6200_daemon` sigue siendo el supervisor de GUI (backoff 2/5/15/60 s). Con feature OFF **no** arranca
  nada nuevo: BASE exacto, sin servicios/IPC.
- Con feature ON instalada **sólo si el binario existe** y el gate runtime existe: arranca
  `x6200_route_service --arbiter`; el árbitro hace **boot a BASE antes de aceptar IPC** (ningún enable
  parcial de MUX); GUI arranca aparte y no habilita MUX sin gate/QA.
- Supervisor separado `--supervisor` sólo monitoriza vía IPC (no escribe reg12); si el árbitro no
  responde sale `!=0` con `SUPERVISOR_ARBITER_UNAVAILABLE` y `BLOCKED_PHYSICAL_RECOVERY`.
- Shutdown: SIGTERM/SIGINT en el servicio ⇒ `shutdown_safe` (BASE si posible), invalida sombra y declara
  físico no confirmado; el daemon hace SIGTERM al servicio en cleanup.
- No se agregó framework/servicio obligatorio en OFF; no se crean settings/UI. Activación QA no es
  operator-accessible.
