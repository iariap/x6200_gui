# Banco QEMU — SWR Scan con la parábola y el barrido codificado (#113)

**Fecha**: 2026-10-16 · Rama `factory/roe-parabola-113` · HEAD `b9160d07`
**Binario probado**: `md5 a97cc1cd3adc` (13.743.084 B, "es el firmware del port", 3/3 marcas)
**Fuente**: `src/ + lvgl/src` → `md5 d22c6f6e544d` · QEMU_CPU `cortex-a7`

## Método

```bash
cd <worktree>
export CHROOT=/home/pablo/desa/x6200/.vfix3 \
       BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot
tools/remote/compila.sh                      # cliente del socket de control
tools/captura-banco.sh --check               # preflight: docker+imagen+volumen+chroot+qemu+PIL ok
tools/captura-banco.sh 01-swrscan-abierto.png '{"cmd":"app","text":"SWRScan"}'
# el contenedor banco-apps queda corriendo: se encadena por docker exec
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"btn\",\"val\":0}"'   # Run
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"get\"}"'
```

El primer `captura-banco.sh` hace **dirclean + rebuild** (el hash de los fuentes cambió): compila el
binario ARM desde este worktree y lo pasa por el gate `verifica-firmware-port.py` antes de correrlo.

## Resultado

```
  limpiando la GUI (dirclean): el rebuild reusa una copia propia del repo
  reconstruyendo la GUI: el hash de los fuentes cambio y un fuente es mas nuevo que el binario
OK: .../usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13743084 B, 3/3 marcas)
  respuesta: {"t":"ok"}
  captura: 01-swrscan-abierto.png (315 colores distintos)
  version: b9160d07 SWR Scan: geometria del barrido y techo de TX codif
  fuente: md5 d22c6f6e544d (src/ + lvgl/src)
  binario: md5 a97cc1cd3adc   qemu-cpu: cortex-a7
```

Estado después de apretar **Run** (`btn 0`):

```
{"t":"state","vfo":"A","freq":21250000,"mode":"LSB",...,"tx":true,"app":"SWRScan",...}
```

- `01-swrscan-abierto.png`: el diálogo abre y dibuja el chart (ejes 1,5..4,5 y 21.226 / 21.250.000 /
  21.274), botón `Run`. La línea blanca del borde inferior es la curva con los datos iniciales
  (`data[] = 1.0`): el camino de la parábola corre en el binario ARM y queda **dentro** del área.
- `02-swrscan-corriendo.png`: con el barrido andando (botón `Stop`, modo `LSB`, `tx:true`), la misma
  línea: el barrido arranca y el diálogo sigue vivo. **`crash.txt` no existe** (no hubo crash).

## requires-radio (lo que el banco NO puede probar)

El banco no tiene radio: **no hay paquetes del flow**, así que `dialog_swrscan_update()` nunca corre
y `data[]` queda en `1.0` → la curva es una recta plana en el borde inferior del chart. Por eso la
**forma de la parábola** (un valle limpio, el vértice, el clamp con datos reales) **no se puede
verificar en el banco**: requiere radio. Lo que sí se verificó acá es que el binario ARM del port
compila desde este worktree, pasa el gate #86, abre el diálogo, arranca el barrido (TX), dibuja la
curva por el camino nuevo y **no crashea**.
