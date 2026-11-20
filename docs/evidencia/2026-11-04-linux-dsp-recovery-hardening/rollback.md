# Rollback

- No hay deploy en este PR: default OFF; el build normal es BASE.
- Si en una build de prueba se activó QA: reponer `x6200_gui` original y firmware oficial 1.0.8 desde
  `/mnt/ui-bak-*`/imagen; detener `x6200_route_service`; verificar un solo dueño de `/dev/i2c-0`, `get`
  con `tx=false`, BASE audible y sha256 originales; restaurar ajustes/volumen del operador.
- Criterios de aborto: TX no intencional, GUI sin restore, audio ausente/incorrecto, MUX que no vuelve
  a BASE, cualquier escritura I2C no atribuida al GUI/árbitro único.
