# Rollback
No hay deploy en este PR (auditoría offline). En sesión autorizada: detener QA, borrar
`/etc/x6200_route_qa.enable`, matar servicio/sink, restaurar GUI original (sha256 respaldado) y
firmware oficial 1.0.8; verificar un solo dueño de `/dev/i2c-0`, `tx=false`, BASE audible, estado del
operador (CB-D 27.635 USB, filtro 2700, volumen) restaurado. Ante TX/ATU inesperado o BASE ausente:
rollback inmediato y no reintentar sin diagnóstico.
