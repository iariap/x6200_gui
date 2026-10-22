# Evidencia — issue #214: supervisor GUI con backoff y logs únicos (epic #211, hijo 3/4)

- **Fecha:** 2026-10-22
- **App:** `iariap/x6200_gui` — rama de trabajo `factory/hardening-214`
- **Base medida:** `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845` (= tip de `origin/lu2dsx/x6200-rom` al iniciar el worktree)
- **Contrato:** [#214](https://github.com/iariap/x6200_gui/issues/214); épica [#211](https://github.com/iariap/x6200_gui/issues/211); origen del hallazgo F-04/F-05 del [assessment #194](https://github.com/iariap/x6200_gui/blob/assessment/rom-194/docs/ASSESSMENT_ROM_194.md)
- **Alcance:** sólo GUI (un commit). Sin push, sin merge, sin radio, sin SD del owner, sin TX/RF.

> **Nota de base:** al iniciar el worktree, `origin/lu2dsx/x6200-rom` ya había avanzado
> a `35bad790` (un commit **sólo de docs**: `docs/evidencia/2026-10-22-hardening212/README.md`,
> la evidencia del hijo #212). El diff `d4fb57f8..35bad790` no toca `src/`, `rootfs/` ni
> `tests/`, así que no hay conflicto con este hijo; la base de trabajo es la verizada
> `d4fb57f8` que fijó el orquestador.

## 1. Qué se cambió (y por qué)

| Archivo | Cambio |
|---|---|
| `rootfs/usr/sbin/x6200_daemon` | Reescrito (17 → ~265 líneas) como supervisor robusto: backoff determinista, logs únicos, señales y una sola instancia. Se **preservó** la preparación de `/dev/shm/images` (bind mount con fallback a copia) y la ejecución de la GUI real `/usr/sbin/x6200_gui` con su salida a `/tmp/x6200_log.txt`. Sólo el script; **no** se tocó `src/`, `S20gui`, `S01create_data`, el watchdog de kernel, init/services ni DSP. |
| `tests/daemon/run_daemon_tests.sh` | Suite hermética nueva: corre el **wrapper real** en un chroot privado (`unshare -Ur -m -p`) con GUI sustituta y dobles de `sleep`/`date`/`mountpoint`/`mount`, sin tocar el `/mnt` del host ni la radio. |
| `tests/run_all_tests.sh` | Stage `daemon` (aditivo) para que la suite entre al gate global; 77 = SKIP si no hay user namespaces. |
| `.gitignore` | Ignora `tests/daemon/build/` (salida de la suite), como ya se hace con `tests/native/build/`. |

El defecto original (evidencia #196): `while true; do GUI || sync; mv .../x6200_log_$(date +%Y-%m-%d_%H-%M-%S).txt; done`.
Sin backoff (34 reinicios en 10 s con DB corrupta), logs que colisionan por nombre con precisión de segundos y `sync` global en el lazo de fallos.

## 2. Método de medición

- **HOST hermético (real, corrido):** `tests/daemon/run_daemon_tests.sh`.
  - Chroot privado en `unshare -Ur -m -p --fork` (namespace de usuario/montaje/PID propio;
    al salir, el kernel limpia todo el namespace: sin residuos).
  - `/bin/sh` y libs del **host (bash)** como intérprete. La ROM del target configura
    `BR2_SYSTEM_BIN_SH_BASH=y` (verificado en `br2_external/configs/X6200_defconfig`), así que
    el shell del host es el mismo shell del target; además el script parsea bajo `dash` (POSIX
    estricto, el `/bin/sh` de la imagen de build).
  - GUI sustituta con guion por corrida (`exit:N`, `live:N`); dobles de `sleep`, `date`,
    `mountpoint`, `mount` y `killall` por PATH. **El wrapper no se edita.**
  - Reloj virtual por el doble de `date` (fallback del wrapper cuando no hay `/proc/uptime`);
    un `sleep` real de 5 min no se usa (sólo la ventana real de 10 s del caso `instant10`).
- **QEMU_USER con rootfs privada:** ver §7 — **PENDING** con la razón exacta (no se pudo
  armar un rootfs ARM privado sin una compilación Buildroot completa; serialización de
  recursos de la épica).

## 3. Matriz de casos — HOST hermético (wrapper real)

Comando: `sh tests/daemon/run_daemon_tests.sh` — **exit 0, 15/15 casos** (matriz cruda en
[`host/daemon-suite/matrix.txt`](host/daemon-suite/matrix.txt)).

Todos los casos son **condición → comando → efecto observado** (la salida cruda del caso está en
la matriz):

| caso | condición | efecto observado (assert) |
|---|---|---|
| `backoff` | GUI `exit:1` siempre; DATA no montada; `sleep` doble instantáneo | delays `2 5 15 60 60`; `GUI exited: code 1`; `GUI restart in 2s`; reloj por `date +%s` |
| `exit78` | GUI `exit:78` (error de configuración SQLite) | delay `60 60 60` **desde el primer fallo**; informa el 78 |
| `uniquelogs` | 2 salidas en el **mismo segundo** (`date` fijo); `/mnt` montada rw | 2 logs en `/mnt/app_logs`, nombres únicos `..._p14_r1.txt` / `..._p14_r2.txt` |
| `single` | 2º arranque del wrapper con el 1º vivo | 2º sale 0, avisa `another instance is alive`, **no** levanta otra GUI |
| `mntabsent` | `/mnt` **sin montar** | **0 archivos** bajo `/mnt`; log retenido en `/tmp`; diagnóstico en stderr |
| `mntro` | `/mnt` montada **read-only** (bind ro real) | **0 archivos** bajo `/mnt`; log retenido en `/tmp` |
| `sigtermgui` | SIGTERM al wrapper con la GUI viva | sale en 3 ms, **no** relanza, no anuncia reinicio |
| `sigtermbackoff` | SIGTERM al wrapper durante el backoff (sleep real de 2 s) | corta la espera en 3 ms (trap sobre `wait`), no relanza |
| `longrun` | corrida "sana" de ≥300 s (reloj virtual, `date` avanza 400 s/llamada) | contador reseteado: delays `2 2 2` (sin reset serían `2 5 15`) |
| `procuptime` | `/proc/uptime` presente (valor estático) | usa `/proc/uptime` (no cae al `date +%s`); backoff `2 5` normal |
| `imagesexisting` | `/dev/shm/images` ya es un montaje | no monta ni copia (no rompe el bind existente) |
| `imagesbind` | `/dev/shm/images` sin montar, bind OK | hace `mount --bind /usr/share/x6200/images /dev/shm/images` |
| `imagescopy` | bind no soportado (`mount` falla) | fallback: copia los assets a `/dev/shm/images` |
| `s20gui` | `S20gui start` / `S20gui stop` con el `start-stop-daemon` doble y el daemon real | start → `OK` + PIDFILE + 1 GUI; stop → `OK` + PIDFILE limpio |
| `instant10` | falla instantánea, **10 s reales** | **3** invocaciones (≤3); delays `2 5` |

Cobertura del contrato (sección "Casos"): crash instantáneo 10 s ⇒ ≤3 invocaciones (`instant10`),
exit78 ⇒ 60 s (`exit78`), PID/único GUI (`single`), 2 salidas en el mismo segundo ⇒ logs únicos
(`uniquelogs`), `/mnt` ausente/ro ⇒ 0 archivos sobre rootfs (`mntabsent`, `mntro`), SIGTERM ⇒ hijo
interrumpido y sin relanzamiento (`sigtermgui`, `sigtermbackoff`), 6 min sano ⇒ contador reseteado
(`longrun`), bind mount existente no roto (`imagesexisting`), retorno de S20gui normal (`s20gui`).

## 4. Control negativo (el test no es vacío)

Con el wrapper **pre-fix** (`host/x6200_daemon.prefix.sh`, el script de 17 líneas de `d4fb57f8`):

```
X6200_CASES="backoff mntabsent instant10" X6200_DAEMON=host/x6200_daemon.prefix.sh \
    sh tests/daemon/run_daemon_tests.sh <out>
```

Resultado real: **3 casos, 3 fallos** (matriz en
[`host/daemon-suite-prefix/matrix.txt`](host/daemon-suite-prefix/matrix.txt), exit 1):

- `instant10`: **881 invocaciones en 10 s** (>3) — el defecto del assessment (#196: 34 reinicios).
- `mntabsent`: **escribió** `/mnt/app_logs/x6200_log_....txt` sobre el rootfs con DATA no montada.
- `backoff`: sin delays (no hay backoff), sin mensajes de estado.

## 5. Gates

Corrido en el worktree `state/worktrees/x6200/hardening-214` (rama `factory/hardening-214`),
base `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845`.

| gate | comando | resultado |
|---|---|---|
| Suite hermética del wrapper | `sh tests/daemon/run_daemon_tests.sh` | **exit 0**, 15/15 ([`host/daemon-suite/matrix.txt`](host/daemon-suite/matrix.txt)) |
| Control negativo | (arriba) | **exit 1**, 3/3 fallos ([matriz](host/daemon-suite-prefix/matrix.txt)) |
| Suite nativa | `sh tests/native/run_native_tests.sh` | **exit 0**; `138 comprobaciones, 0 fallos` ([`gates/native-tests.log`](gates/native-tests.log)) |
| Paridad host↔CI | `sh tests/native/run_native_tests.sh --ci-parity` | **exit 0**; `ci-parity: OK (build host sin lvgl/)` ([log](gates/native-tests-ci-parity.log)) |
| Suite completa (rápida) | `sh tests/run_all_tests.sh --quick` | **exit 0**; `18 OK, 0 fallos` — incluye el stage `daemon: OK all OK: 15 casos, 0 fallos`; SKIP: `wspr`/`sstv`/`wefax` (headers de codec ausentes en la imagen `x6200-uiharness`), `js8` (por `--quick`), `webserver` (sin checkout/volumen) ([log](gates/run_all_tests-quick.log)) |
| Auditoría de pruebas | `python3 tests/audita-pruebas.py` | **exit 0**, `OK: estructura y evidencia en orden` ([log](gates/audita-pruebas.log)) |
| Sintaxis / sello | `bash -n`, `dash -n` (image), `tools/fuente-md5.sh .` | `bash -n OK`, `dash -n OK`, sello `18f5458db571b3056efdfa54c0013a05` ([log](gates/shell-syntax-fuente.log)) |
| Matriz de la app | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | **exit 0**; `themes`/`contracts` ok, `bash -n` de los shell tocados ok, SKIP por trigger (ver §6) |

## 6. Matriz de la app (`check-matrix.sh`)

`scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` — **exit 0** (salida
cruda en [`gates/check-matrix.txt`](gates/check-matrix.txt)):

```
==> [SKIP] firmware / nativo: sin cambios en src/**, lv_conf.h, lv_drv_conf.h ni CMakeLists.txt
==> [SKIP] check_build_identity: sin cambios en la identidad del build
==> [SKIP] check_wcag (temas de la imagen): sin cambios en rootfs/**/themes/*.json
==> [SKIP] ruff del framework A/B: sin cambios en tools/ab_testing/**/*.py
==> [SKIP] pytest del framework A/B: sin cambios en tools/ab_testing/**
==> [check] x6200-ab themes
    ok
==> [check] x6200-ab contracts (7cbd210646d37a9706337b2f385fd078a189a50c)
    ok
==> [check] bash -n tests/daemon/run_daemon_tests.sh
    ok
==> [check] bash -n tests/run_all_tests.sh
    ok
==> [SKIP] harness determinism gate: sin cambios en tools/ui_harness/
==> [SKIP] banco QEMU: sin cambios en src/**, lv_conf.h ni tools/ui_harness/
```

`x6200-ab contracts` evalúa el **head sha del commit** y da
`PASS — 2056 checks, 0 failure(s), 0 unchanged baseline defect(s)` (corrida directa:
`uv run x6200-ab contracts --revision <head> --repo <worktree>`). El sha impreso en el log es el
del **momento de captura**; el diff evaluado es el mismo del commit de este cambio (el sha exacto
del cierre lo reporta el orquestador).

Los SKIP son del **trigger** del gate (el CI espeja por path): el diff sólo toca
`rootfs/usr/sbin/x6200_daemon`, `tests/**`, `docs/**` y `.gitignore`, así que los bloques de `src/`,
temas de la imagen y `tools/ui_harness/` no aplican.

**Nota de una corrida BLOCKED transitoria:** el primer intento del gate marcó
`x6200-ab contracts` **BLOCKED (exit 2)** con `git worktree add failed … fatal: Could not reset index
file to revision 'HEAD'`, un fallo del *tooling* del gate (probable carrera con las otras corridas
en vuelo sobre el mismo object store). Al reintentar, el gate **pasó**; la corrida registrada arriba
es la verde. No es PASS a medias: se reintentó y el resultado real es `ok`.

## 7. Pendientes y SKIP/BLOCKED declarados

- **QEMU_USER con rootfs privada — PENDING.** El contrato pide correr `x6200_daemon` bajo
  QEMU_USER con la GUI ARM real y DB corrupta. Eso necesita un **rootfs ARM privado** (shell +
  GUI del target) que **no existe** en esta corrida:
  - el worktree no trae rootfs ni binario ARM, y el `x6200-buildroot` (imagen compartida) **no**
    trae `output/` construido (toolchain/target vacíos);
  - el cross-compiler del host (`arm-linux-gnu-gcc`) **no tiene libc de ARM**
    (`crt1.o`/`libc.a`/`-lc` ausentes: no se puede enlazar un shell ARM);
  - los volúmenes/rootfs de los otros ejecutores (`h213-x6200-out`, `x6200-build-output`,
    `.vfix3`) son **ajenos** y la épica prohíbe compartirlos.
  Armar el rootfs exige una **compilación Buildroot completa** (toolchain + target). No se lanzó
  en paralelo para respetar la **serialización de jobs pesados** de la épica (`#211`: "si no hay
  recursos suficientes, serializar jobs QEMU/build") con **tres corridas más en vuelo** y el disco
  del host al 92 % (81 GB libres). Comando de reproducción para el orquestador (volumen propio):
  ```sh
  docker volume create h214-x6200-out
  docker run --rm --entrypoint "" -v <AetherX6200Buildroot>:/workspace \
      -v h214-x6200-out:/workspace/build x6200-buildroot \
      sh -c 'make -C buildroot BR2_EXTERNAL=../br2_external O=../build X6200_defconfig && make -C buildroot O=../build'
  ```
  Con el `target/` resultante: chroot privado + `qemu-arm-static` (binfmt) + `/mnt` con una DB
  corrupta y la GUI real; medir que no hay crash-loop rápido (≤3 arranques/10 s) y que el log se
  conserva. **No es PASS**: queda PENDING.
- `PENDING_HARDWARE`: la prueba real en la radio (reinicio físico, WDT de kernel, SD/DATA) queda
  del pipeline de QA remoto del owner (fuera de este hijo).
- **Observación para el orquestador (integración con #213):** el exit 78 del hijo SQLite
  (`Persistence unavailable…`) es exactamente el caso que este wrapper trata con backoff máximo
  (60 s) desde el primer fallo; al integrar, el escenario "DB corrupta ⇒ crash-loop" del
  assessment debe mostrar **1** arranque cada ≥60 s (no 34 en 10 s).
- **Cambio de comportamiento esperado (blast radius del exit 0):** la GUI sale con
  `exit(0)` a propósito al cambiar el tema (`src/dialog_settings.c`: "el x6200_daemon la
  vuelve a lanzar") y en `src/main_screen.c`. Con el contrato nuevo, ese `exit 0` se
  supervisa con backoff (no spin): el **primer** reinicio espera **2 s** (antes era
  inmediato). El contador se resetea en cada corrida ≥300 s, así que un cambio de tema
  suelto cuesta 2 s; sólo una ráfaga de salidas 0 rápidas escala a 5/15/60 s — que es
  justamente lo pedido ("no spin"). No se tocó la GUI para esto.

## 8. SHAs y sello
| qué | valor |
|---|---|
| HEAD de `factory/hardening-214` | el commit de este cambio (ver el reporte de cierre; la rama no se pushea desde acá) |
| base | `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845` |
| `origin/lu2dsx/x6200-rom` al iniciar | `35bad790` (diff sólo docs) |
| sello de fuentes (`tools/fuente-md5.sh .`) | `18f5458db571b3056efdfa54c0013a05` |

El sello es de las **fuentes de la GUI** (`src/` + `lvgl/src`): este cambio no toca `src/`, así que
el sello es el del árbol base; no hay binario nuevo que medir (no se recompiló la GUI).
