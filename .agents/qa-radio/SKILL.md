---
name: qa-radio
description: Monitorear la rama del port del X6200, compilar y desplegar en la radio por WiFi, probarla (barrido de apps, vigilancia de crashes), hacer el triage de lo que falla, cargar el issue en GitHub y dejar la evidencia y el registro de QA. Use when the user says "monitorea main", "monitorea la rama", "desplega en la radio", "probala", "QA", "triage", "carga un issue", "evidencia", "registro de pruebas" or asks to build+deploy+test the firmware and report what fails.
---

# QA en la radio: monitoreo, pruebas, triage, issue y evidencia

El ciclo completo: **rama nueva → build limpio → deploy por WiFi → pruebas → triage → issue → registro**.
Nace de la rutina que pidio el dueño el 2026-10-15 y de los errores que ya se cometieron haciendola
(estan todos en la ultima seccion: leerla antes de empezar).

## El contrato (reglas del dueño, no negociables)

1. Cada commit nuevo a la rama del port se compila, se despliega y **se prueba**.
2. Si algo **no funciona**: se carga un **issue** con todo el detalle y el **triage**. **NO se arregla**:
   el issue es el entregable.
3. **Antes de crear un issue, buscar si ya existe** y comentarlo en vez de duplicar.
4. Vale la regla del proyecto: *todos los hallazgos deben estar fundados, si no se pueden fundar no debe
   ser un hallazgo* (ver el skill `hallazgos-fundados`).
5. La radio y la estacion son del dueño: no se transmite sin su autorizacion y se deja todo como estaba.
6. **TODAS LAS PRUEBAS SE HACEN EN LA RADIO** (regla del owner, 15/10 18:30). No se prueba en el
   banco ni en QEMU: se compila, **se despliega a la radio** y se prueba ahi. O sea: cada build termina
   en un deploy. Y lo que se prueba es **lo ultimo mergeado**: el tip de la rama de integracion
   (`lu2dsx/x6200-rom`), que es donde entran los PRs.
   **Ojo con `main`**: `fork/main` es la linea ORIGINAL del X6100 (hoy `e1476ec`), NO tiene los archivos
   del port y **desplegarla deja la radio sin GUI**: medido el 15/10 18:18, el binario que produce el
   build de `main` no arranca en el X6200 (`error while loading shared libraries: libRHVoice.so.5`).
   Si el owner pide probar `main`, avisarle esto ANTES de desplegar.
   **Antes de cada deploy, guardar el par que funciona** (`.rollback/x6200_gui-port` y
   `.rollback/liblvgl.so-port`): si el deploy deja la GUI muerta, restaurar los dos archivos y
   `kill -9 $(pidof x6200_gui)` para que el daemon relance.
7. **SOLO SE MONITOREA Y SE REPORTA** (regla del dueño, 15/10 17:30, en mayusculas). No se arregla el
   firmware (`src/`, `rootfs/`), **no se tocan los ajustes de la radio** (params, temas, archivos de
   `DATA`) y **no se ofrecen arreglos**: el entregable es el reporte y, si algo falla, el issue. Lo unico
   que este rol escribe son los documentos del QA: el reporte al dueño, `docs/PRUEBAS.md`, la evidencia
   en `docs/evidencia/` y este skill. Si un fix aparece en la rama, se **valida y se reporta**, no se
   escribe.

## Paso 0 — Donde trabajar (el arbol es compartido)

Otro agente trabaja en el **mismo checkout** y a veces lo deja en un **HEAD detached**. Nunca trabajar
ahi ni hacerle checkout: se construye desde **un worktree propio**.

```bash
cd /home/pablo/desa/x6200/x6200_gui            # solo para administrar git
git worktree add /home/pablo/desa/x6200/.wt-pruebas lu2dsx/x6200-rom   # si no existe
cd /home/pablo/desa/x6200/.wt-pruebas
```

Antes de construir, verificar **que commit se va a construir**: `tools/compila-gui.sh` lo imprime al
final. Si dice uno viejo, algo quedo mal (el arbol compartido estaba en otro lado).

## Paso 1 — Monitoreo

```bash
cd /home/pablo/desa/x6200/.wt-pruebas && git fetch fork
git log --oneline HEAD..fork/lu2dsx/x6200-rom      # ¿hay algo nuevo?
```

- La rama que se construye y despliega es **`lu2dsx/x6200-rom`** (el port).
- **`fork/main` es la linea ORIGINAL** del proyecto (cientos de commits atras): **nunca** construirla ni
  desplegarla, revertiria el port. `origin/main` es lo mismo.
- Si no hay nada nuevo: **no recompilar ni re-desplegar**. Solo `tools/verifica-radio.sh` y un resumen.
- **"Nada nuevo" se mide contra la radio, no solo contra git.** Comparar el md5 desplegado con el build
  del tip. Ya paso (15/10 16:10): el git decia "0 commits nuevos" porque mi HEAD ya era el tip, pero la
  radio tenia un build viejo y habia un commit de fuentes (`4e9b5f4`, #77) sin desplegar.
- **Antes de compilar, mirar el volumen compartido**: su sello `.fuente.md5` y la hora del binario dicen
  si el otro agente acaba de compilar su A/B. Si el sello no es el hash de nuestros fuentes
  (`RECONSTRUIR=auto tools/compila-gui.sh` lo calcula), el volumen es suyo: **no compilar**, dejar el
  despliegue pendiente y avisar.
- **Y mirar `docker ps`**: si hay contenedores `x6200-buildroot` corriendo, hay un build en curso **ahora**
  y el volumen esta en uso. La hora del binario NO alcanza para saberlo (refleja el ultimo build
  terminado, no el que esta corriendo): el 15/10 16:39 el binario tenia 35 minutos pero habia tres
  builds activos.
- Si hay que compilar igual, **respaldar antes** el binario que este en el volumen
  (`cp /b/target/usr/sbin/x6200_gui /b/ab-backup-<md5>`): asi el A/B del otro agente no se pierde.
- Si el volumen esta ocupado **varias corridas seguidas** (paso el 15/10: tres ciclos sin poder compilar),
  las salidas son: esperar una ventana, pedirle al owner un volumen propio (`x6200-build-output-ab`), o
  **clonar el volumen** (`docker volume create` + `cp -a` adentro) y compilar sobre la copia, que no
  toca el build del otro. El aviso al owner va igual: el ciclo "cada commit -> build + deploy + prueba"
  se atrasa aunque la rama avance.
- El trabajo en curso del otro agente aparece en ramas `factory/*`: no se mergean.

## Paso 2 — Build (limpio, siempre)

```bash
tools/compila-gui.sh              # borra objetos de la GUI, compila y deja sello del hash de fuentes
```

Por que limpio: el build incremental **se queda con un binario viejo** despues de cambios en headers.
Medido: incremental dio `30bef0dbec2a` y limpio `5a6e561e502f`, con el mismo fuente; la radio mostraba
el color viejo. `RECONSTRUIR=auto` saltea la compilacion solo si el hash de fuentes no cambio.

El volumen `x6200-build-output` es **compartido** con el otro agente: si esta haciendo A/B, avisarle
antes de compilar (un build limpio le pisa el binario).

## Paso 3 — Deploy por WiFi

```bash
tools/prueba-gui-en-radio.sh      # copia el binario y las liblv*.so*
```

- **El gate fail-closed del issue #86 corre solo**: antes de subir nada, `prueba-gui-en-radio.sh`
  valida el binario con `tools/verifica-firmware-port.py` (ELF32 ARM + tamano >= 8.000.000 B +
  marcas del port). Un build desde el `main` del remoto sale con **exit 0** y deja un `x6200_gui`
  de **776.884 B** que **no** es el firmware (el del port: **13.737.784 B**): el gate **aborta el
  deploy** en vez de dejar la radio sin GUI. Si salta, el binario del volumen no es el del port:
  no re-desplegar a mano, reconstruir desde la rama del port.
- La IP **cambia** (se vio en `192.168.18.67` y `192.168.0.236`): no fijarla, la busca
  `tools/lib-radio.sh` (`radio_ip()`: `RADIO_IP`, direcciones conocidas y, si no, barrido de la red
  verificando que sea la radio con el login y `/usr/sbin/x6200_gui`).
- Verificar que la radio quedo con el md5 que se compilo.

## Paso 4 — Pruebas

| Que | Como | Esperado |
|---|---|---|
| Estado general | `tools/verifica-radio.sh` | `TODO OK` (assets, dispositivos de entrada, sink de PulseAudio, sin crashes, 0 errores de archivo) |
| Barrido de apps | `PERMANENCIA=14 tools/prueba-apps-radio.sh` | `0` caidas; informa 22 de 23 (el SWR Scan no se puede abrir por remoto, issue #78) |
| Vigilancia de crashes | contar el PID y los logs por 180 s | PID estable, 0 logs nuevos |
| Crash que mató la corrida anterior | `grep -ac 'CRASHLOG sig'` en los **app_logs nuevos** de `/mnt/app_logs`, no sólo en el log vivo | `verifica-radio` mira `/tmp/x6200_log.txt`: un crash que termina la corrida queda en el log guardado y el chequeo igual dice TODO OK (pasó el 15/10 17:42) |
| Captura de pantalla | `tools/captura-radio.sh salida.png [x1 y1 x2 y2]` | PNG con colores reales (no negro) |
| Sin radio (banco) | **no se usa**: el owner pidio que todas las pruebas sean en la radio (15/10) | — |

- La GUI la relanza `x6200_daemon` en ~1 s: "se reinicio la radio" casi siempre es la **GUI** reiniciandose.
  El uptime del equipo sigue subiendo si no hubo reinicio de verdad.
- Los params de CW (`cw_tune`, `cw_decoder`, `key_train`) se ponen en 0 para dejar la radio sana.

### Transmision (TX)

- **Solo si el dueño lo autorizo**, **solo en la banda de la antena** (hoy 40 m) y en **QRP**, con
  `TX power` al minimo. Antes de cualquier prueba que emita, verificar la banda por el control remoto.
- El barrido de apps **abre** WSPR/FT8/beacon: son los que pueden emitir solos.
- Abrir la app del barrido de ROE **no** emite: el `Run` es lo que llama a `radio_start_swrscan()`
  (`src/radio.c:667-690`: solo desde RX, fuerza LSB, pone 3.0, y **restaura** la potencia al salir).
- Hoy el barrido de ROE **no se puede disparar por socket** (issue #78): anotar la TX como *no medida*,
  no forzarla.

## Paso 5 — Triage (lo que hace util al issue)

**Primero la firma, despues la pila.** El handler de crashes deja en el log:

```
CRASHLOG sig=NN fallo=0x... pc=0x... lr=0x... sp=0x... base=0x...
CRASHLOG-STACK <160 palabras>
```

- `sig=6` + `*** buffer overflow detected ***` = familia de **buffers** (fortify aborta en libc).
- `sig=11` + `fallo=(nil)` = **NULL deref**: otra familia, no confundirlas.
- `base` es `&main` en runtime: para resolver, `enlace = enlace(main) + (palabra - base)`
  (`nm | grep ' main$'` da el enlace de `main`; en este binario `main` esta en `0x4c0000`).

```bash
arm-buildroot-linux-gnueabihf-addr2line -f -C -e x6200_gui <direccion>
```

Reglas del triage:

- Decir **que se verifico y que se descarto**, con el comando. Un triage sin descartes no sirve.
- Un grep sobre un **binario** sin `-a` no vale (ya paso: se afirmo que un helper "no tenia la cadena" y
  era falso).
- Una sola muestra no es una conclusion. Si el crash es cada ~150 s, hay que medir ventanas completas.
- `DATA` (`/mnt`) **sobrevive a las actualizaciones**: una copia vieja puede ganarle a lo que shipea el
  firmware (paso con `/mnt/themes/RS/theme.json` y el color del medidor de RS, issue #44).
- Los logs de la GUI viven en `/mnt/app_logs/x6200_log_*.txt` y se rotan: si no queda el rastro, decirlo.

## Paso 6 — Issue

```bash
gh issue list --repo iariap/x6200_gui --state all --search "palabras clave"   # SIEMPRE primero
gh issue create --repo iariap/x6200_gui --title "..." --body-file cuerpo.md
```

Cuerpo (en ese orden): **sintoma** (que se ve y quien lo reporta) → **medido** (los comandos y su salida
real, con el md5 del build y el commit) → **triage** (la tabla de lo verificado y lo descartado) →
**impacto** (que pruebas quedan bloqueadas) → **entorno** (rama, commit, md5, kernel, sin transmision) →
**no lo resuelvo** (si el dueño lo pidio asi).

Si ya existe un issue del mismo defecto: **comentarlo** con la medicion nueva. Ya paso al reves: se
cargo el #76 y era duplicado del #64, hubo que cerrarlo.

## Paso 7 — Evidencia y registro

- Evidencia cruda en `docs/evidencia/<fecha>-<tema>/`: capturas PNG, logs, salidas de comandos, `md5`.
- Registro en **`docs/PRUEBAS.md`**: una fila en la tabla de resumen y una seccion por corrida, con los
  comandos y sus salidas pegadas. Estados: **medido / codigo / no medido**; lo no fundado va al anexo B.
- Correr la auditoria: `python3 tests/audita-pruebas.py` (verifica la estructura y que cada archivo
  citado exista). Forma parte de `tests/run_all_tests.sh`.
- Commitear **solo por ruta** (`git add <archivo>`, nunca `git add -A`: el arbol es compartido).

## Paso 8 — Dejar todo como estaba

El **tema que tenga puesto el dueño** (no forzar ninguno: el 15/10 a la tarde tenia `IU1WXY`, antes
`VISION`), `cw_tune=cw_decoder=key_train=0`, la radio en RX y los params que se
hayan tocado (por ejemplo `swrscan_span`) anotados en el reporte. Avisar: md5 desplegado, apps ok,
caidas, TX/ROE si se midio, e issues abiertos.

## Errores ya cometidos (no repetirlos)

| Error | Como se vio | Regla |
|---|---|---|
| Build incremental stale | dos md5 distintos con el mismo fuente; la radio mostraba el color viejo | build limpio siempre (`tools/compila-gui.sh`) |
| IP fija en las herramientas | la radio cambio de red y las tools fallaban | `tools/lib-radio.sh`; nunca fijar la IP |
| Creer que "se reinicio la radio" | era la GUI relanzada por el daemon | comparar el uptime del equipo, no el PID |
| Issue duplicado (#76 vs #64) | el buscador no se corrio antes | `gh issue search` **antes** de crear |
| Afirmar sobre un binario sin `-a` | el helper "no tenia la cadena" era falso | greps sobre binarios, con `-a` |
| Concluir con una muestra | 1 caida en 150 s parecia "sigue roto" | medir ventanas completas y mirar la firma |
| Escribir en el arbol compartido | estaba en checkout detached del otro agente | worktree propio, commit por ruta |
| Build pisando el A/B del otro | volumen de build compartido | avisar antes de compilar |
| Editar un archivo recien cambiado | `FS_STALE_VERSION` / archivo "desaparecido" | releer antes de editar; verificar el commit del arbol |
| Dar por muerto el fix del #44 | el JSON nuevo estaba en `/usr/share`, la GUI leia `/mnt` | medir en la radio, mirar `DATA` |
| **Build desde un `git worktree` sin el submódulo `lvgl`** | el CMake falla (`lvgl does not contain a CMakeLists.txt`) o sale un binario **PIE de 776 KB** en vez del de 13.737.784 B | `git submodule update --init lvgl` en el worktree; y **verificar el tamaño/md5 del binario despues de cada build** (el de la GUI es 13.737.784 B; si pesa cientos de KB, algo falta) |
| **Build desde el `main` del remoto sale verde y deja un binario que no es el firmware** (issue #86) | `make x6200-gui-rebuild` desde `fork/main` (e1476ec) da **exit 0** y deja un `x6200_gui` de **776.884 B** (PIE) que no arranca en la radio (el del port: **13.737.784 B**) | el gate `tools/verifica-firmware-port.py` corre solo en `prueba-gui-en-radio.sh` y en el banco y **aborta** si el binario no es el del port; nunca construir desde `main` (Paso 1) |
| Mi `compila-gui.sh` escondia el error | mandaba la salida a /dev/null y decia "FALLO la compilacion" sin el motivo | al fallar un build, correr el `make` a mano con el log a un archivo |
| **El deploy por WiFi no prueba el rootfs** | el flicker venia de `/etc/init.d/S95gui`, que `prueba-gui-en-radio.sh` no toca (copia binario y libs) | si el cambio es del rootfs o de los scripts de init, hay que **flashear la imagen** (ver la receta de SD). Y si la GUI se ve rara, revisar `ps`: dos `x6200_gui` con `/dev/fb0` = dos GUIs dibujando |
| Traer cuadros del framebuffer con `cat` por SSH | los archivos llegaban cortados (786 KB de 1.5 MB) | usar `scp` o leer hasta completar el tamano |
| El push rebota | el otro agente empujo a la misma rama mientras yo trabajaba (15/10 17:20) | `git pull --rebase fork lu2dsx/x6200-rom` y volver a pushear; los commits propios quedan encima de los de el |

## Imagen de SD: armado, flasheo y conservar la DATA

Cuando el owner pide la ROM en la SD (no alcanza el deploy por WiFi):

1. **Build de la imagen** (con el arbol del port y el submodulo `lvgl` poblado): `make -j$(nproc)` en
   el buildroot con `-v .wt-pruebas:/workspace_gui:ro`. Tarda minutos si los paquetes ya estan.
   Verificar: `tools/verifica-firmware-port.py --tar /b/images/rootfs.tar` (ELF32 ARM + tamano +
   marcas del port, el gate del issue #86) — el binario adentro tiene que pesar **13.248.936 B**
   (stripped) y no los ~600 KB del binario roto.
2. **Identificar la SD con evidencia**: `lsblk` → tiene que ser `removable=1`, del tamano de la
   tarjeta, y con el **layout del X6200** (`p1` 31-32 MB vfat `BOOT`, `p2` 512 MB ext4 `rootfs`,
   `p3` 3.2 GB vfat `DATA`). Nunca escribir en `nvme*`.
3. **Flashear**: `dd if=sdcard.img of=/dev/sdX bs=4M conv=fsync` y **verificar leyendo de vuelta**:
   `dd if=/dev/sdX ... | head -c <bytes> | md5sum` tiene que dar el md5 de la imagen.
4. **La imagen trae 2 particiones** (MBR + p1 + p2, 545 MB): al flashear **desaparece la DATA**. Para
   conservar los ajustes, volver a declarar `p3` con `start` = tamano de la imagen en sectores
   (571473920/512 = **1116160**) y `size` = el resto de la tarjeta, **en el slot 3** (offset 478 del
   MBR; si queda en el slot 4 la radio la ve como `p4` y no la monta). El host puede bloquear la
   escritura al dispositivo: hacerlo desde un contenedor `--privileged -v /dev:/dev`.
5. **Verificar la DATA**: `blkid /dev/sda3` → `TYPE=vfat LABEL=DATA` + montarla en ro y ver
   `params.db` (106.496 B) y `themes/`.
