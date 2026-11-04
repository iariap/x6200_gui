# Rollback
No hay deploy en este PR. En futura sesión autorizada: detener QA, borrar `/etc/x6200_route_qa.enable`,
matar servicio/sink, restaurar GUI original (sha256 respaldado) y firmware oficial 1.0.8; verificar un
solo dueño de `/dev/i2c-0`, `tx=false`, BASE audible, estado del operador restaurado. Ante TX/ATU
inesperado, BASE ausente o MUX incierto: rollback inmediato, no reintentar sin diagnóstico.
