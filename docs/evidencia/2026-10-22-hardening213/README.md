# Evidencia — issue #213: persistencia SQLite fail-closed (epic #211, hijo 2/4)

- **Fecha:** 2026-10-22
- **App:** `iariap/x6200_gui` — rama de trabajo `factory/hardening-213`
- **Base medida:** `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845` (= tip de `origin/lu2dsx/x6200-rom` al iniciar)
- **Contrato:** [#213](https://github.com/iariap/x6200_gui/issues/213); épica [#211](https://github.com/iariap/x6200_gui/issues/211)
- **Alcance:** sólo GUI (un commit). Sin push, sin merge, sin radio, sin SD del owner.

## 1. Qué se cambió (y por qué)

| Archivo | Cambio |
|---|---|
| `src/params/db.c` | `database_init()` fail-closed: comprueba que `/mnt` sea un montaje real, de otro filesystem y `rw`; siembra la plantilla con temporal+`link()` (sin pisar nada, 0644, `fsync`); abre `READWRITE` sin `CREATE`; `PRAGMA quick_check(1)=ok`; exige el esquema base (y el que agregan las migraciones); propaga el rc de `sqlite3_step()`, `PRAGMA` y statements; `Persistence write failed` con rc/path, sin spam. |
| `src/params/migrations.c` | Toda la secuencia de migraciones en UNA transacción (con `version` adentro): nada a medias y la versión nunca sube ante fallo. Versión ausente/negativa/más nueva que el firmware → aborta. Guarda de la migración 2 (ATU): convierte sólo si TODAS las filas están en forma de índice; si hay mezcla, aborta sin mutar. |
| `src/params/params.c` / `.h` | `params_init()` devuelve `bool`: valida los statements antes de `cfg_init()`, y el hilo de guardado sólo arranca si todo quedó inicializado. `COMMIT` fallido se reporta. |
| `src/cfg/cfg.c` | `cfg_init()` es fatal si `init_params_cfg()` falla de verdad (no registra observers ni arranca su hilo); una clave AUSENTE sigue siendo el default en memoria (no rompe radios actualizadas en el lugar). |
| `src/main.c` | Guard de arranque: si `params_init()` falla, `Persistence unavailable: check DATA and params.db` por stderr/LV_LOG y `exit(78)` **antes** de radio, CAT, audio y TX. |
| `tests/native/*` | Suite focalizada nueva (ver abajo) + stage en `run_native_tests.sh` y en `run_all_tests.sh`. |

## 2. Método de medición

- **HOST (suite en `run_native_tests.sh`)**: `tests/native/test_db_persistence.c` incluye `src/params/db.c` (para llegar a sus funciones internas, mismo patrón que `test_remote.cpp`) y linkea el `src/params/migrations.c` real. Corre sobre SQLite real, con fixtures compartidos (`db_fixtures.c`). No hay montajes: lo que necesita `/mnt` de verdad va en la suite siguiente.
- **HOST con `/mnt` real (`tests/native/run_db_failclosed.sh`)**: `unshare -Ur -m -p` + chroot privado; `/mnt` es un tmpfs rw, un bind RO, un tmpfs lleno o un directorio sin montar (caso "DATA no montada"). Mide el `database_init()` real con las rutas reales (`/mnt/params.db`, `/usr/share/x6200/params.default.db`). El namespace es privado: el `/mnt` del host no se toca.
- **QEMU con el binario ARM real (`run-arm.sh` + `arm-inner.sh`)**: chroot del rootfs del target construido *de este árbol* (volumen privado), `qemu-arm` por binfmt, `LD_PRELOAD=libfakefb.so` y contenedor privado por caso. Mide el arranque completo de `main()` (exit 78, mensajes, boot.log, socket de control remoto) con `/mnt` real. No bootea kernel, no hay RF ni radio.

## 3. Matriz de casos — HOST con `/mnt` real

Comando: `sh tests/native/run_db_failclosed.sh` (matriz cruda en [`host/matrix.txt`](host/matrix.txt)).
En todos los casos el binario sale 0 y afirma su resultado; `esperado` documenta la semántica del caso.

| caso | condición de `/mnt` | base | esperado | efecto observado (log crudo) |
|---|---|---|---|---|
| `valid` | tmpfs rw | válida v5 | arranca | `init=true bytes_unchanged=true version=5` |
| `corrupt` | tmpfs rw | bytes basura | rechaza | `init=false bytes_unchanged=true` |
| `no-mount` | directorio sin montar | ninguna | rechaza | `init=false created=false` (no creó `/mnt/params.db` sobre el rootfs) |
| `absent` | tmpfs rw | ninguna + plantilla | arranca | `init=true seeded_from_template=true mode=0644` |
| `absent-no-template` | tmpfs rw | ninguna, sin plantilla | rechaza | `init=false created=false` |
| `ro` | bind RO | válida v5 | rechaza | `init=false bytes_unchanged=true` |
| `schema-incomplete` | tmpfs rw | sin `bands` | rechaza | `init=false bytes_unchanged=true` |
| `version-missing` | tmpfs rw | tabla `version` sin fila | rechaza | `init=false bytes_unchanged=true` |
| `v1-atu` | tmpfs rw | v1, ATU en índices | arranca+migra | `init=true version=5 freq=175000,275000` |
| `v1-atu-mixed` | tmpfs rw | v1, ATU mezclada | rechaza | `init=false bytes_unchanged=true version=1` |
| `full` | tmpfs lleno | válida v5 | arranca, escritura falla | `init=true write_failed=true dirty_kept=true errors=1` |

## 4. Matriz de casos — QEMU, binario ARM real

Comando: `docs/evidencia/2026-10-22-hardening213/run-arm.sh` — **exit 0, 12/12 casos OK**
(matriz cruda en [`arm/matrix.txt`](arm/matrix.txt); por caso, las aserciones en `arm/logs/<caso>.log`,
el stdout/stderr crudo del binario en `arm/logs/<caso>.raw` y la base medida en
`arm/logs/<caso>.params.db`).

Medición: chroot de un rootfs del target construido **de este árbol** (volumen privado
`h213-x6200-out`), `qemu-arm` por `binfmt_misc`, `LD_PRELOAD=libfakefb.so` (pantalla falsa 480x800),
`QEMU_CPU=cortex-a7`, y un contenedor **privado por caso** con `/mnt` montado de verdad (tmpfs rw,
bind RO, tmpfs lleno o directorio sin montar) y `/proc` propio. No bootea kernel, no hay RF, no se
tocó la radio ni la SD del owner.

| caso | condición de `/mnt` | base | efecto observado (log crudo) |
|---|---|---|---|
| `valid` | tmpfs rw | plantilla de la ROM (v1 → migra) | arranca (socket en 16 s, handshake ok), boot.log con el sello `51b92487` y hasta `radio`, sin SIGSEGV, presets/identificadores idénticos a la plantilla |
| `corrupt` | tmpfs rw | bytes basura | `exit 78`, motivo `params.db: invalid database`, base con sus bytes, sin socket, boot.log sin `radio` |
| `no-mount` | directorio sin montar | ninguna | `exit 78`, `DATA is not mounted`, **no se creó** `/mnt/params.db` sobre el rootfs |
| `absent` | tmpfs rw | ninguna + plantilla | arranca, `seeding from /usr/share/x6200/params.default.db`, base creada modo 0644, presets idénticos a la plantilla |
| `ro` | bind RO | plantilla | `exit 78`, `is mounted read-only`, base intacta |
| `schema-incomplete` | tmpfs rw | sin `bands` | `exit 78`, `incomplete schema`, base intacta, boot.log sin `radio` |
| `version-missing` | tmpfs rw | tabla `version` sin fila | `exit 78`, `The version table has no row`, base intacta, boot.log sin `radio` |
| `v1-atu` | tmpfs rw | ATU en índices (v1) | migra y arranca; ATU `3→175000` y `5→275000` (una sola conversión), versión 5 |
| `v1-atu-mixed` | tmpfs rw | ATU mezclada | `exit 78`, `ambiguous provenance, refusing to convert`, ATU intacta (`3`, `7025000`), versión 1 |
| `full` | tmpfs lleno (libre ~16 K) | plantilla + umbral inválido | arranca y **reporta** `Persistence write failed`; `PRAGMA integrity_check=ok`; el valor propiamente no quedó persistido |
| `cycles` | tmpfs rw | plantilla | 100 órdenes de frecuencia por el socket: sigue contestando, el proceso vive, `integrity_check=ok` |
| `reboot` | tmpfs rw | plantilla | dos arranques: `freq=14074000` y `filtro=500` idénticos en el arranque 1 y el 2 |

`SIGSEGV`: 0 en **todos** los casos (`CRASHLOG sig=11` ausente; el defecto del assessment era PC
`0x42e12c`, exit 139).

Notas de la corrida (para el orquestador):

- **Build privado**: el volumen propio necesitó `BR2_PACKAGE_SPEEXDSP=y` y `BR2_PACKAGE_LIBDRM=y`
  (los exige `src/CMakeLists.txt` por pkg-config) y `boost` (headers, que pide
  `third-party/js8core`): en el `X6200_defconfig` que trae la imagen `x6200-buildroot` esas tres
  opciones están en `not set` y el build fresco falla al configurar/compilar la GUI. El volumen
  compartido `x6200-build-output` lo tapa (staging viejo con los `.pc`/headers ya instalados). No es
  un defecto de #213, pero sí un hallazgo para la **reconstrucción reproducible** de la épica (repo
  Buildroot). El binario medido salió igualmente de **este** árbol y con el sello de fuentes correcto.
- Un arranque **sano** agrega filas a la base (los savers del firmware persisten defaults de otras
  rutas: `wifi_enabled`, params de la banda/modo actuales). Por eso la comparación con la plantilla
  es "no falta ningún preset y ninguno cambió", no "mismos bytes" — los SHA se registran igual en
  las dos matrices.

## 5. Gates

Corrido en el worktree `state/worktrees/x6200/hardening-213` (rama `factory/hardening-213`), sobre el
HEAD del commit de este cambio (base `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845`); el SHA exacto lo
reporta el cierre de la corrida (la rama no se pushea desde acá).

| gate | comando | resultado |
|---|---|---|
| Suite nativa | `sh tests/native/run_native_tests.sh` | **exit 0**; incluye el stage nuevo `#213` con `141 comprobaciones, 0 fallos` (log completo en [`host/native-tests.log`](host/native-tests.log)) |
| Fail-closed con `/mnt` real | `sh tests/native/run_db_failclosed.sh` | **exit 0**, 11/11 casos (matriz abajo y en [`host/matrix.txt`](host/matrix.txt)) |
| Estáticos | `python3 tests/check_cfg_fills.py .` · `audita-pruebas.py` · `check_actions.py` · `check_apps_menu.py` · `check_app_handlers.py` · `check_settings_focus.py` · `check_settings_grid.py` · `check_manual_apps.py` · `check_wcag.py` · `check_cw_tune_feed.py` · `check_build_identity.py` | todos **OK** |
| Matriz de la app | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | **exit 0**, 17 checks; salida completa en [`gates/check-matrix.txt`](gates/check-matrix.txt) |
| Sello de fuentes | `tools/fuente-md5.sh <worktree>` | `51b924875d97a8dfd68a77ae692f64c3` (§6) |

Dentro de `check-matrix.sh`:

- `native tests`, `check_cfg_fills`, `check_actions`, `check_settings_focus`, `check_apps_menu`,
  `check_app_handlers`, `check_manual_apps`, `check_wcag`, `check_cw_tune_feed`,
  `check_build_identity`, `x6200-ab themes`, **`x6200-ab contracts (HEAD)`**, `bash -n` de los
  shell tocados y el gate de determinismo del arnés de UI: **ok**.
- `[SKIP] check_wcag (temas de la imagen)`, `[SKIP] ruff/pytest del framework A/B`: el diff no toca
  esos paths (el gate espeja el trigger del CI).
- `[BLOCKED] banco QEMU: preflight (tools/captura-banco.sh --check)`: falta el chroot del rootfs de
  la ROM (`../.vfix3`) y el cliente `tools/remote/x6200-remote`; el banco del owner corre en el host
  del owner y el contrato de esta corrida prohíbe usar el volumen/rootfs ajenos (`x6200-build-output`,
  `.vfix3`). **No es PASS**: la matriz lo reporta como BLOCKED con la causa exacta.

Nota de la corrida: el primer intento de `check-matrix` sobre el commit mostró
`x6200-ab contracts` en **BLOCKED (exit 2)** porque `tools/ui_harness/mocks/mock_params.c` declaraba
`void params_init(void)`; el mock se adaptó a la firma nueva (`bool`, issue #213) y el gate quedó
**ok** en la corrida definitiva registrada arriba.

## 6. SHAs y sello

| qué | valor |
|---|---|
| HEAD de la rama `factory/hardening-213` | el commit de este cambio (ver el reporte de cierre; la rama no se pushea desde acá) |
| base (tip de `origin/lu2dsx/x6200-rom` al iniciar) | `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845` |
| sello de fuentes del árbol medido (`tools/fuente-md5.sh`) | `51b924875d97a8dfd68a77ae692f64c3` |
| binario ARM medido (`arm/SHA256SUMS`) | `d32be99dc1079b43126557583f7c2691debdf222fa65983ac81866967ba6574b` (13.937.260 B, ELF32 ARM, `verifica-firmware-port.py` OK) |
| boot.log del caso `valid` (sello embebido en el binario) | `boot_identity: fuente: 51b924875d97a8dfd68a77ae692f64c3 (x6200_gui)` |

El binario se compiló con el toolchain de Buildroot del volumen privado `h213-x6200-out` desde el
`src/` de este commit (override de fuentes del paquete + `X6200_FUENTE_MD5` con el sello); el sello
que el binario loguea al arrancar coincide con `tools/fuente-md5.sh` del árbol.

## 7. Rojo de CI del PR #225 y su fix (ASan+LSan y `fchown`)

El job **`UI Functional Tests`** del run `39742568100` marcó dos cosas sobre esta suite:

1. **Leak de 465 KB en 1166 asignaciones** (`LeakSanitizer`, todas indirectas de `libsqlite3`) en el
   stage `#213`. Causa: el helper local del test,

   ```c
   static void db_close(void) {
       if (db != NULL) { sqlite3_close(db); db = NULL; }   /* <-- insert_stmt seguia vivo */
   }
   ```

   con `insert_stmt` (el statement que deja preparado `db_prepare_insert()`) **sin finalizar**.
   `sqlite3_close()` sobre una conexión con un statement vivo devuelve `SQLITE_BUSY`, **no cierra**
   y deja colgadas la conexión y toda su memoria interna (por eso LSan la ve como indirecta de la
   lib). Alcanzaba a los casos de escritura (`test_write_success`/`test_write_failure`) y a los de
   migración. Fix: cerrar siempre con el helper de `db.c`
   (`db_close_resources()`, que finaliza `insert_stmt` **y** cierra) y limpiar también el camino de
   error de `db_prepare_insert()` (open ok + prepare fallido dejaba la conexión abierta).
   **No hizo falta ninguna supresión de LSan**: con el cierre correcto el stage queda en 0 bytes
   filtrados (el otro leak que aparece en un contenedor Ubuntu 24.04 es de `libliquid` 1.6 en el
   stage de autotune, documentado por el propio runner; el CI usa 1.4.0 y no lo ve).
2. **Warning `-Wunused-result`** en `src/params/db.c`:
   `(void) fchown(tmp_fd, 0, 0)` (glibc marca `fchown` como `warn_unused_result`; el cast a `void` no
   lo silencia). Fix: se maneja el retorno — `EPERM` (build de prueba sin privilegios) no es fatal y
   cualquier otro error se reporta con `LV_LOG_WARN`. `db.c` compila limpio con
   `-Wall -Wextra -Werror` y con `-fanalyzer`.

Evidencia de la verificación (salida real):

- **Stage #213 con ASan+ULSan en el contenedor**: `141 comprobaciones, 0 fallos`, `exit=0`, sin
  reporte de LeakSanitizer.
- **Suite nativa completa con ASan+LSan en el contenedor** (el camino que usa CI, con
  `liquid-dsp 1.4.0` en `/usr/local/lib` como en el job functional): **`runner exit=0`**, log
  completo en [`gates/native-asan-contenedor.txt`](gates/native-asan-contenedor.txt).
  Reproducción: `docker run --rm --entrypoint "" -v <worktree>:/src -v <out>:/out -v <liquid14>:/opt/lq -w /src x6200-uiharness sh -c 'cp -a /opt/lq/lib/libliquid.so* /usr/local/lib/ && ldconfig; sh tests/native/run_native_tests.sh /out/build'`.
  (`liquid-dsp 1.4.0` para x86 se armó en el contenedor `x6200-buildroot` desde el tarball del
  `dl/` de la imagen, con `autoconf`; sin ella, el stage de autotune usa la 1.6 de Ubuntu, que
  filtra 72 B por objeto — leak de la librería, ya documentado en el propio runner.)
- **HOST** (donde ASan no está disponible: el runner lo detecta y omite): `sh tests/native/run_native_tests.sh`
  → **exit 0**; `sh tests/native/run_db_failclosed.sh` → **exit 0**, 11/11; estáticos → OK.
- La medición ARM/QEMU se **repitió** con el commit del fix (binario `d32be99d…`, sello
  `51b92487…`): 12/12 casos OK.

## 8. Pendientes y SKIP declarados

- `[BLOCKED]` **banco QEMU del owner** (`tools/captura-banco.sh --check` dentro de `check-matrix`):
  requiere `../.vfix3` (chroot del rootfs extraído) y el volumen compartido `x6200-build-output`;
  esta corrida usa sólo recursos privados, así que el preflight queda BLOCKED con esa causa. La
  corrida ARM equivalente sí se hizo con un chroot propio (ver §4).
- `PENDING_HARDWARE`: la comprobación real de SD/DATA física, corte de energía y la radio quedan del
  pipeline de QA remoto del owner (fuera de este hijo, igual que en la épica).
- Nota de entorno: la primera corrida de `check-matrix` sobre el commit del fix (1a6bc3bb) falló en el
  gate `x6200-ab contracts` con `OSError: [Errno 122] Disk quota exceeded` (cuota de usuario del tmpfs
  de `/tmp` agotada por el scratch de las corridas, no por el código). Liberado el scratch, la corrida
  repetida da **exit 0** (`gates/check-matrix.txt`). El fallo no se declara PASS: se declara la causa.
- Observación para el orquestador (repo Buildroot, fuera del alcance de #213): el
  `X6200_defconfig` de la imagen no declara `SPEEXDSP`, `LIBDRM` ni `boost`, que la GUI exige
  (§4); un build desde un volumen vacío no llega a configurar el paquete `x6200-gui`.

