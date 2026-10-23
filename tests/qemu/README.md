# `tests/qemu/` — barrido QEMU de las apps reales y cobertura de los diálogos inertes

Infraestructura de **medición** del issue [#222](https://github.com/iariap/x6200_gui/issues/222)
(épica [#216](https://github.com/iariap/x6200_gui/issues/216)). No toca `src/**`: son tests, datos
de cobertura y documentación.

## Qué hay acá

| Archivo | Qué es |
| --- | --- |
| `run_apps_qemu.sh` | El barrido: 3 rondas × las 25 apps de `remote_server.cpp::app_names[]` contra el binario ARM real, en un chroot **privado**, con framebuffer falso y el socket de control. |
| `barrido_qemu.py` | Los datos del barrido: parseo del estado, clasificación `PASS`/`FAIL`/`PARTIAL`/`BLOCKED`, `barrido.json` + `results.csv` + `index.txt`, y un `selftest` del reporteador. |
| `dialog-coverage.csv` | La tabla de cobertura HOST/QEMU de los **22 diálogos** que el arnés de host declara inertes. |
| `../check_dialog_coverage.py` | El gate que mantiene esa tabla pegada al código (mocks, fuentes reales y `app_names[]`). |

## Cómo se corre

```bash
tests/qemu/run_apps_qemu.sh --check          # preflight: dice exactamente qué falta, no toca nada
RONDAS=3 DWELL=2 tests/qemu/run_apps_qemu.sh # el contrato completo
python3 tests/qemu/barrido_qemu.py selftest  # casos del reporteador (capa SELFTEST, no cobertura)
python3 tests/check_dialog_coverage.py       # la tabla contra el código
```

Sin el entorno privado el runner sale **77 (SKIP)** con la lista de requisitos faltantes. Un SKIP
no es un PASS: el barrido no corrió y así queda dicho.

## Aislamiento (regla de laboratorio de la épica)

El runner **nunca** usa ni comparte `banco-apps`, `banco-audio`, `x6200-build-output` ni
rootfs/volúmenes/build dirs de otras corridas. Se lo pasa por variables y se verifica antes de
arrancar (`aislamiento()`):

| Variable | Default | Qué es |
| --- | --- | --- |
| `LAB` | `/tmp/x6200-next222-qemu` | laboratorio propio (chroot, capturas, logs). No puede caer dentro de un worktree de otra corrida. |
| `CONT` | `x6200-next222-qemu` | contenedor propio (tiene que empezar con `x6200-` y no ser uno compartido). |
| `VOL` | `x6200-next222-build` | volumen de build propio (nunca `x6200-build-output`). |
| `IMAGEN` | `x6200-buildroot` | la imagen de build del repo (la misma del CI/arnés). |
| `BUILDROOT` | — | clon privado de Buildroot, solo si hay que construir (`CONSTRUIR=si`). |
| `ROOTFS` | — | `rootfs.tar` privada, si no se quiere armar el chroot desde `$VOL/target`. |
| `RONDAS` / `DWELL` | `3` / `2` | rondas del barrido y permanencia por app, en segundos. |

Los scripts del banco del owner tienen el contenedor y el volumen **fijos**
(`docker exec banco-apps`, `-v x6200-build-output:/b`), así que acá se ejecutan sus **primitivas
compatibles** en copia privada (compilar la GUI con `tools/compila-gui.sh` apuntado al volumen
propio, compilar `x6200-remote` con el toolchain del volumen propio, armar el chroot desde
`$VOL/target`). El guard `#86` (`tools/verifica-firmware-port.py`) corre sobre el binario copiado
antes de arrancar nada.

## Qué mide cada app

`QEMU_USER`: userland armv7 por `binfmt_misc`, kernel del host. No hay kernel/init real, no hay
radio, no hay RF y no se toca la SD del owner.

Por app y por ronda: `get` del estado inicial → `app <nombre>` → confirmación de que la app quedó
abierta → PNG real 800×480 → permanencia (`DWELL`) → `app off` → confirmación de la home. Y
estabilidad de PID, `boot_id`, `logrev`, `freq`/`mode`/`band` y de la cuenta de fds/hilos entre el
principio y el final.

Un paso de más de 5 s, un `CRASHLOG`, un `SIGSEGV`, una TX no solicitada, un estado persistido
cambiado sin querer o una fuga de fds/hilos son **FAIL**. Una app que necesita un periférico o
servicio ausente (GPS, WiFi, Bluetooth, red) es **PARTIAL**: se guarda la foto real del mensaje
controlado y **nunca** se declara PASS del dispositivo. Una corrida fallida **no** se descarta ni
se repite para obtener verde: las 3 rondas quedan en `barrido.json`.

## La tabla de cobertura

`dialog-coverage.csv` tiene una fila por diálogo inerte con: la fuente de producción que define el
descriptor real, el fixture de host que llega a su camino (si hay), la app del barrido que lo abre,
lo que el barrido **debería** producir (`REAL`/`PARTIAL`/`UNSUPPORTED`) y lo **medido** con la
captura o la razón exacta. `tests/check_dialog_coverage.py` la valida:

- el set de símbolos tiene que ser exactamente el de `= &mock_dialog;` en
  `tools/ui_harness/mocks/mock_main_screen.c`;
- cada `source` tiene que definir de verdad el descriptor real;
- `host_status` es `INERT` en las 22 (el harness de host sigue siendo sintético: no se promueve a
  REAL);
- `qemu_app` tiene que estar en `app_names[]`, y las **25** apps del barrido tienen que estar
  cubiertas por una fila o por una excepción verificada (Settings/BT/SWRScan son diálogos reales;
  RTTY y DFLEditor no son diálogos; FT4 es alias de FT8);
- un resultado `REAL`/`PARTIAL` exige captura y **no** admite razón de bloqueo; un
  `BLOCKED`/`UNSUPPORTED` exige la razón exacta y **no** admite captura.
