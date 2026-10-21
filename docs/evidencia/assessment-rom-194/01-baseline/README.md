# Unidad #195 — Identidad, inventario exhaustivo y baseline funcional

- **Campaña:** assessment pre-release ROM #194 (`iariap/x6200_gui`).
- **Unidad:** #195 (1/5), ejecutada el 2026-10-21 (UTC) por un agente ejecutor de evidencia.
- **Carpeta de evidencia:** `docs/evidencia/assessment-rom-194/01-baseline/`.
- **Rama del informe:** `assessment/195-evidence` → PR contra `assessment/rom-194`.
- **Alcance:** sólo investigación, ejecución de herramientas existentes, medición y documentación.
  No se modificó firmware, tests, CI, budgets, golden ni excepciones; no se desplegó, flasheó,
  transmitió ni tocó la radio.

## 1. Base congelada y divergencias observadas

| Elemento | Valor congelado / observado |
|---|---|
| GUI candidato (medido) | `c888308eede71205605c06383432ee7e165f202e` |
| Rama de desarrollo | `lu2dsx/x6200-rom` |
| Gitlink LVGL | `74d0a816a440eea53e030c4f1af842a94f7ce3d3` (v8.3.11) |
| Buildroot | `d084e2870a00b64d9c57474e7eceeef80f43f25b` (`iariap/AetherX6200Buildroot`, `lu2dsx/x6200-rom`) |
| Submódulo `buildroot` del repo Buildroot | `cc34b5b2abf2588f0ef55e46a4fe85a60c19ba1f` (2023.02.11) |
| Rama de integración publicada | `assessment/rom-194` = `c888308e` (creada y pusheada en esta unidad) |
| Worktree de reporte | `assessment-194/wt-assessment-195` (rama `assessment/195-evidence`) |
| Clon de medición limpio | `assessment-194/clone-gui` en `c888308e`, `git status --porcelain` vacío |
| Clon Buildroot privado | `assessment-194/clone-buildroot` en `d084e287` |

**Divergencias registradas (no se cambió de candidato):**

- Al comenzar (2026-10-21 ~18:20 UTC) el tip de `lu2dsx/x6200-rom` era `90aebad5`, que es
  `c888308e` + 3 commits **sólo de documentación/evidencia** (12 archivos, 59 inserciones, 8 PNG).
  Verificado con `git merge-base --is-ancestor` y `git diff --name-only`.
- Durante la ejecución (2026-10-21 18:32 UTC) se mergeó el PR #200 y el tip pasó a
  `50bf8ab0faef117a02d3a2d8c9b6bec4befafba6` ("Neo-retro animated text boot…"), que **sí cambia
  producción**: 22 archivos fuera de `docs/` (`src/bootanim/*`, `src/boot_splash.*`, `src/main.c`,
  `src/CMakeLists.txt`, `CMakeLists.txt`, `rootfs/etc/init.d/S01bootanim`, tests y
  `.github/workflows/boot-rootfs.yml`; +4654/−1 líneas).
- Consecuencia: **el candidato `c888308e` sigue siendo válido para este dossier**, que mide
  exactamente ese SHA. Para el tip nuevo `50bf8ab0` el resultado es **STALE**: la cobertura que
  debe repetirse es la que toca `src/`/`rootfs/`/tests de arranque (build ARM, identidad de
  artefactos, baseline host y barrido de apps). La corrida de CI `boot-rootfs` del push a
  `50bf8ab0` (run 39653588113) terminó en **failure** y pertenece a #200, no a este candidato.
- No se incorporó #192 (no forma parte de este candidato); #193 sí está integrado en `c888308e`.

## 2. Aislamiento y recursos

- Todo el trabajo vive bajo `assessment-194/`: `clone-gui`, `clone-buildroot`, `scratch/`,
  `build/` (del Buildroot), `wt-assessment-195`.
- No se usó el volumen compartido `x6200-build-output` (ni `x6200-build-cache`), ni
  `x6200_qa`, ni `buildroot_new`, ni la radio `192.168.0.236`, ni scripts de radio.
- `banco-apps` estaba **ocupado** al iniciar (contenedor `x6200-buildroot` corriendo, ajeno) y
  siguió ocupado durante toda la unidad: no se usó, no se detuvo. Esta unidad no necesita bancos.
- Build ARM/ROM en un contenedor privado `assessment195-build` (imagen
  `x6200-buildroot:latest`, `sha256:b3e7601313d64284645a5224c60fb40f39ea58e0bfc68f42fd9bb0dad13a1189`),
  corriendo como usuario 1000 y montando sólo `assessment-194` en su ruta absoluta.
- Contenedor de baseline con imagen `x6200-uiharness-full:latest`
  (`sha256:33fc1ed113e31978c600df82375628fe9d979d5c3408b4e16a42f43ae2f9312c`) para códecs y
  sanitizers; `x6200-uiharness`/`-ci` no se modificaron.
- Host: 16 núcleos, 89 GB RAM, 127 GB libres al iniciar; sin `ninja`, sin `fakeroot` de host y
  **sin runtime de sanitizers** para gcc 16.2.1 (se usó el toolchain de la imagen full como
  complemento, ver §5).

## 3. Paso 1 — Estado, refs, toolchain y CI

Logs: [`logs/step1/`](logs/step1/), JSON originales en [`capturas/ci/`](capturas/ci/).

| Comprobación | Resultado |
|---|---|
| `git status --porcelain` del clon de medición | vacío antes y después de la baseline |
| `git rev-parse HEAD` | `c888308eede71205605c06383432ee7e165f202e` |
| `git ls-tree HEAD lvgl` | `160000 commit 74d0a816a440eea53e030c4f1af842a94f7ce3d3 lvgl` |
| `git submodule status --recursive` | `74d0a816… lvgl (v8.3.11)`; `test -f lvgl/CMakeLists.txt` OK |
| Buildroot `HEAD` | `d084e287…`; submódulo `cc34b5b2…` (2023.02.11) |
| Toolchain host | git 2.55.0, gh 2.101.0, python 3.14.7, uv 0.9.18, cmake 4.3.0, gcc/g++ 16.2.1, make 4.4.1, docker 29.8.2, qemu-arm 10.2.2; `ninja`/`fakeroot`/`patchelf` ausentes |
| Docker | `x6200-buildroot:latest` b3e76013…, `x6200-uiharness{,-ci}:latest` 9eedee0e…, `x6200-uiharness-full:latest` 33fc1ed1…, `x6200-uiharness-dflmirror-ci:latest` 826eae2c… |
| Espacio | 127 GB libres al iniciar (`/home/pablo/desa/x6200`) |

**CI (manifiestos guardados, no nuevas corridas):**

- `runs-candidate-c888308e.json`: **0 corridas** para el commit de merge `c888308e`. El push a la
  rama sólo dispara `boot-rootfs` cuando cambian `buildroot/**`, `CMakeLists.txt` o
  `docs/IMAGE_ROOTFS_PRUNE.md`; `ab-testing` corre en `pull_request` con paths `tools/**`.
  Conclusión: **no existe un manifiesto de CI del commit exacto**; la referencia más cercana es el
  head del PR #193.
- PR #193 (merge `c888308e`, 2026-10-21T18:06:32Z): head `08d604d9…`, run
  [`39649140152`](https://github.com/iariap/x6200_gui/actions/runs/39649140152) `UI A/B testing
  harness` = **success** (JSON en `capturas/ci/run-39649140152-pr193.json`).
- `runs-docstip-90aebad5.json`: 0 corridas.
- `runs-newtip-50bf8ab0.json`: run [`39653588113`](https://github.com/iariap/x6200_gui/actions/runs/39653588113)
  `boot-rootfs` = **failure** (push de #200, posterior al candidato; se registra como referencia,
  no como fallo de esta unidad).
- `runs-recent-50.json`: listado de las 50 corridas recientes con URL/head/conclusión.

## 4. Paso 2 — Inventario exhaustivo (`coverage.csv`)

Archivo: [`coverage.csv`](coverage.csv) — **334 filas**, una por pantalla/app/acción/grupo de
Settings/ítem de Settings/página de botones/entrada física/subsistema/servicio/paquete/config del
Buildroot y por prueba existente, con ruta fuente y **dueño de medición** (`#195`…`#199`). No se
borra ninguna fila: lo que no tiene instrumento queda `UNSUPPORTED`. La exhaustividad se verifica
con [`scripts/coverage-audit.py`](scripts/coverage-audit.py) contra las fuentes del candidato
([log](logs/step2/coverage-audit.log)): acciones 42/42, apps 24 del dispatcher + Home + alias FT4,
8 grupos y 31 ítems de Settings, 16 filas de la barra que nombran las 29 páginas reales, 18
servicios (`init.d` del GUI y del overlay + `usr/sbin`), 7 paquetes y 4 piezas de config/overlay
del Buildroot, 3 pantallas de diálogo y 114 pruebas.

| Dimensión | Filas |
|---|---|
| apps (24 del dispatcher + FT4 alias + Home) | 26 |
| acciones `press_action_t` | 42 |
| grupos de Settings | 8 |
| ítems de Settings | 31 |
| páginas de la barra de botones (29 páginas reales, agrupadas) | 16 |
| entradas físicas (F1–F5, touch, rotary 0–3, teclado, PTT) | 10 |
| pantallas que no son app (dialog_freq, msg CW, msg voz) | 3 |
| subsistemas | 49 |
| servicios (`rcS`, `S01boot_time`, `S20gui`, `S39btpairings`, `S94btcat`, `S01create_data`, `S02update`, `S03tune_cache`, `S04restore_backup_config`, `S11radio_daemon`, `S50alsa`, `S50pulseaudio`, `S96webserver`, `btaudio-ctl`, `btcat-ctl/setup`, `x6200_daemon`, `x6200-field`, `x6200-pota-spots`) | 18 |
| paquetes del Buildroot (`x6200-gui`, `ft8lib`, `aether-x6200-control`, `x6100-webserver`, `rhvoice`, `rtw88`, `python-bottle-sqlite`) | 7 |
| config/overlay del Buildroot (`X6200_defconfig`, `genimage.cfg`, `post-build.sh`, `rootfs-overlay`) | 4 |
| entorno/CI/toolchain | 6 |
| pruebas existentes (114 de 317 rutas versionadas en `tests/` + `tools/ab_testing/tests`) | 114 |

Cobertura declarada: **151 REAL, 138 PARTIAL, 4 SYNTHETIC, 41 UNSUPPORTED**.
Dueños: #197 = 142, #195 = 102, #196 = 51, #198 = 18, #199 = 21.

**Diferencias explícitas que el inventario deja visibles:**

- **FT4 es alias de FT8**: en `main_screen_start_app` no hay `case` de FT4; `remote_server.cpp`
  mapea `app_names[2] = "FT4"` a `ACTION_APP_FT8`. Queda como fila propia con nota de alias.
- **DFLEditor**: figura en el control remoto (`app_names[25]`) y se abre desde la 5.ª celda de la
  barra DFL / long press DFL, pero **no está en el menú APPS por defecto del barrido**
  (`tools/ui_harness` no tiene fixture "DFL editor" como app del menú). Queda fila propia.
- **About**: eliminada del firmware (2026-10-14) pero el manual la sigue documentando (§6.22);
  ver [`docs-review.md`](docs-review.md).

**Top-5 huecos de cobertura** (filas `UNSUPPORTED`/`BLOCKED`, sin inventar medición):

1. **Camino de radio real** (RX, TX/PTT, ATU tune, CAT, USB, GPS/WiFi real): sólo `HARDWARE`,
   dueño #199; en #195–#198 no se toca la radio. Los 41 `UNSUPPORTED` son, en su mayoría, este
   camino y el arranque/init real (sin kernel ni QEMU de sistema en esta unidad).
2. **Web configurator** (19 páginas): `SKIP` en esta unidad (WS inexistente y sin `python3` de
   target privado); no se adjudica a la GUI.
3. **CTest `test_ft8_qso` / `test_qth`**: `BLOCKED` por dependencias exactas (ver §5).
4. **Persistencia/arranque/update/DATA**: `PENDING`/`BLOCKED` para #196 (no hay instrumento de
   arranque ni copia de DATA en esta unidad).
5. **Documentación del operador**: hueco DFL editor y sección About muerta; además conteos
   históricos inconsistentes; `REVIEW_REQUIRED` para #197/#199.

## 5. Paso 3 — Revisión de documentación, fixtures y pruebas

- Revisión completa en [`docs-review.md`](docs-review.md): 18 afirmaciones contrastadas contra el
  candidato. Hallazgos principales: `docs/TESTING.md` declara 55/469 comprobaciones (hoy 138) y
  `docs/INTEGRATION_AUDIT.md` 401 (los tres números conviven y no coinciden); `ROM_REVIEW.md`,
  `RESOURCE_USAGE.md` e `IMAGE_INVENTORY.md` son evidencia de imágenes del 2026-10-11/12, no del
  candidato (los temas legados que lista ya no existen); el manual documenta About (eliminada) y
  no documenta el editor DFL; `check_manual_apps.py` reporta `ACTION_APP_DFL_EDITOR` como “sin
  nombre mapeado” y aun así termina 0.
- Fixtures: **199** archivos `.scn` en `tools/ui_harness/fixtures` ([`fixtures.txt`](fixtures.txt)).
- Tests versionados: **317** rutas en `tests/` + `tools/ab_testing/tests` ([`tests.txt`](tests.txt));
  **114** pruebas inventariadas y asignadas una sola vez por dueño en `coverage.csv` (sin duplicar
  la suite completa en unidades posteriores).

## 6. Paso 4 — Baseline host (una vez) y complementos

Comando exacto (cwd `assessment-194/clone-gui`):

```
WS="/home/pablo/desa/x6200/assessment-194/scratch/no-webserver" sh tests/run_all_tests.sh
```

Resultado: **exit 0**, **17 OK, 0 fallos**, **5 omitidos** (`wspr`, `sstv`, `wefax`, `js8`,
`webserver`). Log completo: [`logs/step4/run_all_tests.log`](logs/step4/run_all_tests.log);
logs por suite en [`logs/step4-runners/`](logs/step4-runners/).

| Runner | Resultado | Detalle |
|---|---|---|
| `config` | OK | 70 campos inicializados en orden |
| `actions` | OK | toda acción ofrecida tiene handler |
| `settings_focus` | OK | todos los widgets de Ajustes en el grupo de foco |
| `settings_grid` | OK | grilla de Ajustes |
| `apps_menu` | OK | todas las apps del menú llegan al dispatcher |
| `app_handlers` | OK | todos los botones de apps tienen handler |
| `manual_apps` | OK | 24 apps con sección (ver límite en §5) |
| `pruebas` | OK | estructura y evidencia de `PRUEBAS.md` |
| `wcag` | OK | pares WCAG 2.1 AA |
| `cw_tune_feed` | OK | alimentación del tune CW |
| `autotune_v2` | OK | Auto Tune v2 (#190/#191) |
| `build_identity` | OK | identidad de build fundada |
| `curve_evidence` | OK | 8 pruebas de curva ROE |
| `host` | OK | **138 comprobaciones, 0 fallos** |
| `host_ci_parity` | OK | **138 comprobaciones, 0 fallos** (lvgl/ oculto y restaurado) |
| `drawbuf_partial` | OK | framebuffer idéntico con draw buffer parcial (sw-rotate 90) |
| `navtex` | OK | `all OK` |
| `wspr`/`sstv`/`wefax`/`js8` | SKIP | imagen `x6200-uiharness` minimal sin `fftw3.h`/`sndfile.h` (exit 77) |
| `webserver` | SKIP | `WS` inexistente (por diseño, §7) |

**Sanitizers:** el host gcc 16.2.1 **no tiene runtime** (`libasan.so.8`, `libubsan.so.1`,
`libtsan.so.2` ausentes para el linker): `run_native_tests.sh` omite ASan/UBSan y TSan. Para no
dejar el hueco, se repitió **sólo la suite nativa** con el mismo script dentro de
`x6200-uiharness-full` (gcc 13.3.0, Ubuntu 24.04): **ASan + UBSan activos**, **TSan disponible**
y la fixture de carrera deliberada disparó; resultado **138 comprobaciones, 0 fallos**
([`logs/step4b/native-sanitizers.log`](logs/step4b/native-sanitizers.log)). Es una capa HOST con
otro toolchain (x86_64), no ARM.

**Códecs con imagen full** (mismos scripts que usa el runner, `IMAGE=x6200-uiharness-full`):

| Suite | Resultado |
|---|---|
| WSPR | **158 comprobaciones, 0 fallos** ([log](logs/step4b/codec-wspr.log)) |
| SSTV | **all OK**, 7 modos, correlación 0,956–0,982 ([log](logs/step4b/codec-sstv.log)) |
| WeFax | **PASS**, 300/300 líneas, correlación 0,9953 ([log](logs/step4b/codec-wefax.log)) |
| JS8 | **1994 aserciones en 88 test cases** ([log](logs/step4b/codec-js8.log)) |

**`tools/ab_testing`** (cwd `clone-gui/tools/ab_testing`; `UV_CACHE_DIR` privado porque
`~/.cache` es de sólo lectura para el sandbox):

| Comando | Resultado |
|---|---|
| `uv sync --frozen` | OK (pytest 9.1.1, pyyaml 6.0.3, ruff 0.17.0, ty 0.0.87) |
| `uv run ruff check .` | `All checks passed!` |
| `uv run ruff format --check .` | `199 files already formatted` |
| `uv run ty check src tests` | `All checks passed!` |
| `uv run pytest -m "not e2e"` | **227 passed, 13 deselected** (5,29 s) |

**CTest `run_tests.sh` (ft8/qth): BLOCKED.** `bash run_tests.sh` en una copia limpia del clon
falla en `src/ft8/worker.h:11: fatal error: ft8lib/constants.h: No such file or directory`
(la librería `ft8lib` la provee Buildroot como paquete; no está en el host) y, además, el host no
tiene runtime de ASan/UBSan que `tests/CMakeLists.txt` fuerza con `-fsanitize=address,undefined`.
No se creó un target ni se fabricó el binario; los targets `test_ft8_qso` y `test_qth` quedan
registrados como existentes pero **no ejecutados**.

## 7. Paso 5 — Webserver

- `WS=/home/pablo/desa/x6200/assessment-194/scratch/no-webserver` **no existe**: la condición del
  runner `[ -f "$WS/tests/run_pages_test.py" ]` es falsa y omite el webserver **antes** de tocar
  el volumen compartido. Registrado como **SKIP** (nunca PASS).
- No se usó `x6200-build-output`; la imagen `x6200-buildroot` no trae `/b/target/usr/bin/python3`
  sin ese volumen. Existe un checkout del webserver en `/home/pablo/desa/x6200/.x6200_webserver`
  (`HEAD 7bf44f84…`, árbol limpio) pero pertenece a otra sesión: **no se usó**.
- Evidencia: [`logs/step1/webserver-skip.log`](logs/step1/webserver-skip.log) y el resumen del
  runner. Las páginas del webserver **no se adjudican a la GUI**.

## 8. Paso 6 — Artefactos ARM/ROM en sandbox privado

Receta (idéntica a `.github/workflows/main.yml`, con ambos repos congelados y override local):

1. `clone-buildroot` en `d084e287` + submódulo `buildroot` `cc34b5b2`; `bash ./br_config.sh`
   (genera `build/.config` con `X6200_defconfig`).
2. `build/local.mk` **creado sólo en el build privado**:

```
X6200_GUI_OVERRIDE_SRCDIR = /home/pablo/desa/x6200/assessment-194/clone-gui
```

   SHA256 del archivo: `6b9a8ef9e2be3291a8d05c835bc49cd5b9ded6965b7de0c7e7c79f02cc2c152e`.
   No se editó `br2_external/package/x6200-gui/x6200_gui.mk` (sigue apuntando al vendor
   `gdyuldin/x6200_gui` v0.1.0).
3. `make -j16 lame mpg123` → `make libsndfile-dirclean` → `make -j16`, con logs completos.

**Resultado: imagen completa, pero el build necesitó una corrección de configuración local.** La
receta congelada, ejecutada tal cual, **no llega a imagen**: `X6200_defconfig` (d084e287) fija
`BR2_PACKAGE_PULSEAUDIO_DAEMON=y` sin `BR2_PACKAGE_PULSEAUDIO=y`; kconfig descarta el símbolo y
con él `libpulse`/`libsndfile`, que el `CMakeLists.txt` del GUI exige (`pkg_check_modules`). El
configure de `x6200-gui` falla con `sndfile`, `libpulse`, `libpulse-simple` ausentes
([log](logs/step6/make-resume3.log.gz)). Se habilitó **sólo en el `.config` del build privado**
`BR2_PACKAGE_PULSEAUDIO=y` + `make olddefconfig` (arrastra `PULSEAUDIO_ENABLE_ATOMIC`,
`LIBSNDFILE` y `LIBTOOL`): [cambio exacto y diff](logs/step6/config-change.log),
[config final](logs/step6/config-build-final.config). **Es una desviación declarada** respecto del
defconfig congelado; el `.mk` del paquete y el defconfig no se tocaron.

El build se interrumpió una vez (proceso terminado a las ~19:22Z sin escribir error ni imágenes;
[log del primer intento](logs/step6/make-first-attempt.log.gz), [build-time.log](logs/step6/build-time.log)),
y se reanudó sobre el mismo árbol en cuatro pasadas: `host-gdb` con estado a medias
([resume1](logs/step6/make-resume1.log.gz)), descarga FTP de `host-uboot-tools` colgada 13 min
([resume2](logs/step6/make-resume2.log.gz); se sembró el tarball con su sha256
`312b7eea…` en `dl/`), configure del GUI sin audio ([resume3](logs/step6/make-resume3.log.gz)) y
**[make final exit 0](logs/step6/make-resume4.log.gz)** a las 20:53Z. La primera pasada usó
`-j16`; las reanudaciones, `-j4`.

Artefactos de esta corrida (capa build, no ejecutados):

| Archivo | Bytes | SHA256 |
|---|---:|---|
| `sdcard.img` | 571.473.920 | `d0ff4435ad1d068e25bf2c357a4e93478463b187e7745a9a54844488e44e33a5` |
| `rootfs.tar` | 242.790.400 | `7c26c76b6fc27bc12dc8d5e012e51d5c2c0568fa72a8e02cad5432dfcd36a2dd` |
| `rootfs.ext4` (symlink a `rootfs.ext2`) | 536.870.912 | `9365b52f9aef33e0623ae8271aaf415483fe72e1afbc8ef99f93e15437bc620a` |
| `boot.vfat` | 33.554.432 | `914f07eb00d1e5afee56ab795ce6b7b1d4410d774a3534881da494319c30b17d` |
| `zImage` | 5.567.760 | `660c94c6f7231e7dc262fefb295aae1a8dfcf9b9c2b46cdc1eced2c3296bcc14` |
| `u-boot-sunxi-with-spl.bin` | 556.112 | `dd96b0ca1eda2c98b318ed35f284d873e64eca795a51f1d9b944115a77de0381` |
| `sun8i-r16-x6200.dtb` | 26.544 | `4d73f8a5d5f846c21e4ac30550a5811443a01666f9ee57880b5316e78627cc38` |

Los artefactos quedan en `assessment-194/clone-buildroot/build/images/` (copia durable local, fuera
de Git por tamaño); acá se entrega hash+tamaño y los logs completos comprimidos.

**Control cruzado con el build del orquestador** (`/home/pablo/desa/x6200/rom-build`, sólo
lectura): produjo una ROM con el **GUI del tip `50bf8ab0`**, no del candidato. Su `x6200_gui.mk`
fue editado (`X6200_GUI_SITE=…/rom-build/clone-gui`, `SITE_METHOD=local`) y su `.config` tiene
`PULSEAUDIO`/`LIBSNDFILE` habilitados a mano; el binario embebe `a576d48d9994efd3e28b9f1438fa7b24`
= `fuente-md5` de ese clon shallow, y su rootfs incluye `S01bootanim` (cambio de producción de
#200). El gate #86 **pasa** (es el port, 13.412.248 B) pero su identidad **no es `c888308e`**:
se registra como divergencia, no como artefacto del candidato
([identidad](logs/step7/identity-orchestrator.log)).


## 9. Paso 7 — Identidad de los artefactos

Todas las comprobaciones sobre `roots.tar` y `rootfs.ext4` de **esta corrida** (capa HOST; no se
ejecutó el binario):

| Comprobación | Resultado |
|---|---|
| `python3 tools/verifica-firmware-port.py --tar <rootfs.tar>` (gate #86) | **exit 0**: `OK … es el firmware del port (ELF32 ARM, 13.407.624 B, 3/3 marcas)` |
| `/usr/sbin/x6200_gui` (desde `rootfs.tar` y desde `rootfs.ext4` con `debugfs dump`) | 13.407.624 B, **md5 `1f998c46909307397525c975a712adfd`**, sha256 `7e2e9331…`; las dos vías dan el mismo md5 |
| `file` / `readelf -h` | `ELF 32-bit LSB pie executable, ARM, EABI5 … stripped`; `ET_DYN`, Machine ARM |
| `fuente-md5` | checkout candidato = staged source = **`964193ae9306b5127845cec44a57d52c`**, **embebido en el binario** (único md5 de 32 hex en el blob) |
| `/etc/fstab` | `/dev/mmcblk0p3 /mnt ext4 rw 0 2` ✓ |
| `/etc/init.d/S01create_data` | contiene `DATA ya existe` y monta con el tipo real ✓ |
| `/etc/init.d/` en el rootfs | 24 entradas, incluidas `S01create_data`, `S02update`, `S20gui`, `S94btcat` |
| `rootfs.tar` vs `rootfs.ext4` | mismo binario (md5 idéntico) |

Log completo con todos los comandos y salidas: [`logs/step7/identity-mine.log`](logs/step7/identity-mine.log)
y JSON crudo [`identity-mine.json`](logs/step7/identity-mine.json).



## 10. Paso 8 — Registro de cobertura funcional

| Métrica | Valor |
|---|---|
| Suites del runner `run_all_tests.sh` | **17 OK, 0 fallos, 5 omitidos** (4 codecs por imagen minimal + webserver) |
| Códecs repetidos en imagen full | **4 OK**: WSPR 158 comprobaciones, SSTV `all OK` (7 modos, r 0,956–0,982), WeFax 300/300 (r 0,9953), JS8 1994 aserciones en 88 casos |
| `tools/ab_testing` | ruff/format/ty limpios; **227 passed, 13 deselected** |
| Sanitizers | host gcc 16: **omitidos** (sin runtime); contenedor full gcc 13: **ASan+UBSan activos**, **TSan activo** y la fixture de carrera deliberada **disparó**; 138 comprobaciones, 0 fallos |
| CTest `test_ft8_qso`/`test_qth` | **BLOCKED**: falta `ft8lib/constants.h` y `libasan/libubsan` en el host (exit 2) |
| Cobertura inventariada | **334 filas**; auditoría estructural **PASS** (42/42 acciones, 24 apps+Home+FT4, 8/31 Settings, 29 páginas, 18 servicios, 7 paquetes, 114 pruebas) |
| Dependencias faltantes exactas | `libasan.so.8`, `libubsan.so.1.0.0`, `libtsan.so.2` (host gcc 16); `ninja`, `fakeroot`, `patchelf`; `ft8lib/constants.h`; `fftw3.h`, `sndfile.h` (imagen minimal); `BR2_PACKAGE_PULSEAUDIO=y` ausente del defconfig |
| Reproducibilidad | **no medida bit a bit** (no se compiló dos veces el mismo par): no se afirma hash reproducible. Sí se demuestra que el binario embebe el `fuente-md5` del candidato |



## 11. Limitaciones

1. **Radio y hardware**: esta unidad no ejecuta nada sobre el equipo; RX/TX/ATU/CAT/USB/GPS/WiFi
   reales quedan `PENDING_HARDWARE` (#199). No se transmitió ni se cortó energía. El binario ARM
   no se ejecutó (QEMU/hardware quedan para #196/#199): la identidad es estática.
2. **Webserver**: SKIP por diseño (no se toca el volumen compartido de otra sesión).
3. **CTest ft8/qth**: BLOCKED por `ft8lib` y runtime de sanitizers del host (causa exacta en §6).
4. **Sanitizers**: el host no los soporta; el complemento en contenedor es x86_64/gcc 13, no ARM.
5. **CI**: no hay corrida del commit de merge `c888308e`; se usa el head del PR #193 como
   referencia y se distingue de las corridas nuevas de esta campaña (ninguna).
6. **Divergencia del tip**: `50bf8ab0` (PR #200) cambia producción después del congelamiento;
   este dossier es válido para `c888308e`, **STALE** para el tip nuevo. El ROM del orquestador
   corresponde al tip (`fuente-md5 a576d48d`), no al candidato, y no se usa como identidad.
7. **Defconfig incompleto**: `X6200_defconfig` no habilita `BR2_PACKAGE_PULSEAUDIO` y el GUI no
   configura sin `libpulse`/`libsndfile`; la imagen de esta unidad se construyó con ese símbolo
   habilitado en el `.config` local (desviación declarada en §8).
8. **Build interrumpido**: el primer `make` murió sin escribir error; se reanudó el mismo árbol y
   el make final salió 0. No se repitió el build completo, así que no hay reproducibilidad
   bit a bit (y no se afirma).
9. **`banco-apps` ocupado**: no se usó; los escenarios que lo requieran son de #196/#197/#198.
10. Los conteos de documentación histórica no se copian como actuales (ver `docs-review.md`).

## 12. Handoff a las unidades siguientes

- **#196 (ROM/confiabilidad)**: consumir `artifact-manifest.json` y los artefactos de §8/§9 (no
  re-ejecutar la suite completa). Usar `rootfs.tar` sha256 `7c26c76b…` (GUI 13.407.624 B, md5
  `1f998c46…`); **no** el ROM del orquestador (tip `50bf8ab0`). El paquete `x6200_gui` se construyó
  con `OVERRIDE_SRCDIR` al clon del candidato y el binario embebe `fuente-md5 964193ae…`.
  Reproducir el build exige habilitar `BR2_PACKAGE_PULSEAUDIO=y` (§8); `banco-apps` sigue ocupado.
- **#197 (UI/UX)**: `coverage.csv` asigna 138 filas; usar los 199 fixtures existentes; atender el
  hueco documental DFL/About (`docs-review.md`) sin corregirlo en #195.
- **#198 (performance)**: el baseline no mide performance; los huecos de spectrum/waterfall,
  natural/wakeups, recursos y audio quedan asignados a #198.
- **#199 (consolidación + QA físico)**: el protocolo físico debe cubrir los 19 `UNSUPPORTED` y los
  12 `PENDING_HARDWARE`; el webserver y CTest quedan como huecos documentados.

## 13. Evidencia y reproducibilidad

- Todos los comandos con cwd, variables no secretas, inicio/fin UTC, exit code y referencia al log
  completo: [`commands.jsonl`](commands.jsonl). Los logs de build grandes van comprimidos sin
  pérdida (`.log.gz`) para no meter decenas de MB en Git; el original queda en
  `assessment-194/scratch/logs/` con el mismo contenido.
- Scripts locales de captura/análisis usados (no sustituyen harness ni tests):
  [`scripts/coverage-audit.py`](scripts/coverage-audit.py), [`scripts/make_coverage.py`](scripts/make_coverage.py),
  [`scripts/verify-artifacts.py`](scripts/verify-artifacts.py).
- Hash de todos los archivos de esta carpeta: [`SHA256SUMS`](SHA256SUMS).
- Manifiesto con SHAs, toolchain, configuración, artefactos y limitaciones:
  [`artifact-manifest.json`](artifact-manifest.json).
- Resultados legibles por escenario: [`results.csv`](results.csv); inventario: [`coverage.csv`](coverage.csv).
- Logs crudos completos: [`logs/`](logs/). JSON originales de CI: [`capturas/ci/`](capturas/ci/).
