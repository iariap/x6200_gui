# Banco QEMU — la cola de eventos no se satura en reposo (#111)

**Fecha**: 2026-10-16 · Rama `factory/events-queue-111` · HEAD `e7684242`
**Binario probado**: `md5 16ac31c854f46a567ee003b8e4d2bd54` (13.747.752 B, "es el firmware del port", 3/3 marcas)
**Fuente**: `src/ + lvgl/src` → `md5 428809ef0a5c3d3aefcca0956075fa90` · QEMU_CPU `cortex-a7`

## Método

```bash
cd <worktree>
export CHROOT=/home/pablo/desa/x6200/.vfix3 \
       BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot
tools/remote/compila.sh                      # cliente del socket de control
tools/captura-banco.sh --check               # preflight: docker+imagen+volumen+chroot+qemu+PIL ok
tools/captura-banco.sh docs/evidencia/2026-10-16-events111-banco/01-home.png
tools/captura-banco.sh docs/evidencia/2026-10-16-events111-banco/02-settings.png '{"cmd":"app","text":"Settings"}'
```

El `captura-banco.sh` hace el rebuild cuando el hash de los fuentes cambió: **dirclean + rebuild** del
binario ARM desde este worktree (el hash cambió: `src/events.c`), y antes de correrlo lo pasa por el
gate #86 `tools/verifica-firmware-port.py` (fail-closed: un build desde `main` sale con exit 0 y deja
un binario que no sirve). `CHROOT` y `BUILDROOT` viven junto al checkout base
(`/home/pablo/desa/x6200/`), no junto al worktree: sin exportarlos, el preflight no los encuentra.

## Resultado

```
OK: /home/pablo/desa/x6200/.vfix3/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13747752 B, 3/3 marcas)
  respuesta: {"t":"ok"}
  captura: 01-home.png (343 colores distintos, 25790 B)
  captura: 02-settings.png (420 colores distintos, 41153 B)
  version: e7684242 Eventos #111: reporte A/B (pixel-identico, contrato y performance PASS)
  fuente: md5 428809ef0a5c (src/ + lvgl/src)
  binario: md5 16ac31c854f4   qemu-cpu: cortex-a7
```

- `01-home.png`: la GUI abre y dibuja la home (waterfall/meter/spectrum, dial en `21.250.000`).
- `02-settings.png`: con `{"cmd":"app","text":"Settings"}` la app Ajustes abre y navega (Date/Time) —
  el camino `event_send`/`event_recv` que este cambio toca sigue funcionando.
- **`crash.txt` no existe** (no hubo crash; el guard del banco lo deja en `$CHROOT/crash.txt`).

### Ausencia de flood en reposo (medido)

`log-banco-reposo.txt` es el log de la GUI (`$CHROOT/out-banco.txt`) durante **127,9 s** en reposo,
con el binario de arriba corriendo en el banco:

```
span    = 127.9 s
lineas  = 1096
overflow= 0        (0 lineas con "Overflow")
events.c= 0        (0 lineas con la firma del issue: "in events.c line #90")
```

En la radio el mismo síntoma era **sostenido** (+4.447 líneas en 240 s = **18/s, el 100% Overflows**,
con la radio en reposo y load 0,84; `docs/PRUEBAS.md`, 16/10 ~17:35). Acá, en reposo, **0 en ~128 s**.

## requires-radio (lo que el banco NO puede probar)

El banco no tiene radio: el log lo dice con nombre y archivo —
`radio_init: La BASE no respondió: la GUI sigue sin control de la radio (src/radio.c:449)`. Sin radio
**no corre el productor que satura la cola** (el candidato medido es `meter_update()` →
`event_send(obj, LV_EVENT_REFRESH, NULL)`, `src/meter.c:319`, ~25/s desde el hilo del
waterfall/DSP). Por eso queda **requires-radio**:

- el **flood sostenido** bajo la radio real y su desaparición tras el fix (el latch
  `queue_write == queue_read` que dejaba la cola trabada para siempre);
- el **resumen de overflow acotado** en vivo (sólo se emite cuando hay descartes, y sin radio no hay
  descartes: no es observable acá);
- el **desglose por tipo** de los contadores nuevos (`event_queue_drops_of()`).

Lo que el banco **sí** prueba: la GUI del port corre con este cambio (gate #86 + rebuild desde el
worktree `e7684242`), navega (camino de eventos sano) y **no floodea en reposo**.
