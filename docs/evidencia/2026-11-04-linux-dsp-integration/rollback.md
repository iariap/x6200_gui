# Rollback (queda DEFAULT OFF; no hay deploy en este PR)

## Estado de este PR
No se despliega nada: la feature se compila sólo con `-DX6200_LINUX_DSP_ROUTE=ON`; por defecto **OFF**.
No se modifica `src/audio.c`, `src/radio.c`, `audio_enhancement`, X6200Control ni el daemon. El build
normal sigue BASE, sin servicios nuevos ni menús.

## Rollback si en el futuro se habilita (build de prueba)
1. Restaurar `/usr/sbin/x6200_gui` original desde `/mnt/ui-bak-*` (hash verificado) y el build rootfs
   original; no tocar firmware MCU/FPGA (nunca se flashea).
2. Detener el servicio del árbitro/supervisor (si se instaló) y verificar que no quedan procesos ni
   sockets; el daemon normal queda en BASE.
3. Verificar `get` (`tx=false`), audio nativo BASE audible, un solo `x6200_gui`, sha256 original,
   `/dev/i2c-0` con un solo dueño (GUI).
4. Restaurar ajustes/volumen del operador desde el snapshot previo.
5. Confirmar feature OFF en la build de producción.

## Criterios de aborto
Cualquier TX no intencional, GUI colgada sin restore, audio ausente/incorrecto, MUX que no vuelve a
BASE ⇒ restaurar original y no reintentar sin diagnóstico.
