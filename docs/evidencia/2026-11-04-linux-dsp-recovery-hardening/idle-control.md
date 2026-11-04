# Problema B — idle/control de radio

- **Pérdida silenciosa corregida**: con proxy activo, `idle` sin bulk/snapshot **no** escribe directo;
  registra `ROUTE_ERR_UNSUPPORTED`, incrementa `attempts/failures/blocked`, coalesce el último snapshot
  (`coalesced`, `pending`) y expone `route_libaether_proxy_refresh_status()`. No hay pending indefinido:
  cada `idle` intenta una vez con el snapshot más reciente; el fallo es explícito y consultable.
- **Single owner serializado**: refresco sólo por IPC `REFRESH` al árbitro; merge de `voice_rec`, strip
  TX con ruta activa, lease/bounds, bus-fail⇒FAULT/sombra inválida. Sin segundo writer, sin writes
  individuales (218 B bulk en build parcheada).
- **Compatibilidad**: frecuencia/modo/filtros nativos/volumen/mute/split/VFO/PTT/TX/ATU/poweroff,
  audio generado/TTS, CW/digital permanecen directos por libaether o por proxy según registro 12;
  no hay cambios de UI/settings/presets. Feature OFF: idle original, BASE exacto.
- Build normal: refresh deshabilitado; patched feature-ON: habilitado sólo con QA gate
  (`X6200_LINUX_DSP_ROUTE_QA=1` + `/etc/x6200_route_qa.enable` + `X6200_CTRL_HAVE_BULK_WRITE`).
