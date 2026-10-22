# #211 — verificación combinada del hardening P1 (par integrado)

Corrida de **verificación** (no implementación) de la épica
[iariap/x6200_gui#211](https://github.com/iariap/x6200_gui/issues/211) sobre el
**tip integrado** con sus 4 hijos ya fusionados. Contrato de la corrida: las
secciones *Secuencia vinculante*, *Gates finales* y *Salida del orquestador* del
epic; hijos [#212](https://github.com/iariap/x6200_gui/issues/212),
[#213](https://github.com/iariap/x6200_gui/issues/213),
[#214](https://github.com/iariap/x6200_gui/issues/214),
[#215](https://github.com/iariap/x6200_gui/issues/215).

| | |
| --- | --- |
| Fecha | 2026-10-22 |
| Worktree (único escritor) | `/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined` — rama `factory/next-211-combined`, **sin push** |
| Base medida | GUI `b798f13b57ae4c6e0ec5a935a9dba2d4b2ca16bd` (`origin/lu2dsx/x6200-rom`) · Buildroot `e5a6393000b83ce2d9433b3e0560beaf1364536a` (`origin/lu2dsx/x6200-rom`) · LVGL gitlink `74d0a816a440eea53e030c4f1af842a94f7ce3d3` (v8.3.11) |
| Alcance | sólo medir y documentar: **no** se modificó código de producción, tests, contratos, budgets ni golden |
| Recursos | propios y desechables: clon Buildroot `state/worktrees/x6200/next-211-combined-buildroot`, lab `state/worktrees/x6200/next-211-combined-lab` (árbol buildroot, `dl` propio, salida, ccache), imágenes docker propias (`x6200-uiharness-211combo`); **no** se usaron `banco-apps`/`banco-audio`/`x6200-build-output`, ni rootfs/volúmenes/contenedores de otras corridas |
| Prohibiciones respetadas | sin SSH a la radio, sin flash, sin deploy, sin TX/RF, sin formatear la SD del owner, sin releases, sin `sudo` interactivo |
| Estado propuesto | ver §11 — **no** es `GO` de release |

Dossier de escenarios: `docs/ASSESSMENT_ROM_194.md` de la rama
`assessment/rom-194` (leído de ahí con `git show`, no copiado).

---

## 1. Tabla por hijo (issue / PR / commit / base / resultado)

Rama y commits: los 4 hijos entraron **squash/merge-lineal** sobre
`lu2dsx/x6200-rom`; el "commit del hijo" es el commit fusionado del repo
correspondiente.

| Hijo | Repo | PR | Commit fusionado (head de rama) | Base del PR | Cambio medido (`--stat`) | Resultado en el tip integrado |
| --- | --- | --- | --- | --- | --- | --- |
| #212 DATA ext4 (SD nueva / DATA existente) | **Buildroot** | [#4](https://github.com/iariap/AetherX6200Buildroot/pull/4) | `f0ff620a` (head `486ae98f`) | `f23d14f9` | 3 archivos, +1538/−43: `S01create_data` +347, `br2_external/tests/test_S01create_data.sh` (banco) +1135, `tests/README.md` +99 | fusionado e integrado (banco releído en §5.1) |
| #212 evidencia GUI | GUI | [#224](https://github.com/iariap/x6200_gui/pull/224) | `35bad790` (head `109da8cc`) | `d4fb57f8` | `docs/evidencia/2026-10-22-hardening212/README.md` +287 (sólo docs) | fusionado |
| #213 SQLite fail-closed | GUI | [#225](https://github.com/iariap/x6200_gui/pull/225) | `20a78062` (head `e2c321fd`) | `35bad790` | 78 archivos, +7376/−100 (`src/params/*`, `src/cfg/cfg.c`, `src/main.c`, suites nativas) | fusionado e integrado (§5.2) |
| #214 supervisor/backoff | GUI | [#226](https://github.com/iariap/x6200_gui/pull/226) | `d31e6df0` (head `e7926ba9`) | `20a78062` | 14 archivos, +1668/−15 (`rootfs/usr/sbin/x6200_daemon` + `tests/daemon/run_daemon_tests.sh`) | fusionado e integrado (§5.3) |
| #215 update verificado + backups (GUI) | GUI | [#229](https://github.com/iariap/x6200_gui/pull/229) | `32f938d9` (head `330816db`) | `a7907eb7` | 134 archivos, +3326/−166 (`tools/release/**`, `tests/release/**`, `docs/TESTING.md`) | fusionado e integrado (§5.4) |
| #215 update verificado + backups (Buildroot) | **Buildroot** | [#5](https://github.com/iariap/AetherX6200Buildroot/pull/5) | `e5a63930` (head `3344971e`) | `f0ff620a` | 2 archivos, +605/−12: `S02update` +596, `tests/README.md` +21 | fusionado e integrado (§5.4) |

Base de trabajo de los 4 hijos: `d4fb57f8` (GUI) y `f23d14f9` (Buildroot), como
declara el epic. Los hijos que tocaron los dos repos (#215) tienen **un PR por
repo** y su matriz de par de commits: `e5a63930` (Buildroot) + `32f938d9` (GUI).

Cambios **no-hijo** presentes en el tip (hecho, no hallazgo): GUI `#227`
(bootanim), `#228` (RX Auto Tune, tip actual), `#230` (límites de docker del
harness) y commits de docs/QA de la campaña #194/#199; Buildroot `#6`/`#7`
(orden de pulseaudio en el boot). El par integrado medido los incluye.

## 2. Orden real de merges y conflictos

Orden `--first-parent` real en `d4fb57f8..b798f13b` (GUI) — la **secuencia
vinculante del epic se respetó** (DATA → SQLite → Supervisor → Update):

```
35bad790  #224  #212 evidencia (docs)
20a78062  #225  #213 SQLite fail-closed
d31e6df0  #226  #214 supervisor con backoff
00a653e7  #227  bootanim (no-hijo)
c44a1203 252741d9 a262e4f0 76f3f10e a383b191 07ee0808 3793aa83 443d480e  (docs/QA #194/#199)
fa417c1a  QA: ROM fresca flasheada en la SD (f0ff620a)
a7907eb7  #230  harness docker caps (no-hijo)
32f938d9  #229  #215 update + backups
b798f13b  #228  RX Auto Tune (no-hijo) = tip
```

Buildroot `f23d14f9..e5a63930`:

```
f0ff620a  #4  #212 S01create_data DATA ext4
19b746f2  #6  pulseaudio antes de la GUI (no-hijo)
ec3fa555  #7  quita S50pulseaudio (no-hijo)
e5a63930  #5  #215 S02update verificado + paridad (tip)
```

**Conflictos: ninguno.** Evidencia: cada uno de los 4 commits de hijo tiene
**un solo padre** (`git rev-list --parents -n 1` = 2 tokens → squash/rebote
lineal sobre el tip vigente del momento), y ningún archivo quedó con marcadores
de conflicto en el tip (`grep -rn '^<<<<<<<'` sobre el árbol: 0). Los archivos de
#212 (`S01create_data`) y #215 (`S02update`) son **disjuntos** entre sí y
respecto de `fstab`, así que el par DATA↔update no compitió por el mismo path.

## 3. Manifiesto único del par GUI+Buildroot

*(los SHA256 de los artefactos se completan cuando termina la reconstrucción;
ver §4.1 y `manifiesto/`)*

| Elemento | Valor |
| --- | --- |
| GUI HEAD medido | `b798f13b57ae4c6e0ec5a935a9dba2d4b2ca16bd` |
| Buildroot HEAD medido | `e5a6393000b83ce2d9433b3e0560beaf1364536a` |
| Builtroot submodule gitlink (fuente upstream) | `cc34b5b2abf2588f0ef55e46a4fe85a60c19ba1f` (buildroot 2022.11.x → árbol `2023.02.11`; ver §4.1) |
| LVGL gitlink (GUI) | `74d0a816a440eea53e030c4f1af842a94f7ce3d3` = `v8.3.11` (`git submodule status` limpio) |
| Sello de fuentes `tools/fuente-md5.sh` (GUI) | `1c3577610cdee7059a6d17769fe52e85` |
| `S01create_data` (Buildroot, fuente) | ver `manifiesto/sha256-fuentes.txt` |
| `S02update` GUI ↔ Buildroot | idénticos byte a byte (gate de paridad `tools/release/check_updater_parity.sh`) |
| Artefactos de la reconstrucción | **PENDING** — en curso (§4.1) |

## 4. Gates finales de la épica

Capa **HOST** salvo indicación; todos con salida real en `logs/`.

| # | Gate (lista textual del epic) | Comando | rc | Resultado | Log |
| --- | --- | --- | --- | --- | --- |
| 4.1 | Reconstrucción reproducible GUI+Buildroot desde fuentes y submódulo correcto | build privado con `X6200_defconfig` + override de fuentes GUI + `X6200_FUENTE_MD5` | – | **EN CURSO** (ver §4.1) | `logs/buildroot-build.log` |
| 4.2 | Suite nativa | `sh tests/native/run_native_tests.sh` | – | **EN CURSO** | `logs/gate-native-tests.log` |
| 4.3 | Paridad con CI (sin submódulo lvgl) | `sh tests/native/run_native_tests.sh --ci-parity` | – | **PENDING** (espera a que el build haya copiado las fuentes GUI: no se oculta `lvgl/` mientras el build lo lee) | — |
| 4.4 | Suite completa | `sh tests/run_all_tests.sh` | – | **PENDING** (incluye el stage `host_ci_parity`: mismo motivo que 4.3) | — |
| 4.5 | Determinismo del arnés de UI | `sh tests/check_harness_determinism.sh` | 0 | **PASS** — `PASS: deterministic (identical PNGs for every fixture)` | `logs/gate-check-harness-determinism.log` |
| 4.6 | Perfil natural del arnés | `sh tests/check_harness_natural.sh` | 0 | **PASS** — `PASS: natural bench deterministic, profiled and delimited (main-natural)` | `logs/gate-check-harness-natural.log` |
| 4.7 | Waterfall incremental (ASan/UBSan) | `sh tests/check_waterfall_incremental.sh` | 0 | **PASS** — `PASS: 195 checks, 0 fallas` | `logs/gate-check-waterfall-incremental.log` |
| 4.8 | Auditoría de pruebas/documentación | `python3 tests/audita-pruebas.py` | 0 | **OK** — `OK: estructura y evidencia en orden` (237 builds, 14 secciones, 59 enlaces; 32 ítems para revisar a mano, sin fallo) | `logs/gate-audita-pruebas.log` |
| 4.9 | Gate de identidad del port | `python3 tools/verifica-firmware-port.py <binario ARM>` | – | **PENDING** (necesita el binario de la reconstrucción) | — |
| 4.10 | Suites focalizadas de cada hijo | ver §5 | – | **parcial** (§5) | `logs/gate-*.log` |
| 4.11 | Negativos QEMU / rootfs / MBR / fstab / update / arranque GUI en chroot | pasada combinada F-01..F-05 (§6) + banco #212 + banco #215 | – | **EN CURSO / PENDING** | `logs/`, `foco/` |

Nota de método para 4.5–4.7: se construyó una imagen propia
`x6200-uiharness-211combo` (`--target ci`, igual que el CI) para no pisar la
etiqueta compartida `x6200-uiharness` que usan otras corridas.

### 4.1 Reconstrucción reproducible (en curso)

Receta usada (todo privado; nada compartido con otras corridas):

```
clon buildroot upstream en el gitlink del repo   : cc34b5b2 (== árbol que trae la imagen del CI)
copia propia del dl de la imagen                 : state/worktrees/x6200/next-211-combined-lab/dl (169 entradas, 5.4 G)
salida y ccache privados                         : next-211-combined-lab/{out,ccache}
./br_config.sh && cd build && make -j12
  con  X6200_GUI_OVERRIDE_SRCDIR=/workspace_gui (worktree GUI del tip, :ro)
       X6200_FUENTE_MD5=1c3577610cdee7059a6d17769fe52e85
```

Verificación previa ya hecha: `make X6200_defconfig` en el tip **sí** habilita las
dependencias que F-11 daba por faltantes (`BR2_PACKAGE_LIBSNDFILE`,
`BR2_PACKAGE_PULSEAUDIO`, `BR2_PACKAGE_SPEEXDSP`, `BR2_PACKAGE_LIBDRM`,
`BR2_PACKAGE_BOOST`, `BR2_PACKAGE_LIQUID_DSP`, `BR2_PACKAGE_X6200_GUI`), así que
la reconstrucción desde el defconfig del tip es la esperada (el defecto de F-11
se cerró upstream en Buildroot PR #2). El build en sí seguía corriendo al cerrar
esta sección: **no se declara PASS sin artefacto**.

## 5. Suites focalizadas por hijo

| Hijo | Suite | Comando | rc | Resultado |
| --- | --- | --- | --- | --- |
| #212 | Banco DATA ext4 (Buildroot) — 17 casos sobre copia de tarjeta con `parted`/`mkfs` reales de userland ARM | `docker run --rm --privileged -v <clon BR>:/repo:ro -v <lab>:/lab h212-x6200-data-arm bash /repo/br2_external/tests/test_S01create_data.sh` | – | **EN CURSO** (`logs/gate-212-banco-data.log`) |
| #213 | Persistencia fail-closed con `/mnt` **real** (namespace+chroot privados) | `sh tests/native/run_db_failclosed.sh` | 0 | **PASS 11/11 casos** (`valid absent absent-no-template corrupt no-mount ro schema-incomplete version-missing v1-atu v1-atu-mixed full`) — `logs/gate-db-failclosed.log` |
| #213 | Suites nativas nuevas del hijo | incluidas en `sh tests/native/run_native_tests.sh` | – | **EN CURSO** (stage con 141 comprobaciones, 0 fallos hasta ahora) |
| #214 | Banco del supervisor (wrapper real en chroot privado, reloj virtual) | `sh tests/daemon/run_daemon_tests.sh` | 0 | **PASS 15/15 casos** — `all OK: 15 casos, 0 fallos` (`logs/gate-daemon-suite.log`) |
| #215 | Banco hermético del camino de update (contenedor privado + chroot sobre copia del rootfs + BusyBox/bash del target) | `sh tests/release/test_safe_update.sh <tgz publicado> <lab>` | – | **EN CURSO** (`logs/gate-release-safe-update.log`) |
| #215 | Paridad de updater GUI ↔ Buildroot | `tools/release/check_updater_parity.sh` | – | **PENDING** (§5.4) |

## 6. Pasada combinada F-01..F-05 (capa + cobertura por caso)

*(script propio de esta corrida: `foco/run-f-combined.sh` + `foco/f-inner.sh`;
en curso/PENDING según §6.1)*

| Escenario (dossier #194) | Caso de la pasada | Capa | Cobertura | Resultado |
| --- | --- | --- | --- | --- |
| F-01 DATA en SD nueva / existente | `f01f03-chain` (DATA nueva) + banco #212 con el script **instalado** (§5.1) | QEMU_USER + HOST | PARTIAL (no es la eMMC/SD de la radio; kernel del host) | EN CURSO |
| F-02 DB inválida → sin crash-loop | `f02-corrupt`, `f02-nomount` (GUI sale 78) + `f04-daemon-exit78` (supervisor) | QEMU_USER | PARTIAL | EN CURSO |
| F-03 escritura persistente / error que propaga | `f03-absent`, `f03-writefail`, `f01f03-chain` (persistencia tras "reinicio") | QEMU_USER | PARTIAL | EN CURSO |
| F-04 supervisor con backoff y logs únicos | `f04-daemon-exit78`, `f04-daemon-nomnt` | QEMU_USER | PARTIAL | EN CURSO |
| F-04/F-05 update verificado con backup completo | banco #215 (§5) + paridad de updater + hash del `S02update` instalado | HOST/QEMU_USER | PARTIAL (el primer update de una imagen histórica queda UNSUPPORTED/UNSAFE_LEGACY por contrato) | EN CURSO |
| F-05 watchdog del SoC | fuera de alcance de los 4 hijos (no se tocó WDT de kernel; #214 es supervisión del proceso) | — | UNSUPPORTED en esta campaña | — |

Sin generalizar: ningún caso de esta pasada mide radio física, cortes de energía
ni el update real in-place; eso queda `PENDING_HARDWARE` (§9).

## 7. Coherencia de la integración e interacciones

*(se completa con las mediciones; hipótesis a medir listadas)*

- `S01create_data` (#212) ↔ `fstab` ↔ GUI (#213): la GUI exige `/mnt` montada de
  otro filesystem y `rw`; el `fstab` del tip declara `ext4` y el script nuevo
  monta la DATA existente con su tipo real. Medición: `f01f03-chain`.
- `exit(78)` de #213 ↔ backoff de #214: interacción que **ninguno de los dos
  hijos probó por separado** (el banco de #214 usó una GUI sustituta con
  `exit:78`; el de #213 no corre el supervisor). Medición: `f04-daemon-exit78`.
- `S01create_data` (#212) ↔ `S02update` (#215): archivos disjuntos; el update no
  toca DATA y el contrato de #212 prohíbe reformatear. Medición: el banco #215
  corre sobre un rootfs con el `S02update` nuevo y el `S01create_data` nuevo.
- Dependencia de herramientas del target en el supervisor (#214): `dest_ready()`
  usa `mountpoint -q` y `prepare_images()` usa `mountpoint`/`mount --bind`; el
  banco del hijo corrió con **dobles** de `mountpoint`. Medición: `f04-*` sobre el
  rootfs real del tip (se registra si `mountpoint` existe en la imagen).

## 8. Riesgos residuales

*(se completa al cerrar; adelanto de los que ya se ven)*

- El par integrado incluye cambios **no-hijo** (GUI #227/#228/#230, Buildroot
  #6/#7): los gates miden el par, no cada hijo aislado.
- Reconstrucción desde fuentes con `dl` copiado de la imagen del CI: si algún
  tarball no estuviera en el `dl`, el build lo baja de red; se registra qué se
  bajó, si pasa.
- Requisito de `mountpoint` en el rootfs real para la rama "logs a DATA" de #214
  (§7, en medición).
- Corte de energía en medio de la extracción del update: **no** garantiza
  recuperación (contrato #215 §7), y no se probó con cortes reales.
- `READY_FOR_REMOTE_QA` no implica que el update in-place de una imagen histórica
  sea seguro (`UNSUPPORTED/UNSAFE_LEGACY`).

## 9. Huecos HARDWARE y plan mínimo de QA físico

Toda la evidencia de esta corrida es HOST/QEMU_USER: **no** arranca kernel/init
real, no hay RF, no hay eMMC/SD física, no hay cortes de energía. Estado
`PENDING_HARDWARE` para:

1. **F-01 en SD virgen real** (HW-01): flashear la imagen reconstruida en una SD
   de prueba (no la del owner), primer arranque, `blkid -L DATA` + `findmnt /mnt`,
   verificar p3 `ext4` montada `rw` y que p1/p2 no cambiaron; segundo arranque
   idempotente.
2. **DATA existente preservada** (HW-02): con una DATA con contenido (params.db,
   `app_logs/`), arrancar y comprobar hashes/owners/symlinks intactos.
3. **F-02/F-04 con DB corrupta** (HW-03): corromper una copia de `params.db` en
   DATA de prueba, arrancar: la GUI debe salir 78 sin SIGSEGV, el supervisor no
   debe entrar en lazo rápido (≤3 arranques/10 s) y el log debe llegar a DATA.
4. **F-04/F-05 update real** (HW-04/HW-06): copiar tgz + `<tgz>.sha256` a DATA,
   correr el update con la radio en la ventana autorizada, verificar backup
   completo y `Update applied`; **no** hacer cortes de energía.
5. **Primer update de una imagen histórica** (HW-05): sólo con reimagen de SD de
   laboratorio y backup/restauración manual autorizados (nunca in-place).
6. **Watchdog del SoC / corte de energía / térmica / audio / LCD / TX**: fuera de
   alcance de la épica; ningún hijo los tocó, siguen como hipótesis.

Prerrequisitos: autorización escrita, ventana exclusiva, SD(s) de prueba,
backup/restauración de DATA, instrumentos si se midiera energía. La ejecuta el
**pipeline de QA físico separado**, no esta campaña.

## 10. Estado propuesto

*(se cierra al terminar §4/§5/§6; criterio del epic: `READY_FOR_REMOTE_QA` sólo
si **todos** los gates factibles están aprobados; nunca `GO` de release)*
