# #211 — verificación combinada del hardening P1 (par integrado)

Corrida de **verificación** (no implementación) de la épica
[iariap/x6200_gui#211](https://github.com/iariap/x6200_gui/issues/211) sobre el
**tip integrado** con sus 4 hijos ya fusionados. Contrato: las secciones
*Secuencia vinculante*, *Gates finales* y *Salida del orquestador* del epic; hijos
[#212](https://github.com/iariap/x6200_gui/issues/212) (DATA),
[#213](https://github.com/iariap/x6200_gui/issues/213) (SQLite),
[#214](https://github.com/iariap/x6200_gui/issues/214) (supervisor),
[#215](https://github.com/iariap/x6200_gui/issues/215) (update/backup).

| | |
| --- | --- |
| Fecha | 2026-10-22 |
| Worktree (único escritor) | `state/worktrees/x6200/next-211-combined` — rama `factory/next-211-combined`, **sin push** |
| Base medida | GUI `b798f13b57ae4c6e0ec5a935a9dba2d4b2ca16bd` · Buildroot `e5a6393000b83ce2d9433b3e0560beaf1364536a` (ambos `origin/lu2dsx/x6200-rom`) · LVGL `74d0a816a440eea53e030c4f1af842a94f7ce3d3` (v8.3.11) |
| Alcance | sólo medir y documentar: **no** se modificó código de producción, tests, contratos, budgets ni golden (el árbol del par quedó intacto: `git status` limpio en `src/`, `rootfs/`, `tests/`, `tools/`; `git status` del clon Buildroot vacío) |
| Recursos | **propios y desechables**: clon Buildroot `state/worktrees/x6200/next-211-combined-buildroot`, lab `state/worktrees/x6200/next-211-combined-lab` (árbol buildroot upstream, `dl`, salida, ccache), imagen propia `x6200-uiharness-211combo`, contenedores propios. **No** se usaron `banco-apps`/`banco-audio`/`x6200-build-output`, ni rootfs/volúmenes/contenedores de otras corridas |
| Prohibiciones respetadas | sin SSH a la radio, sin flash, sin deploy, sin TX/RF, sin formatear la SD del owner, sin releases, sin `sudo` interactivo |
| Estado propuesto | **`READY_FOR_REMOTE_QA`** (condicionado, ver §11) — **no** es `GO` de release |

Dossier de escenarios: `docs/ASSESSMENT_ROM_194.md` de la rama
`assessment/rom-194` (leído con `git show` de esa rama, no copiado).

---

## 1. Tabla por hijo (issue / PR / commit / base / resultado)

Los 4 hijos entraron por PR sobre `lu2dsx/x6200-rom`; el "commit fusionado" es el
commit del repo correspondiente que quedó en el tip.

| Hijo | Repo | PR | Commit fusionado (head de rama) | Base del PR | Cambio (`--stat`) | Resultado verificado en el tip |
| --- | --- | --- | --- | --- | --- | --- |
| #212 DATA ext4 (SD nueva / existente) | **Buildroot** | [#4](https://github.com/iariap/AetherX6200Buildroot/pull/4) | `f0ff620a` (head `486ae98f`) | `f23d14f9` | 3 archivos, +1538/−43 (`S01create_data` +347, banco +1135, README +99) | **PASS**: banco 17/17 con el script del repo y **17/17 con el script instalado** (§5, fila #212) |
| #212 evidencia GUI | GUI | [#224](https://github.com/iariap/x6200_gui/pull/224) | `35bad790` (head `109da8cc`) | `d4fb57f8` | 1 archivo (+287, sólo docs) | fusionado (docs) |
| #213 SQLite fail-closed | GUI | [#225](https://github.com/iariap/x6200_gui/pull/225) | `20a78062` (head `e2c321fd`) | `35bad790` | 78 archivos, +7376/−100 | **PASS**: `run_db_failclosed` 11/11, QEMU exit 78 sin SIGSEGV (§5, fila #213) |
| #214 supervisor/backoff | GUI | [#226](https://github.com/iariap/x6200_gui/pull/226) | `d31e6df0` (head `e7926ba9`) | `20a78062` | 14 archivos, +1668/−15 | **PASS**: banco del supervisor 15/15 + QEMU real (§5, fila #214) |
| #215 update verificado + backups (GUI) | GUI | [#229](https://github.com/iariap/x6200_gui/pull/229) | `32f938d9` (head `330816db`) | `a7907eb7` | 134 archivos, +3326/−166 | **PASS**: banco 26 PASS/0 FAIL/1 UNSUPPORTED-UNSAFE_LEGACY (§5, filas #215) |
| #215 update verificado + backups (Buildroot) | **Buildroot** | [#5](https://github.com/iariap/AetherX6200Buildroot/pull/5) | `e5a63930` (head `3344971e`) | `f0ff620a` | 2 archivos, +605/−12 (`S02update` +596) | **PASS**: paridad GUI↔Buildroot (mismo sha256) + `S02update` instalado == fuente (§5, filas #215) |

CI de los PRs (consultado, no re-ejecutado): #225, #226 y #229 → todos los jobs
(`UI Functional Tests`, `UI Quality Contracts`, `UI Visual Regression`, `UI
Performance`, `UI A/B Summary`) **pass**; #224 sólo docs (sin checks). El tip
`b798f13b` tiene la corrida `boot-rootfs` [39814350405](https://github.com/iariap/x6200_gui/actions/runs/39814350405)
**success** (jobs `rootfs prune guards` + `boot anim gate`).

`run_db_failclosed.sh`, `run_daemon_tests.sh`, `test_safe_update.sh`,
`test_S01create_data.sh` y los archivos de runtime de los 4 hijos en el tip son
**byte-idénticos** a los de sus ramas (`git diff <head-de-rama> HEAD -- <paths>`
vacío): los gates focalizados que corren acá son los de los hijos, no una copia.

## 2. Orden real de merges y conflictos

Orden `--first-parent` real en `d4fb57f8..b798f13b` (GUI):

```
35bad790  #224  #212 evidencia (docs)
20a78062  #225  #213 SQLite fail-closed
d31e6df0  #226  #214 supervisor con backoff
00a653e7  #227  bootanim (no-hijo)
c44a1203 252741d9 a262e4f0 76f3f10e a383b191 07ee0808 3793aa83 443d480e   (docs/QA #194/#199)
fa417c1a  QA: ROM fresca 458b1d06 flasheada en la SD
a7907eb7  #230  harness: límites de docker (no-hijo)
32f938d9  #229  #215 update verificado + backups
b798f13b  #228  RX Auto Tune multimode (no-hijo) = tip
```

Buildroot `f23d14f9..e5a63930`:

```
f0ff620a  #4  #212 S01create_data DATA ext4
19b746f2  #6  pulseaudio antes de la GUI (no-hijo)
ec3fa555  #7  quita S50pulseaudio (no-hijo)   <- "Merge pull request #7"
e5a63930  #5  #215 S02update verificado + paridad (tip)
```

**Secuencia vinculante respetada** (DATA → SQLite → Supervisor → Update): el único
orden posible en el log es #212 → #213 → #214 → #215.

**Conflictos: ninguno.** Evidencia: los 4 commits de hijo tienen **un solo padre**
(`git rev-list --parents -n 1` = 2 tokens → squash/rebote lineal sobre el tip
vigente del momento); no quedan marcadores de conflicto (`grep -rn '^<<<<<<<'`
sobre el árbol del par: 0). Los archivos de runtime de #212 (`S01create_data`) y
#215 (`S02update`) son **disjuntos** y ninguno toca `fstab`, así que DATA y update
no compitieron por el mismo path (los respectivos overlays conviven en la rootfs:
verificado en §3 y §7).

Cambios **no-hijo** presentes en el par medido (hecho, no hallazgo): GUI `#227`,
`#228`, `#230` y commits de docs/QA #194/#199; Buildroot `#6`, `#7`.

## 3. Manifiesto único del par GUI+Buildroot

Fuente legible: [`manifiesto/manifiesto.txt`](manifiesto/manifiesto.txt) (lo genera
`manifiesto/manifiesto.sh`, que sólo lee).

| Elemento | Valor |
| --- | --- |
| GUI HEAD medido | `b798f13b57ae4c6e0ec5a935a9dba2d4b2ca16bd` |
| Buildroot HEAD medido | `e5a6393000b83ce2d9433b3e0560beaf1364536a` |
| Buildroot upstream (submódulo) | `cc34b5b2abf2588f0ef55e46a4fe85a60c19ba1f` (árbol `2023.02.11`, el gitlink del repo) |
| LVGL (submódulo GUI) | `74d0a816a440eea53e030c4f1af842a94f7ce3d3` = `v8.3.11` |
| Sello de fuentes GUI (`tools/fuente-md5.sh`) | `1c3577610cdee7059a6d17769fe52e85` |
| Sello embebido en el binario del artefacto | `1c3577610cdee7059a6d17769fe52e85` (**coincide**) |
| `S01create_data` (fuente = instalado) | `044596da2643b6392e63ab95d9a450e42a2151a3852a7bc4804c4cb8dec0a0b7` |
| `S02update` (fuente Buildroot = fuente GUI overlay = instalado) | `1a225ddb0de38ac4552172f4de255358ea274beeb23324694303a3868cd4d65f` |
| `fstab` (fuente = instalado) | `31d158a78754aef665a5c86315c19d79286acaf64951c50be5eebb19714a2ad8` |
| `x6200_daemon` (GUI) | `07c431cceaf28ac3e5b7f630d36c70f7d04b9af3cb19f078c0728c7f53c99877` |
| `x6200_gui` (binario en la rootfs) | `e7776e227b81317e074f0998e26a459784efd47d645293acd88969678da1eb26` (13 428 656 B, ELF32 ARM) |
| Gate de paridad del updater | `check_updater_parity.sh` rc=0 — *mismo archivo, mismo hash* |

Artefactos de la reconstrucción reproducible (salida privada
`next-211-combined-lab/out/images`):

| Artefacto | Bytes | SHA256 |
| --- | ---: | --- |
| `sdcard.img` | 571 473 920 | `f19231aaa186ff48c5326c6ac8abfd66733f60e6df549c72282f09fee50c9456` |
| `rootfs.tar` | 242 821 120 | `ebd02e5a7bec119f4fa81fd05699cdaf73b137a5f766c51988ca54b91cf719de` |
| `rootfs.ext2` (= `rootfs.ext4`) | 536 870 912 | `56136aaeac972e77f7e39db477d1452cd8d40a3106b947f1369856731c6089ed` |
| `boot.vfat` | 33 554 432 | `784ce581a9800995448e69f00d00ff764e8d9328270eb50f8bb73decf3729cd6` |
| `zImage` | 5 541 304 | `24065e8d83de7b0ad6569c3b03761c17cbeca50c7d9d7a1c377658d95e5e44d9` |
| `u-boot-sunxi-with-spl.bin` | 556 112 | `25dc83d5e7085cd8a485599b1c972208fe49af73b98ab2e69e55880cb6064788` |

Otros hashes útiles: fuente del banco `test_S01create_data.sh`
`283ae46897ebe9769bc31102795b8375ec2825cd4d622ca718b82009dd2cfda6`; banco del
supervisor `6e63004a5300da3d6871c7715141a7157d6493bf73ca63ed9d942594039fdc24`;
banco del update `66dbfa208fafb9a0205239f4ce519fed8dedeffa666360cbb8c2eff0eb857fe9`.

Inventario de `etc/init.d` — imagen histórica (`x6200_r1cbu_v0.1.2-lu2dsx.tgz`,
la del owner) **vs** rootfs construida: la única diferencia es que la construida
trae `S01bootanim` (del boot animado, #200/#227) y `S12pulseaudio` en lugar de
`S50pulseaudio` (Buildroot #6/#7); **no aparece ningún servicio de watchdog nuevo**
(dato de F-05) y `S01create_data`/`S02update`/`S20gui` conservan su lugar.

## 4. Gates finales de la épica

Todos con salida real en `logs/`; capa HOST salvo indicación.

| # | Gate (lista textual del epic) | Comando | rc | Resultado | Log |
| --- | --- | --- | --- | --- | --- |
| 4.1 | Reconstrucción reproducible GUI+Buildroot desde fuentes y submódulo correcto | `./br_config.sh && cd build && make -j12` con `X6200_GUI_OVERRIDE_SRCDIR=<worktree GUI>` + `X6200_FUENTE_MD5` | **0** | **PASS** — 1565 etapas de paquete, `make rc=0`, `ExitCode=0`, `OOMKilled=false`; artefactos + SHA256 en §3; sello embebido == sello de fuentes | `logs/gate-buildroot-build-sintesis.log` (log completo 45 MB en el lab: `next-211-combined-lab/logs/build.log`) |
| 4.2 | Suite nativa | `sh tests/native/run_native_tests.sh` | **0** | **PASS** — 47 etapas, cierra con `#213 … 141 comprobaciones, 0 fallos` | `logs/gate-native-tests.log` |
| 4.3 | Paridad con CI (sin submódulo lvgl) | `sh tests/native/run_native_tests.sh --ci-parity` | **0** | **PASS** — `== ci-parity: OK (build host sin lvgl/)`; `lvgl/` restaurado | `logs/gate-native-tests-ci-parity.log` |
| 4.4 | Suite completa | `sh tests/run_all_tests.sh` | **0** | **PASS** — `19 OK, 0 fallos`; **SKIP(77)**: `wspr sstv wefax js8` (la imagen `--target ci` no trae fftw3/sndfile) y `webserver` (no hay checkout) | `logs/gate-run-all-tests.log` |
| 4.4b | variante con imagen *full* para correr los codecs | `IMAGE=x6200-uiharness-full sh tests/run_all_tests.sh` | **0** | **PASS** — `23 OK, 0 fallos` (wspr/sstv/wefax OK, **js8 1994 assertions**), único omitido: `webserver` | `logs/gate-run-all-tests-imagen-full.log` |
| 4.5 | Determinismo del arnés de UI | `sh tests/check_harness_determinism.sh` | **0** | **PASS** — `PASS: deterministic (identical PNGs for every fixture)` | `logs/gate-check-harness-determinism.log` |
| 4.6 | Perfil natural del arnés | `sh tests/check_harness_natural.sh` | **0** | **PASS** — `PASS: natural bench deterministic, profiled and delimited` | `logs/gate-check-harness-natural.log` |
| 4.7 | Waterfall incremental (ASan/UBSan) | `sh tests/check_waterfall_incremental.sh` | **0** | **PASS** — `PASS: 195 checks, 0 fallas` | `logs/gate-check-waterfall-incremental.log` |
| 4.8 | Auditoría de pruebas/documentación | `python3 tests/audita-pruebas.py` | **0** | **OK** — `OK: estructura y evidencia en orden` (237 builds, 14 secciones, 59 enlaces) | `logs/gate-audita-pruebas.log` |
| 4.9 | Identidad del port | `python3 tools/verifica-firmware-port.py <binario>` · `--tar rootfs.tar` | **0** · **0** | **PASS** — `es el firmware del port (ELF32 ARM, 13428656 B, 3/3 marcas)`; idem validando `usr/sbin/x6200_gui` dentro de `rootfs.tar` | `logs/gate-verifica-firmware-port.log` |
| 4.10 | Suites focalizadas de cada hijo | §5 | **0** | **PASS** (todas) | `logs/gate-212-banco-data.log`, `logs/gate-212-banco-instalado.log`, `logs/gate-db-failclosed.log`, `logs/gate-daemon-suite.log`, `logs/gate-release-safe-update.log` |
| 4.11 | Negativos QEMU / rootfs / MBR / fstab / update / arranque GUI en chroot | pasada combinada F-01..F-05 (§6) + bancos #212 y #215 + MBR/fstab/rootfs del artefacto | **0** | **PASS** — 7/7 casos + 100 ciclos; soak parcial (§6.2); MBR del `sdcard.img`: p1 BOOT FAT32 (2048+65536 sectores) + p2 rootfs Linux (67584+1048576), **sin p3** (DATA se crea en el primer boot, diseño de #212) y `fstab` instalado `…/mmcblk0p3 /mnt ext4 rw 0 2` | `logs/`, `foco/`, `logs/gate-mbr-fstab-rootfs.log` |
| 4.12 | Extra (no pedido por el epic): camino de CI con ASan/LSan | `docker run … x6200-uiharness-211combo … sh tests/native/run_native_tests.sh` con libliquid 1.4.0 | **0** | **PASS** — suite completa bajo ASan+LSan sin leak | `logs/extra-native-tests-asan-liquid14.log` |

Método de 4.5–4.7: se construyó una imagen propia `x6200-uiharness-211combo`
(`--target ci`, igual que el CI) para no pisar la etiqueta compartida
`x6200-uiharness` de otras corridas.

### 4.1 Notas de la reconstrucción reproducible

- `make X6200_defconfig` del tip **ya** habilita las dependencias que F-11 daba
  por faltantes (`LIBSNDFILE`, `PULSEAUDIO`, `SPEEXDSP`, `LIBDRM`, `BOOST`,
  `LIQUID_DSP`, `X6200_GUI`): el defecto de F-11 se cerró upstream (Buildroot
  PR #2). No hizo falta tocar el defconfig ni `.config` a mano.
- Se usó el árbol upstream en el gitlink del repo (`cc34b5b2`) y el `dl` de la
  imagen del CI como caché; **6 tarballs no estaban** en esa caché y se bajaron
  de red (`boost 1.80.0`, `espeak 1.48.04`, `libpthread-stubs 0.4`,
  `libdrm 2.4.115`, `speexdsp 1.2.1`, `x6100-webserver`): un build reproducible
  de este par **necesita red** o un `dl` completo (nota para el CI/build oficial).
- Errores **no ignorados** en el log: 0. El log tiene mensajes benignos de
  `texi2dvi` (docs de e2fsprogs) y de `.mo` de glibc, todos marcados `(ignored)`.
- Duración medida: 13:40:41 → 14:36 (≈55 min) con `-j12`, en 16 cores.

### 4.2 Extra: ASan/LSan con la libliquid del CI

El camino de CI (contenedor + ASan/LSan) con la imagen `ci` (Ubuntu 24.04,
**libliquid 1.6**) aborta el stage *Auto Tune v2* con
`LeakSanitizer: 4320 byte(s) leaked in 60 object(s)` y el origen
`qs1dsearch_create (/lib/x86_64-linux-gnu/libliquid.so.1)` — es un leak de la
**librería del contenedor**, no del código del port (el hijo #213 documentó el
mismo artefacto). Repetido con **libliquid 1.4.0** (la del CI, compilada del
tarball del `dl`): **suite completa rc=0, sin leaks** (§4.12). No se declara FAIL
de la ROM por esto; sí es un dato de entorno para el job.

## 5. Suites focalizadas por hijo

| Hijo | Suite | Comando | rc | Resultado |
| --- | --- | --- | --- | --- |
| #212 | Banco DATA ext4 (17 casos, `parted`/`mkfs`/`mount` reales de userland ARM, copia de tarjeta, loop devices en `/dev` privado) | `docker run --rm --privileged -v <clon BR>:/repo:ro -v <lab>:/lab h212-x6200-data-arm bash /repo/br2_external/tests/test_S01create_data.sh` | **0** | **PASS** — `casos: 17, con fallas: 0, checks con FAIL: 0` · `RESULTADO: PASS` (`logs/gate-212-banco-data.log`) |
| #212 (integración) | El mismo banco contra el `S01create_data` **y el `fstab` instalados** en la rootfs construida | `foco/run-212-instalado.sh` | **0** | **PASS** — `instalado == fuente del repo` y `RESULTADO: PASS` (17/17) (`logs/gate-212-banco-instalado.log`) |
| #213 | Persistencia fail-closed con `/mnt` **real** (namespace de montaje/PID privado + chroot) | `sh tests/native/run_db_failclosed.sh` | **0** | **PASS 11/11** (`valid absent absent-no-template corrupt no-mount ro schema-incomplete version-missing v1-atu v1-atu-mixed full`) (`logs/gate-db-failclosed.log`) |
| #214 | Banco del supervisor (wrapper real, chroot privado, reloj virtual, GUI sustituta) | `sh tests/daemon/run_daemon_tests.sh` | **0** | **PASS 15/15** — `all OK: 15 casos, 0 fallos` (`logs/gate-daemon-suite.log`) |
| #215 | Banco hermético del update (contenedor privado + chroot sobre copia del rootfs + BusyBox/bash del target; fixture = `x6200_r1cbu_v0.1.2-lu2dsx.tgz` del owner, sólo lectura) | `sh tests/release/test_safe_update.sh <tgz> <lab>` | **0** | **PASS** — `PASS: 26  FAIL: 0  UNSUPPORTED/UNSAFE_LEGACY: 1` (`legacy-first-update`, por contrato) (`logs/gate-release-safe-update.log`) |
| #215 | Paridad GUI ↔ Buildroot del updater | `BUILDROOT_S02UPDATE=… tools/release/check_updater_parity.sh` | **0** | **PASS** — mismo archivo, mismo sha256 `1a225ddb…` (`manifiesto/manifiesto.txt`) |
| #213/#215 | Stage nativo nuevo del hijo | dentro de `run_native_tests.sh` | **0** | **PASS** — `141 comprobaciones, 0 fallos` (§4, fila 4.2) |

## 6. Pasada combinada F-01..F-05 (capa y cobertura por caso)

Script propio de esta corrida: [`foco/run-f-combined.sh`](foco/run-f-combined.sh)
(host) + [`foco/f-inner.sh`](foco/f-inner.sh) (contenedor). Chroot privado del
rootfs **del par integrado** (binario ARM real, scripts de init instalados),
`qemu-arm` por `binfmt_misc` del host (flag `F`), contenedor privado y desechable
(`--privileged` para poder montar una imagen ext4 con loop; los casos **no**
referencian ningún dispositivo del host y cada log registra qué dispositivos de
bloque ve el contenedor). Matriz cruda: [`foco/matrix.txt`](foco/matrix.txt); identidad del par medida por la pasada: [`foco/pasada-combinada-identidad.txt`](foco/pasada-combinada-identidad.txt).

| Escenario (dossier #194) | Caso | Capa | Cobertura | Medición |
| --- | --- | --- | --- | --- |
| **F-01** DATA en SD nueva | `f01f03-chain` (DATA ext4 nueva montada en `/mnt`) + banco #212 con el script **instalado** | QEMU_USER (+HOST) | **PARTIAL**: es una imagen ext4 propia y una copia de tarjeta, no la eMMC/SD de la radio; kernel del host | **PASS** — DATA sin base → la GUI siembra `params.default.db` (0644, esquema v5) y arranca; el banco crea/valida p3 ext4 con el script instalado |
| **F-02** DB inválida → sin crash-loop | `f02-corrupt`, `f02-nomount`, `f04-daemon-exit78` | QEMU_USER | **PARTIAL** | **PASS** — DB corrupta: `rc=78` + `Persistence unavailable: check DATA and params.db`, bytes intactos, **sin socket**, **sin crash** (0 SIGSEGV); `/mnt` sin montar: `rc=78` + `DATA is not mounted` y **no** crea `/mnt/params.db` sobre el rootfs |
| **F-03** escritura persistente / error que propaga | `f03-absent`, `f03-writefail`, `f01f03-chain` | QEMU_USER | **PARTIAL** | **PASS** — siembra atómica desde la plantilla y arranque; `/mnt` lleno: `Persistence write failed` y la base existente sigue `integrity_check=ok` con la GUI viva; cambio de frecuencia persistido y **leído de nuevo tras “reinicio” (remontaje del ext4)** |
| **F-04** supervisor con backoff y logs únicos | `f04-daemon-exit78`, `f04-daemon-nomnt` | QEMU_USER | **PARTIAL** | **PASS** — 1 solo `GUI exited: code 78` y `GUI restart in 60s` en ~11 s (pre-fix: 34 arranques/10 s), log de corrida en `/mnt/app_logs` con nombre único `x6200_log_<UTC>_p<pid>_r1.txt`, SIGTERM ⇒ 0 GUI vivas; `/mnt` ausente ⇒ **0 archivos** en el rootfs y log retenido en `/tmp` con diagnóstico |
| **F-04/F-05** update verificado con backup completo | banco #215 (§5, filas #215) + paridad + hash del `S02update` instalado | HOST/QEMU_USER | **PARTIAL** | **PASS** — 26 casos OK (sidecar/hash/truncado/preflight/backup verificado/`tar` que falla ⇒ paquete retenido/`/mnt` RO o ausente/uid 1000/setuid exacto). El primer update de una **imagen histórica** queda **`UNSUPPORTED/UNSAFE_LEGACY`** por contrato (no es PASS) |
| **F-05** watchdog del SoC | inventario de `init.d` (built vs histórica) + backoff medido | HOST (+QEMU_USER) | **UNSUPPORTED** para el WDT de kernel (no se tocó; no se declara que no exista) | La imagen no agrega **ningún** servicio de watchdog; la mitigación del lazo es el supervisor (#214), medida arriba |

Casos extra de la pasada:

- `f-cycles` — 100 órdenes de frecuencia por el socket sobre el par integrado:
  **PASS** (0 errores de socket, sigue contestando, `integrity_check=ok`, sin crash).
- `f-soak` — soak de GUI viva sin radio, muestras de RSS/hilos por minuto en
  [`logs/f-soak.log`](logs/f-soak.log). Estado al cerrar: **ver §11**.

### 6.1 Interacción medida (lo que ningún hijo probó por separado)

1. **`exit(78)` de #213 × backoff de #214** — medido en `f04-daemon-exit78`
   (antes: el banco de #214 usó una GUI **sustituta** que salía 78; el de #213
   nunca corrió el supervisor). Resultado: el supervisor del artefacto real
   informa `GUI exited: code 78` y aplica **60 s desde el primer fallo**, un solo
   arranque en ~11 s, log a DATA y parada limpia con SIGTERM.
2. **DATA nueva → siembra → persistencia** (`f01f03-chain`): la cadena
   F-01→F-03 con el binario y los scripts **del artefacto**: la GUI siembra la
   plantilla en la DATA recién creada, persiste el cambio y lo recupera tras el
   remontaje, sin volver a sembrar.
3. **DATA (#212) ↔ update (#215)**: archivos disjuntos; el banco del update corre
   sobre un árbol con el `S01create_data` nuevo y con `/mnt` protegido: el update
   **no** toca DATA y el contrato del update exige `/mnt` montado y `rw` (medido
   en `readonly-mnt`/`mnt-not-mounted`).
4. **`fstab` ↔ `S01create_data` ↔ guarda de la GUI**: el `fstab` de la imagen
   construida y el script instalado son los del repo (§3) y la guarda de la GUI
   (que exige `/mnt` montado de otro filesystem y `rw`) acepta la DATA que el
   par monta (`f01f03-chain`, `f03-absent`).
5. **Herramientas del target que el supervisor necesita**: `mountpoint`,
   `mkfs.ext4`, `parted` **existen** en la rootfs del par
   (`/bin/mountpoint`, `/sbin/mkfs.ext4`, `/usr/sbin/parted`), así que la rama
   "logs a DATA" de `dest_ready()` funciona en la imagen real (queda cubierto el
   doble que usó el banco de #214).

### 6.2 Soak y límites de ventana

El soak (`f-soak`, objetivo 60 min) se lanzó como caso aparte con el mismo chroot
y muestrea por minuto la GUI viva (`logs/f-soak.log`). **Medido: 22 minutos**
(1–22), con `RSS` **plano en 60 512 kB** todo el tramo (sin crecimiento), 12 hilos
hasta el minuto 19 y 11 desde el 20 (un hilo de trabajo terminó; `RSS` sin
cambio), sin crash y sin re-siembra. El tramo restante hasta los 60 min **no se
completó en la ventana de esta corrida** y se declara **PENDING** con esa razón
(el caso no alcanza a imprimir su veredicto final; las muestras crudas quedan como
evidencia parcial). No se declara PASS de soak completo.

## 7. Coherencia de la integración (observaciones del tip)

- `S20gui` (no tocado por los hijos, por contrato) sigue haciendo
  `mkdir -p /mnt/app_logs`, `cp /usr/share/x6200/params.default.db /mnt/params.db`
  si falta y `killall x6200_gui` en `stop()`: **son rutas preexistentes**. Si DATA
  no estuviera montada, ese `mkdir`/`cp` escribe sobre el rootfs — hallazgo
  **preexistente**, fuera del alcance de los 4 hijos y **no medido** en esta
  corrida (los casos ejercitan al supervisor y a la GUI, no el init). Queda
  anotado como riesgo residual (§9) y como candidato del epic siguiente.
- La guarda de arranque de la GUI (#213) es la que decide: con `/mnt` sin montar
  el proceso sale 78 antes de cualquier escritura (medido), así que la ruta de
  `S20gui` no llega a ejecutarse si la DATA no está.
- `params_init()` cambió de firma (`bool`): el mock del harness
  (`tools/ui_harness/mocks/mock_params.c`) ya está adaptado en el tip; el gate
  `x6200-ab contracts` del CI pasa (PR #225) y la suite del arnés pasa acá
  (§4.5–4.7).

## 8. Huecos declarados (no maquillados)

- **HARDWARE**: todo lo físico queda `PENDING_HARDWARE` (§10).
- **`webserver`** (stage de `run_all_tests.sh`): **SKIP** — no hay checkout
  `.x6200_webserver`; además el stage necesita el volumen compartido
  `x6200-build-output`, que esta corrida **no** puede usar. Nunca contado como PASS.
- **Banco QEMU del owner** (`tools/captura-banco.sh`): **BLOCKED** en esta corrida
  (requiere `x6200-build-output` y el chroot `.vfix3` ajenos). Equivalente QEMU
  propio sí corrido (§6).
- **Soak**: ver §11 (ventana).
- **Cortes de energía** en medio de la extracción del update: no probados (el
  banco usa un fixture que fuerza el error de `tar`, sin corte real).
- La reconstrucción necesitó red para 6 tarballs (§4.1).

## 9. Riesgos residuales

1. El **par medido** incluye cambios no-hijo (§1, §2): los gates miden el par, no
   cada hijo aislado.
2. `S20gui` puede escribir `boot.log`/`params.db` sobre el rootfs si DATA no está
   montada (§7): preexistente, no medido acá.
3. El **primer update in-place** de una imagen histórica sigue siendo
   `UNSUPPORTED/UNSAFE_LEGACY`: la migración recomendada es reimagen de SD con
   backup/restauración manual.
4. Corte de energía durante la extracción del update: **sin** recuperación
   garantizada (no hay A/B ni journal).
5. El pasaje a **Cortex-A7/LCD/RF/energía** no está medido: los PASS son de
   HOST/QEMU_USER.
6. Leak de `libliquid 1.6` del contenedor Ubuntu 24.04 en el stage de Auto Tune v2
   bajo LSan (§4.2): ruido de entorno del job `ci`, no del código.
7. Los artefactos reconstruidos **no** se compararon byte a byte contra la ROM
   publicada (`458b1d06`): son de fuentes distintas (el par trae 4 hijos +
   cambios no-hijo). No hay afirmación de reproducibilidad bit-exacta.

## 10. Huecos HARDWARE y plan mínimo de QA físico

Estado de esta campaña: **HOST + QEMU_USER**. No arranca kernel/init real, no hay
RF, no hay SD/eMMC física, no hay cortes de energía. Plan mínimo (lo ejecuta el
pipeline de QA físico separado, con autorización, ventana exclusiva y SD de
prueba — nunca la del owner):

1. **F-01 SD virgen** (HW-01): flashear el `sdcard.img` reconstruido
   (`f19231aa…`) en SD de prueba, primer arranque, `blkid -L DATA` + `findmnt /mnt`
   (p3 `ext4`, `rw`), hashes de p1/p2 sin cambios, segundo arranque idempotente.
2. **DATA existente** (HW-02): con una DATA con `params.db` + `app_logs/`, arrancar
   y comprobar contenido, owners y symlinks intactos (`DATA ready (ext4)` /
   `(vfat legacy)`).
3. **F-02/F-04 con DB corrupta** (HW-03): corromper una **copia** de `params.db`,
   arrancar: GUI sale 78 sin SIGSEGV, el supervisor no entra en lazo (≤3
   arranques/10 s; 60 s desde el primer fallo) y el log llega a `/mnt/app_logs`.
4. **F-04/F-05 update real** (HW-04): copiar `x6200_r1cbu_*.tgz` + `.sha256` a
   DATA, correr el update en la ventana autorizada; verificar `Update applied`,
   backup completo y lista de rutas nuevas. **Sin** cortes de energía.
5. **Primer update de imagen histórica** (HW-05): sólo reimagen de SD de
   laboratorio + backup/restauración manual autorizados (nunca in-place).
6. **Fuera de alcance de la épica**: watchdog del SoC, corte de energía real,
   térmica, audio analógico, LCD, TX/RF (los hijos no los tocaron).

## 11. Estado propuesto

**Todos los gates factibles de la lista del epic están aprobados** (§4, §5, §6):
reconstrucción reproducible desde fuentes con el submódulo correcto (GUI+Buildroot,
sello de fuentes embebido == sello del árbol), suite nativa + paridad CI, suite
completa (con y sin imagen *full*), los tres gates del arnés, `audita-pruebas`,
el gate de identidad del port (binario y `rootfs.tar`), las suites focalizadas de
los 4 hijos sobre el tip y la pasada combinada F-01..F-05 (7/7, +100 ciclos), con
la interacción `exit(78)`×backoff medida sobre el artefacto real.

Por eso se propone **`READY_FOR_REMOTE_QA`** para el par
`GUI b798f13b + Buildroot e5a63930`, con estas condiciones declaradas:

- **No** es `GO` de release: no hay validación en radio, ni cortes de energía, ni
  update in-place (esos siguen `PENDING_HARDWARE`, §10).
- **Soak parcial**: se midieron 22 de los 60 minutos (§6.2) con `RSS` plano y sin
  crash; el tramo restante queda **PENDING** por ventana de la corrida. Es el
  único punto de la lista del epic que no se completó.
- El primer update de una imagen histórica se mantiene
  **`UNSUPPORTED/UNSAFE_LEGACY`**: el QA remoto debe usar reimagen de SD de
  laboratorio, no update in-place.
- El QA remoto debe usar el **SHA del artefacto** (§3), no un build “del tip”:
  cualquier rebase posterior invalida esta evidencia.
- La revisión de los PRs por el owner ya ocurrió (los 4 PRs están mergeados, §1);
  esta corrida agrega la verificación combinada, con la evidencia en este
  directorio (commits locales, **sin push**).
