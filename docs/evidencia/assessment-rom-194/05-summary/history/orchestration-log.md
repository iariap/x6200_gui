# Campaña assessment pre-release ROM #194 — estado de orquestación

Owner: "quiero hacer un assessment del equipo". Epic: https://github.com/iariap/x6200_gui/issues/194
**Monitoreo programado de la radio: DESACTIVADO** durante la campaña (pedido del owner).

## Base congelada
- GUI candidato: `c888308eede71205605c06383432ee7e165f202e` (rama lu2dsx/x6200-rom)
- Buildroot candidato: `d084e2870a00b64d9c57474e7eceeef80f43f25b` (iariap/AetherX6200Buildroot, rama lu2dsx/x6200-rom)
- LVGL: gitlink del candidato = `74d0a816a440eea53e030c4f1af842a94f7ce3d3`
- Divergencia al iniciar (21/10 15:20 -03): tip de la rama = `90aebad5` = candidato + commits SOLO de documentación
  (QA del otro agente y propios). No hay cambios de producción: candidato sigue válido.

## Reglas (de la épica)
- Ejecución **estrictamente secuencial**: #195 -> #196 -> #197 -> #198 -> #199. Sin agentes ni bancos en paralelo.
- Un clon privado por ejecutor; nunca tocar checkouts/volúmenes/rootfs/contenedores/radio de otra sesión.
- Clon de medición limpio en el SHA congelado; el reporte va en worktree y rama aparte.
- Prohibido: modificar src/, rootfs/, sql/, lvgl/, tests/harness, CI, budgets, golden, excepciones; deploy/flash/tags/release/RF/cortes de energía.
- Bancos `banco-apps` / `banco-audio`: si están ocupados -> BLOCKED, no matar contenedores ajenos.
- Volumen compartido `x6200-build-output`: NO usarlo como medición (webserver SKIP con WS a ruta inexistente).
- Radio: #195-#198 NO tocan radio. #199 tiene fase física remota RX con sesión exclusiva.
- Un PR de documentación por unidad a `assessment/rom-194`. Sin auto-merge. PR final a lu2dsx/x6200-rom con revisión del maintainer.

## Directorios de trabajo
- Base: `/home/pablo/desa/x6200/assessment-194/`
- Clon GUI privado: `assessment-194/clone-gui` (origin = https://github.com/iariap/x6200_gui.git)
- Clon Buildroot privado: `assessment-194/clone-buildroot` (origin = https://github.com/iariap/AetherX6200Buildroot.git)
- Scratch: `assessment-194/scratch` · Build: `assessment-194/build`
- Worktree de reporte: `assessment-194/wt-assessment-<N>` (rama assessment/<N>-evidence)

## Estado de las unidades (actualizado 21/10 18:37 -03)

| Unidad | Rama | Worktree | PR | Estado |
|---|---|---|---|---|
| **#195 baseline** | assessment/195-evidence | wt-assessment-195 | **#201 MERGEADO** (merge 230b2063, commit informe be6088d1) | ✅ **COMPLETADA** |
| **#196 ROM/confiabilidad** | assessment/196-evidence | wt-assessment-196 | (sin PR aún) | 🔄 **EN EJECUCIÓN** (QEMU + negativas activos) |
| #197 UI/UX | assessment/197-evidence | wt-assessment-197 | — | ⏳ Bloqueada por #196 |
| #198 performance | assessment/198-evidence | wt-assessment-198 | — | ⏳ Bloqueada |
| #199 consolidación + QA físico | assessment/199-evidence | wt-assessment-199 | — | ⏳ Bloqueada |
| Integración final | assessment/rom-194 -> lu2dsx/x6200-rom | — | — | ⏳ Pendiente |

### Artefacto del candidato (de #195, elegido para #196-#199)
- sdcard.img sha256 d0ff4435ad1d068e25bf2c357a4e93478463b187e7745a9a54844488e44e33a5 (571.473.920 B)
- rootfs.tar sha256 7c26c76b6fc27bc12dc8d5e012e51d5c2c0568fa72a8e02cad5432dfcd36a2dd (242.790.400 B)
- rootfs.ext4 sha256 9365b52f9aef33e0623ae8271aaf415483fe72e1afbc8ef99f93e15437bc620a
- GUI md5 1f998c46909307397525c975a712adfd (13.407.624 B) · fuente-md5 esperado=staged=embebido 964193ae9306b5127845cec44a57d52c
- Ubicación: assessment-194/clone-buildroot/build/images/ (fuera de Git por tamaño; hashes en 01-baseline/artifact-manifest.json)

### DECISIÓN PENDIENTE (para el owner) — defconfig congelado incompleto
El `X6200_defconfig` de d084e287 no habilita `BR2_PACKAGE_PULSEAUDIO=y` (sólo `PULSEAUDIO_DAEMON=y` suelto)
ni `LIBSNDFILE`; kconfig descarta el símbolo y el GUI no configura (falta sndfile/libpulse). El build del candidato
se hizo con `PULSEAUDIO=y` **sólo en build/.config** (declarado en 01-baseline/README §8, manifiesto y
logs/step6/config-change.log). **Opciones**: (a) aceptar la desviación documentada (el dossier la declara y limita
la conclusión de reproducibilidad), o (b) PR al repo Buildroot corrigiendo el defconfig. La épica prohíbe a los
ejecutores abrir tickets/implementar; queda a decisión del owner/orquestador. NO se tocó el defconfig ni el .mk.

### Otros puntos del informe #195
- Divergencia: tip lu2dsx/x6200-rom = 50bf8ab0 (PR #200, 22 archivos de producción) -> **STALE** para el tip;
  el dossier conserva validez para c888308e.
- `banco-apps` OCUPADO por sesión ajena (no se usó ni detuvo). `assessment195-build` sigue corriendo (inocuo).
- Huecos documentados: radio/hardware (15 filas, #199), boot/init/DATA/update (13 PENDING, #196), CTest ft8/qth +
  QTH app BLOCKED, webserver SKIP, subsistemas sin instrumento (WF mode, RIT/XIT, lock, QSO log/ADIF, screenshot).

## Entorno medido
- Host: 16 núcleos, 89 GB RAM, 130 GB libres en /home. Docker: imagen `x6200-buildroot:latest` (b3e7601313d6) y
  `x6200-uiharness*` disponibles. `ninja` NO instalado en host (usar el generador que use cada script o registrar
  dependencia exacta). `uv`, python3, cmake, git, gh, qemu-arm-static disponibles.
- `banco-apps` OCUPADO al iniciar (no usar ni detener).
- Radio X6200: 192.168.0.236, GUI desplegada 3ef20224dbd9 (= build del candidato c888308e, medición de esta
  sesión). No tocarla en #195-#198.

## Actualización 21/10 16:45 -03 — build de ROM para flasheo (pedido del owner)

El owner pidió "arma la ROM y flashea con lo último". Se construye en sandbox privado `rom-build/`:
- GUI = **tip de lu2dsx/x6200-rom = `50bf8ab0`** (PR #200 "Neo-retro animated text boot", MERGED, 44
  archivos, cambios de PRODUCCIÓN: src/bootanim/*, src/boot_splash.*, main.c, rootfs S01bootanim, tests).
  **OJO**: el candidato congelado del assessment sigue siendo `c888308e`; `50bf8ab0` es el tip nuevo.
  → Dossier #195 debe declarar **STALE para el tip nuevo** (cobertura a repetir si se adopta #200).
- Buildroot = `d084e287` ✓ (par congelado; incluye PR #1 fstab ext4).
- Inyección del GUI: método del CI (patch del `.mk` en el clon privado: SITE=local al clon, SITE_METHOD=local,
  GIT_SUBMODULES deshabilitado). Verificado: el ROM incluirá el GUI del clone-gui (50bf8ab0), NO el vendor.
- Entorno: build dentro del contenedor `x6200-buildroot` (Ubuntu 24.04 / GCC 13.3 / glibc 2.39) porque el host
  tiene GCC 16.2 / glibc 2.43 y rompía host-util-linux (pidfd), host-e2fsprogs (C23 typedef int bool) y libcody
  (exige C++11 exacto). Parches aplicados SOLO en el clon privado:
  - `buildroot/package/Makefile.in`: `HOST_CFLAGS += -std=gnu17` y
    `HOST_CXXFLAGS := $(filter-out -std=gnu17,$(HOST_CXXFLAGS)) -std=gnu++11`
  - `BR2_CCACHE_DIR` dentro del workspace; `ac_cv_func_pidfd_{open,send_signal}=no` por env.
- Estado: compilando paquetes target (dbus/host-dtc), imágenes aún no generadas. Al terminar: verificar y flashear
  (script `flash-rom-sd.sh`: exige imagen <= 571.473.920 B para no pisar DATA, backup del tramo boot+rootfs,
  verifica UUID de DATA). La SD tiene DATA ext4 con los datos del owner (backups: `backups/DATA-prefl-*.tgz`,
  `backups/sd-*sd-full.img`).
- Campaña #194: la unidad #195 usa su PROPIO build congelado (assessment-194/clone-buildroot, par c888308e +
  d084e287); el build del orquestador NO se usa como artefacto del candidato.

## HALLAZGO de reproducibilidad del par congelado (21/10 17:47 -03)

El par congelado (GUI c888308e + Buildroot d084e287) **NO compila el GUI desde cero**:
- `br2_external/configs/X6200_defconfig` **no habilita** `BR2_PACKAGE_LIBSNDFILE` ni `BR2_PACKAGE_PULSEAUDIO`
  (solo hay `BR2_PACKAGE_PULSEAUDIO_DAEMON=y` suelto, inconsistente). El `x6200-gui` exige
  `sndfile`, `libpulse`, `libpulse-simple` por pkg-config → `configure` falla:
  `CMake Error: The following required packages were not found: sndfile, libpulse, libpulse-simple`.
- El ROM instalado en la radio SÍ tiene PulseAudio y libsndfile → **el defconfig congelado no reproduce el ROM**
  (o el ROM se construyó con una config distinta). El volumen compartido `x6200-build-output` también tiene
  `# BR2_PACKAGE_PULSEAUDIO is not set`, pero su **staging sysroot conserva los .pc de builds anteriores**, y por
  eso `tools/compila-gui.sh` funciona (no desde cero).
- Evidencia adicional: dos corridas fallidas de build limpio por esta causa; el GUI **sí** es el PORT
  (fuente md5 a576d48d9994efd3e28b9f1438fa7b24, `src/adaptive_tuning.c`, `src/dfl_inline.c`, `lvgl/` presentes) —
  el nombre de directorio `x6200-gui-v0.1.0` es por `X6200_GUI_VERSION` y NO implica vendor (aclaración: el
  ejecutor de #195 sostuvo que era vendor; es incorrecto, el `SITE_METHOD=local` usa el clon del port).
- Fix aplicado SOLO en el sandbox privado del orquestador para el ROM del owner: `BR2_PACKAGE_LIBSNDFILE=y`,
  `BR2_PACKAGE_PULSEAUDIO=y` + `olddefconfig` + receta CI (lame, mpg123, libsndfile-dirclean, make).
- **Para la campaña #194**: este hallazgo debe entrar en `01-baseline/` (manifiesto/procedencia) y en el dossier:
  el par congelado requiere este ajuste de config para construir; declararlo explícitamente y no atribuir el
  fallo al código del port.

## ROM flasheada en la SD (21/10 17:54 -03) — pedido del owner "con lo último"

- Imagen: `rom-build/clone-buildroot/build/images/sdcard.img`
  - **sha256 `17dc49ed553b953b8a2ec2fb94d95d2ec74ed07099c8db1d7c7d929a72eb8779`** · size 571.473.920 B (545 MiB)
  - `rootfs.tar` sha256 `966bfc44312cd7c54e0337f6d37c2be89e2c5c849724daebafde099c710d8c0d` (242.821.120 B)
- Contenido verificado antes de flashear: fstab `ext4` para /dev/mmcblk0p3 ✓; S01create_data con "DATA ya existe" ✓;
  GUI 13.412.248 B (md5 `3b22a03a3b7850e5c546c564125d0eb0`) ✓ port; S01bootanim presente (#200) ✓; pulseaudio y
  libsndfile presentes ✓; rootfs 233 MB.
- GUI del ROM = **tip `50bf8ab0`** (PR #200 incluida) — **NO** es el candidato `c888308e` del assessment.
  fuente md5 del GUI en el build: `a576d48d9994efd3e28b9f1438fa7b24`.
- Flasheo: dd de la imagen a /dev/sda (solo primeros 545 MiB) → p3 (DATA) intacta, UUID `9facdaea-a527-4660-94b9-bd89465ec071`
  verificado después ✓. Backup del tramo previo: `backups/sd-bootrootfs-antes-20261021-175211.img` (574.619.648 B).
- Fix de config necesario para que el par congelado compile (ver hallazgo arriba): LIBSNDFILE + PULSEAUDIO.

## Arranque de la ROM del tip 50bf8ab0 en la radio (21/10 18:35 -03) — VALIDADO con un defecto de MBR reparado

- La radio arrancó con la ROM nueva (pedido del owner, tip 50bf8ab0 = candidato + #200):
  - identidad: GUI md5 `3b22a03a3b7850e5c546c564125d0eb0` (13.412.248 B), `boot_identity fuente a576d48d9994efd3e28b9f1438fa7b24` ✓
  - **#200 boot animation funcionó**: boot.log `bootanim stop 18.1 s`, `bootanim handoff 18.1 s`, `GUI lista 18.2 s` ✓
  - CRASHLOG 0 ✓, 1 GUI + 1 daemon ✓, DATA UUID 9facdaea… intacto ✓
- **Defecto encontrado y REPARADO en vivo**: `/mnt` no montaba por `EXT4-fs (mmcblk0p3): bad geometry: block count 828544
  exceeds size of device (828416 blocks)`. Causa: el MBR que escribe genimage tiene 2 entradas y **dejó la 3ra entrada
  vieja** del MBR anterior (tipo FAT32, 512 KiB más chica) → p3 quedaba 512 KiB corta. Fix: `parted -s /dev/mmcblk0
  unit B resizepart 3 100%` (solo tabla, sin mover datos) → p3 = 3.393.716.224 B ✓ → `/mnt` monta ext4 rw ✓.
  **Datos intactos**: `params.db` 118.784 B md5 `7f34a9cae05ce43b7d2f690dfa0e0997` = el md5 previo al flasheo ✓.
- Añadido al script de flasheo (`verify-flash-rom.sh`): extender p3 al final tras el dd, para futuras SD.
- **Pendiente de decisión del owner**: borrar los ~5,8 GB de `rootfs_*.tar` viejos en `images/` (dejar el `.tgz` de
  update).

## Limpieza de imágenes viejas (21/10 18:12 -03, pedido del owner)

- Borradas **49 `.img`** (26 GB) + `x6200_20261012_193033.img` (571 MB) + **25 `rootfs_*.tar`** (5,8 GB) de
  `/home/pablo/desa/x6200/images/`. Manifiestos: `backups/deleted-old-images-20261021-180220.txt` y
  `backups/deleted-old-rootfs-tars-20261021-181147.txt`. Se conservó `x6200_r1cbu_v0.1.2-lu2dsx.tgz` (paquete de
  update, 98 MB) y los docs/scripts chicos.
- A salvo: ROM nueva (`rom-build/.../sdcard.img`, 545 MB), backup SD completa (3,7 GB), backup boot+rootfs previo
  (548 MB), backups de DATA. Disco: 113 GB libres.
- Radio verificada tras el cleanup: uptime ~14 min, 1 GUI, CRASHLOG 0, `/mnt` ext4 rw, `params.db` md5 7f34a9ca ✓.

## Plan "imagen nueva con todo" (21/10 18:15 -03, pedido del owner)

- El owner avisó que **otro agente está implementando el issue/PR #202** (boot stuff/MBR) y que cuando termine se hará una
  imagen nueva con todo. Mientras tanto la campaña #194 sigue (unidad #196 en ejecución, contenedores `assessment196-qemu`
  y `assessment196-neg` activos).
- Pipeline listo: `/home/pablo/desa/x6200/rebuild-rom-incremental.sh` (corre en contenedor `x6200-buildroot`):
  actualiza clon GUI y Buildroot a los tips de `lu2dsx/x6200-rom`, re-aplica override del paquete GUI (método CI),
  asegura LIBSNDFILE/PULSEAUDIO, `make x6200-gui-rebuild` + `make` → sdcard.img nuevo (incremental, ~5-10 min).
  Baseline del watcher en `rom-build/last-built.txt` = GUI 0d00e7fc / BR d084e287 (producción equivalente a la imagen
  flasheada, que se construyó desde 50bf8ab0; sólo difieren docs + el u-boot prebuilt removido sin uso).
- Flasheo: `verify-flash-rom.sh` ya incluye el fix de extender p3 al final (MBR con 2 entradas dejaba la 3ra entrada
  vieja → p3 512 KiB corta → `/mnt` no montaba).

## Imagen nueva "con todo" reconstruida (21/10 18:15 -03)

- Pedido del owner: cuando #202 esté mergeado, hacer una imagen nueva con todo. #202 ya está **MERGED**
  (merge en el tip GUI `0d00e7fc`; Buildroot sigue en `d084e287`).
- Rebuild **incremental** con `rebuild-rom-incremental.sh` en contenedor: **~3 minutos** (ccache). Resultado:
  - `sdcard.img` sha256 `c8ce5bd05206cdfeff0ee5bbc7402ff32394a681afcd47ec27fef72e25eba16d` · 571.473.920 B
  - `rootfs.tar` sha256 `f24ce406d4f1e2caf5a893895458442cec71e4435f3293897e0b432a72efe2dd` · 242.821.120 B
  - Verificado: gate #86 PASS (13.412.248 B), GUI md5 `3b22a03a3b7850e5c546c564125d0eb0` (**igual** a la
    flasheada), fuente-md5 `a576d48d…`, fstab ext4, `S01create_data` con "DATA ya existe", S01bootanim presente,
    libpulse+libsndfile, **sin u-boot prebuilt en rootfs** (efecto de #202 ✓).
  - `rom-build/last-built.txt` = GUI 0d00e7fc / BR d084e287 / shas nuevos.
- Flash pendiente: requiere la SD en la PC. `verify-flash-rom.sh` ya incluye extender p3 al final (fix del defecto de
  MBR detectado en el primer flasheo).
- Nota: el contenido funcional es idéntico al ya flasheado (la diferencia de sha256 es por empaquetado/timestamps);
  flashear es para dejar la SD "con todo" formalmente y validar el script con el fix de p3.

## PR de fix de build creado (21/10 18:45 -03)

- **PR iariap/AetherX6200Buildroot#2** — `fix/defconfig-audio-194` -> `lu2dsx/x6200-rom`, commit `b57e31ae`.
  Agrega a `br2_external/configs/X6200_defconfig`: `BR2_PACKAGE_LIBTOOL=y`, `BR2_PACKAGE_LIBSNDFILE=y`,
  `BR2_PACKAGE_PULSEAUDIO=y` (conserva `PULSEAUDIO_DAEMON=y`). Validado: `make X6200_defconfig` habilita los 4
  símbolos y `olddefconfig` no descarta nada (par autosuficiente). Elimina la desviación declarada en el dossier
  #195 (build se hizo con esos símbolos sólo en `.config`). Con esto el par congelado compila desde cero.
- Evidencia para el #196/#199: antes del PR el build limpio fallaba en el configure del GUI
  (`sndfile`, `libpulse`, `libpulse-simple` no encontrados). Dos builds independientes con estos símbolos llegaron a
  imagen y la imagen arrancó en la radio (gate #86 PASS, GUI port 13.412.248 B, /mnt ext4 rw, CRASHLOG 0).

## Delegación de unidades #196 y #197 a otros agentes (21/10 18:50 -03, aviso del owner)

- El owner delegó **#196 (ROM/confiabilidad) y #197 (UI/UX)** a otros agentes. Este orquestador **deja de ejecutar**
  esas unidades y **detuvo el ejecutor #196 que había lanzado antes** (contenedores `assessment196-*` detenidos;
  no había rama publicada ni PR: `git ls-remote assessment/196*` vacío, 0 PRs). Su worktree `wt-assessment-196`
  queda **parado** como referencia (sin commitear/pushear) para que el nuevo ejecutor pueda continuar o descartar.
- Reparto actual: **orquestador** = integración/revisión de PRs, watcher de rebuild/flash, y **#198/#199** cuando
  corresponda (secuencia: #198 va después de #197; #199 incluye QA físico remoto RX con sesión exclusiva de radio).
- La rama de integración sigue en `230b2063` (#195 mergeado). Cualquier evidencia de #196/#197 que llegue de otro
  agente se revisa/mergea con el mismo criterio (diff sólo de su carpeta, manifiesto, SHA256).

## Sesión exclusiva de radio verificada (21/10 21:42Z) — prerequisito de #199

Evidencia: ping OK; `ss -tn :22` en la radio = **sólo** la conexión transitoria desde la PC del operador
(192.168.0.195); procesos en la radio = sólo `sshd` (listener) y `x6100_webserver` propios; radio **idle**
(`lastbreath app=home`, contadores ev sin actividad de comandos, 1 GUI, CRASHLOG 0, /mnt ext4 rw, params
7f34a9cae05c); sin contenedores ajenos conectados. Los contenedores largos de otras sesiones siguen existiendo
pero no tocan la radio. => **fase física HABILITADA** para #199 y para el flasheo de la imagen nueva.

## Flasheo de la imagen nueva en la SD (21/10 18:47 -03)

- Imagen: sdcard.img sha256 c8ce5bd05206cdfeff0ee5bbc7402ff32394a681afcd47ec27fef72e25eba16d (571.473.920 B),
  built from GUI tip 0d00e7fc + BR d084e287 (verificación: GUI 13.412.248 B md5 3b22a03a, fstab ext4,
  S01create_data 'DATA ya existe', S01bootanim #200, gate #86 PASS).
- **Incidente durante el dd**: la imagen tiene 2 particiones y el dd deja la 3ra entrada MBR inconsistente; en este
  flasheo el kernel terminó sin p3 (`lsblk` mostraba solo p1/p2). **Datos intactos** (el dd solo escribe 545 MiB,
  p3 empieza en 571.473.920 B). Fix: borrar/recrear la entrada p3 con **sectores exactos**
  (`parted unit s mkpart primary 1116160 7744511`); ojo: `parted ... mkpart ... 100%` **alinea al MiB** y deja
  p3 512 KiB corta (primer intento de fix falló así). Verificado post-fix: p3 = 3.393.716.224 B, LABEL=DATA,
  UUID 9facdaea-a527-4660-94b9-bd89465ec071, mount ro OK. `verify-flash-rom.sh` quedó actualizado con la
  recreación por sectores exactos para futuros flasheos.
- DATA: params.db 118.784 B md5 3de650e36cbc7da4ed33d5ff5d2415af (cambió desde el 7f34a9ca previo porque la radio
  reescribió params al usarla tras el fix anterior; mismo tamaño y contenido esperado). Backup fresco:
  backups/DATA-postflash-20261021-184646.tgz (6,7 MB, sha256 30c69a22…). Backup del tramo previo:
  backups/sd-bootrootfs-antes-20261021-184335.img (574.619.648 B).
- SD desmontada y sincronizada, lista para volver a la radio. Pendiente: primer arranque + verificación (bootanim,
  boot_identity a576d48d, /mnt ext4 rw, params, CRASHLOG 0).

## Boot de la ROM nueva en la radio + causa de la "imagen anterior" (21/10 18:55 -03)

- La radio **arrancó OK** con la imagen c8ce5bd0: GUI md5 3b22a03a (13.412.248 B), boot_identity fuente
  a576d48d9994efd3e28b9f1438fa7b24, `bootanim stop/handoff 14,6 s` + `GUI lista 14,7 s` (animación #200
  funcionando), 1 GUI + 1 daemon, CRASHLOG 0, `/mnt` ext4 rw, params.db 118.784 B (la GUI lo reescribió al arrancar;
  md5 nuevo 081f93b5 = normal). La radio quedó en otra red: **192.168.18.67** (el helper la encuentra solo).
- **Hallazgo del owner**: en el boot se ve "la imagen anterior" antes de la animación nueva. **Causa medida**: el
  kernel dibuja el logo configurado por Buildroot (`BR2_LINUX_KERNEL_CUSTOM_LOGO_PATH` →
  `board/X6200/linux/logo.png`, 800x480 → `CONFIG_LOGO_LINUX_CLUT224`). U-Boot no dibuja nada (sin splash/BMP en su
  config) y `boot.scr` sólo carga `zImage`/dtb. La animación #200 arranca en `S01bootanim` (userspace).
- **Fix propuesto/PR**: **iariap/AetherX6200Buildroot#3** (`fix/no-kernel-logo`): quita `CUSTOM_LOGO_PATH`, pone
  `# CONFIG_LOGO is not set` y elimina `logo.png` → en boot la pantalla queda negra hasta que arranca la animación.
- PRs de build abiertos: **#2** (defconfig: LIBTOOL/LIBSNDFILE/PULSEAUDIO) y **#3** (sin logo de kernel). Cuando se
  mergeen, el watcher detecta el cambio del tip de Buildroot y lanza el rebuild incremental + flash.

## Imagen reconstruida sin logo de kernel (21/10 19:00 -03)

- PRs mergeados: **#2** (defconfig LIBTOOL/LIBSNDFILE/PULSEAUDIO, merge f23d14f) y **#3** (sin logo de kernel, merge 128d96d). Buildroot tip = f23d14f; last-built actualizado.
- Rebuild incremental (kernel con linux-dirclean + GUI + imagen): sdcard.img sha256 03c6f9e6e5ad75ec271125caf17bfe7f7721d9aaf0924c676b43f257ef4ecce5, rootfs.tar sha256 781aa0b54425e857b5a1d7ba61427971fe84929be06c5b561b5ef6e80418e459. Kernel .config: # CONFIG_LOGO is not set ✓ (zImage 5.541.376 B). GUI 13.412.248 B md5 3b22a03a ✓, gate #86 PASS, fstab ext4 ✓, S01create_data ✓, bootanim ✓, sin logo viejo en rootfs.
- Incidente resuelto: el build/.config conservaba BR2_LINUX_KERNEL_CUSTOM_LOGO_PATH a logo.png (removido por #3) → el kernel fallaba convirtiendo un archivo inexistente. Saneado a "" en build/.config y el script de rebuild lo hace automáticamente.
- Pendiente: flashear (requiere la SD) y verificar en boot que solo se vea la animación nueva.

## VERIFICADO en la radio: ROM sin logo de kernel (21/10 19:12 -03)

- Radio arrancada con la imagen 03c6f9e6 (Buildroot f23d14f = PR #2 + #3): GUI md5 3b22a03a (13.412.248 B),
  boot_identity fuente a576d48d9994efd3e28b9f1438fa7b24, `bootanim stop 15,4 s` + `handoff 15,4 s` +
  `GUI lista 15,5 s`, 1 GUI + daemon, CRASHLOG 0, `/mnt` ext4 rw, params.db 118.784 B (la GUI lo reescribió),
  kernel 6.1.82 con `# CONFIG_LOGO is not set` (sin la imagen vieja de boot; confirmado visualmente por el owner:
  "arranco OK"). Radio en 192.168.0.236. => **imagen "con todo" validada en hardware**.
- Fixes de build en el repo: PR #2 (defconfig LIBTOOL/LIBSNDFILE/PULSEAUDIO) y PR #3 (sin logo de kernel), ambos
  mergeados. El script de rebuild reconstruye kernel cuando cambia el tip de Buildroot y saneа CUSTOM_LOGO_PATH.

## #196 COMPLETADA — PR #207 revisado y mergeado (21/10 19:55 -03)

- **PR iariap/x6200_gui#207** revisado por el orquestador: 103 archivos, 0 fuera de `docs/evidencia/assessment-rom-194/02-rom/`;
  **SHA256SUMS (102 archivos) verificado con `sha256sum -c` OK**; README con tabla de estados; results.csv 87 filas,
  reliability.csv 46; procedencia revalidada por SHA256 del artefacto #195 (d0ff4435/7c26c76b/9365b52f/914f07eb, GUI
  1f998c46, fuente 964193ae, gate #86 PASS). **MERGED** en `assessment/rom-194` (commit afa6109b, merge posterior).
- Resultados: ROM-01 PASS, ROM-02 PASS, ROM-03 FAIL tarjeta nueva / PASS DATA existente, ROM-04 FAIL empaquetador/PASS
  aplicación, ROM-05 PASS parcial, ROM-06 **BLOCKED** (banco-apps ocupado), ROM-07 PASS parcial (100 ciclos x2 + 1 h
  reposo), ROM-08 PASS protocolo / FAIL DB corrupta-RO-faltante (SIGSEGV determinista cb_hooks_init PC 0x42e12c).
- **Nota de transparencia**: el contenedor del primer soak de #196 fue **detenido externamente por el orquestador**
  (yo) a los 31 min (exit 137) cuando el owner delegó #196/#197 y detuve los contenedores `assessment196-*`; no fue
  OOM ni crash. El ejecutor preservó el parcial y repitió con proceso nuevo. Registrado para que el "actor desconocido"
  no quede como anomalía.
- **Pendiente de decisión del owner**: ventana de `banco-apps` (o autorización de banco privado) para los 3 barridos
  de 25 apps (hueco principal antes de #197); fase de hardware en #199 (primer boot en tarjeta nueva, update con
  rollback/corte de energía, DB llena/RO); si se adopta el tip `0d00e7fc` hay que repetir build/identidad/layout/init/
  estrés (el dossier es STALE para el tip).
- **Defconfig**: el finding de #196 quedó **resuelto en el repo** por el PR Buildroot #2 ya mergeado (LIBTOOL/
  LIBSNDFILE/PULSEAUDIO en X6200_defconfig); el dossier de #196 fue escrito antes de ese merge y debe declararlo en
  la consolidación #199.

## #197 COMPLETADA — PR #203 revisado y mergeado (21/10 20:00 -03)

- **PR iariap/x6200_gui#203** revisado por el orquestador: 816 archivos nuevos, **diff real contra el merge anterior = 816 y 0 fuera** de
  `docs/evidencia/assessment-rom-194/03-ui-ux/`; **SHA256SUMS (815 archivos) verificado con `sha256sum -c` OK**; README con tabla de estados.
  **MERGED** en `assessment/rom-194` (merge c41f36b2).
- Resultados #197: results.csv 256 filas (PASS 236, INCONCLUSIVE 10, PENDING_HARDWARE 5, BLOCKED 2, REVIEW_REQUIRED 2, UNSUPPORTED 1,
  0 FAIL); determinismo 199 fixtures PNG byte-idénticas; capturas 199; audit 26.455 objetos/199 escenarios; contratos 2056 checks 0 fallos;
  temas 10 paletas/90 pares WCAG. No tocó radio ni bancos; BLOCKED por dependencia de artefactos de #196 (declarado, no tapado).
- Integración actual: `assessment/rom-194` = **c41f36b2** (merge #203) sobre ca1698d1 (merge #207) sobre 230b2063 (merge #201).
- **#198 LANZADA** (ejecutor propio): performance/eficiencia sobre el artefacto del candidato, sin radio ni bancos; PR a `assessment/rom-194`
  con título "docs: performance and efficiency evidence" y carpeta 04-performance/.
- Pendiente para #199: consolidación + **fase de hardware** (primer boot en tarjeta nueva, update con rollback/corte de energía, DB llena/RO,
  RX remoto) con **sesión exclusiva de radio** (ya verificada una vez) y declaración STALE por el tip `dadb1705` (cambios #200/#202 y PR #206
  bootanim v2 abierto de otro agente). PR final de integración a `lu2dsx/x6200-rom` para el maintainer.
- Decisión abierta del owner: **ventana de `banco-apps`** para los 3 barridos de apps (hueco de #196) y para las partes de banco de #198.

## #198 COMPLETADA — PR #208 revisado y mergeado (21/10 22:10 -03)

- **PR iariap/x6200_gui#208** revisado: 273 archivos, **0 fuera** de `04-performance/`; **SHA256SUMS (272) verificado con sha256sum -c OK**;
  results.csv 599 filas; README con tabla de estados; **MERGED** en `assessment/rom-194` (merge 05020942).
- Resultados #198 (sin FAIL): PASS 574, INCONCLUSIVE 11, REVIEW_REQUIRED 5, BLOCKED 2, UNSUPPORTED 1, PENDING_HARDWARE 6.
  PERF-01 render INCONCLUSIVE (3.980 frames; p50 4,29 ms/p95 5,38 ms; 198/199 estables); PERF-02 A/A INCONCLUSIVE por ruido de host
  (banco-apps ajeno cargando ~101% de un core); PERF-03 natural/wakeups/waterfall PASS; PERF-04 recursos QEMU PASS parcial (home 1,675%,
  CW decoder 9,033%, RSS ~56-60 MB, fds 25-27); PERF-05 audio digital voz/control PASS (controles silencio exacto 0/444k), mixer/FFT BLOCKED;
  PERF-06 estático PASS con REVIEW_REQUIRED en timers (WiFi 30 ms incondicional = 33/s; 41,8 wakeups/s estáticos) y lazo radio_thread.
- **STALE CONFIRMADO**: tip real `d4fb57f8` (PR #206 bootanim v2 mergeado) = 25 archivos de producción fuera del candidato
  (`src/bootanim/*`, `boot_splash`, `main.c`, `CMakeLists.txt`, `S01bootanim`, tests bootanim, workflows, u-boot). El orquestador había
  informado dadb1705 (stale él mismo); corregido.
- Integración: 05020942 (merge #208) sobre c41f36b2 (merge #203), ca1698d1 (merge #207), 230b2063 (merge #201). Evidencia en rama:
  01=81, 02=103, 03=816, 04=273 archivos.
- **#199 LANZADA (fase A: consolidación documental sin radio)**: ejecutor construye dossier `05-summary/` (aceptación, hallazgos fundados vs
  hipótesis, STALE, tabla de pendientes de hardware, plan de fase B, manifiesto/SHA256SUMS) y entrega PR a `assessment/rom-194`.
  **Fase B (QA físico)** requiere la radio disponible (ahora no responde) y coordinación del owner; luego PR final de integración a
  `lu2dsx/x6200-rom` para el maintainer.
- Decisiones abiertas del owner: (1) ventana de `banco-apps` para los 3 barridos de apps (#196) y un rerun limpio PERF-01/02 (hoy INCONCLUSIVE
  por carga ajena; NO se repite "para ver verde"); (2) disponibilidad de la radio para la fase B de #199; (3) si se adopta el tip d4fb57f8
  (rebuild/re-flash con bootanim v2) o se deja la imagen actual 03c6f9e6.

## #199 FASE A COMPLETADA — PR #209 revisado y mergeado (21/10 22:20 -03)

- **PR iariap/x6200_gui#209** revisado: 40 archivos, **0 fuera** de `05-summary/`; **SHA256SUMS (39) sha256sum -c OK**; dossier con README/index/coverage-final (334 ítems),
  findings (18 fundados + 10 hipótesis separadas), stale.md, hardware-plan (HW-01..08 + HR-01..06), hardware-remote (6/6 BLOCKED), provenance-audit,
  acceptance-manifest, commands.jsonl, logs. **MERGED** en `assessment/rom-194` (merge 8074d297).
- Aceptación consolidada: #195 (81, SHA 80 OK), #196 (103, 102 OK), #197 (816, 815 OK), #198 (273, 272 OK); artefacto revalidado; diff producción integración↔candidato = 0.
- Recomendación del ejecutor: **CONDITIONAL software / NO_GO de release** hasta completar la **fase B de hardware**.
- **Lo que falta para cerrar #199 y el PR final**: fase B (QA físico) + PR de integración final. La fase B requiere del owner: autorización escrita,
  ventana exclusiva de radio, decisión de identidad/SD (la radio corre el tip GUI 3b22a03a, no el candidato 1f998c46), backup+restore de DATA,
  SD(s) de prueba e instrumentos (V/I/P, audio, cámara); TX sólo con autorización aparte. Runbook y criterios en
  `05-summary/hardware-plan.md` y `.csv`.
- Pendiente de decisión: ubicación de `ASSESSMENT_ROM_194.md` y la fila de `docs/PRUEBAS.md` (hoy preparadas en `05-summary/`, ruta raíz diferida);
  el PR final (`assessment/rom-194` -> `lu2dsx/x6200-rom`) se armará con esas dos piezas cuando se cierre la fase B.
- Estado del repositorio: rama de integración `8074d297`; ninguna unidad de la campaña quedó pendiente de merge (las 5 están integradas, #199 fase A).

## PR FINAL DE INTEGRACIÓN ABIERTO — iariap/x6200_gui#210 (21/10 22:30 -03)

- **PR #210**: `assessment/rom-194` -> `lu2dsx/x6200-rom`, título **`docs: pre-release ROM evidence assessment`** (exacto según épica #194),
  cuerpo con `Refs #194..#199`, síntesis de 5 unidades, hashes de aceptación, recomendación CONDITIONAL/NO_GO de release hasta fase de
  hardware, STALE d4fb57f8 y pendientes físicos enlazados. **Sin auto-merge**; requiere revisión/merge del maintainer.
- Antes de abrirlo: la rama `assessment/rom-194` se sincronizó con el tip `d4fb57f8` (merge sin conflictos c20acc9e), se agregó
  `docs/ASSESSMENT_ROM_194.md` (ruta pedida por #199) y la fila/sección en `docs/PRUEBAS.md` (auditoría OK). Commit final 89c9f8ff.
  **Diff del PR: 1.315 archivos, 0 fuera de docs/** (verificado).
- Estado: 5/5 unidades integradas + PR final abierto. **Sólo queda la fase B (#199 hardware)** para que el PR final refleje el cierre completo:
  requiere del owner autorización escrita, ventana exclusiva de radio, decisión de identidad/SD, backup+restore de DATA e instrumentos.

## Fase B (#199) — preparada y lista (21/10 22:35 -03)

- `phase-b-remote.sh` escrito y validado (`bash -n` OK): ejecuta HR-01..HR-06 del runbook (identidad/boot snapshot, baseline,
  navegación RX, QSY reversible 20x, 60 min RX con serie nativa, cierre/restauración) **solo RX, sin TX/flash/reformat**, con
  gating de autorización (`--i-have-authorization`) y BLOCKED si no hay radio en red. Evidencia a `assessment-194/fase-b/<label>/`.
- `fase-b-README.md`: uso, cobertura, requisitos del owner y qué queda BLOCKED sin instrumentos (HW-02..HW-08).
- Estado: PR final #210 abierto y MERGEABLE (0 reviews del maintainer aún); radio apagada/no accesible → fase B no ejecutable hoy.
  El objetivo del goal queda activo: falta la fase B física y la actualización del PR #210 con sus resultados.

## AUTORIZACIÓN DEL OWNER para la fase B (21/10 23:22 -03)

- El owner autorizó la **fase B opción (a)**: prender la radio y avisar para correr **HR-01..HR-06 (solo RX)**:
  QA físico remoto, sin TX, sin flash, sin reformatear, con snapshot y restauración, escorando evidencia en
  `assessment-194/fase-b/<label>/`. El script `phase-b-remote.sh` (validado) es el ejecutor; cubre HR-01..06
  (~65-75 min por el soak de 60 min). HW-02..HW-08 siguen requiriendo instrumentos y autorización específica (TX aparte).
- Cuando la radio responda: verificar sesión exclusiva (sin SSH ajenas), correr el script con
  `--i-have-authorization`, y luego actualizar el PR final #210 con los resultados y el estado de los BLOCKED restantes.

## FASE B COMPLETADA — HR-01..06 PASS en el tip (22/10 00:36 -03 / 03:36Z)

- Ejecutada con autorización del owner y sesión exclusiva sobre la ROM instalada (= tip 03c6f9e6, GUI 3b22a03a,
  fuente a576d48d, kernel 6.1.82, boot_id cdd42e40), **sin TX, sin flash, sin reformatear**:
  HR-01 PASS (exclusividad/identidad/montajes/CRASHLOG 0) · HR-02 PASS (captura base 268.069 B + verifica-radio 57 líneas OK)
  · HR-03 PASS (8/8 apps abiertas/capturadas/cerradas) · HR-04 PASS (20/20 QSY reversibles a 7.105.008 Hz; set 394-402 ms,
  restore 1,398-1,637 s) · HR-05 PASS (60 muestras/59,5 min, 1 boot_id, CRASHLOG 0, 1 GUI/daemon PID, RSS 23.348 kB constante,
  fds 39-40, threads 13, get RTT mediana 408 ms, params md5 estable salvo 1 cambio, CPU GUI mediana 54,0% de un core)
  · HR-06 PASS (freq restaurada, app off/home, captura final). Observación no instrumentada: bat 8.600→8.000 mV en el soak.
- Evidencia integrada en `05-summary/hardware-remote/` (README, results.csv 6 PASS, raw/ 104 archivos, artifact-manifest,
  hardware-plan.csv con HR en PASS, SHA256SUMS 143 entradas verificado) + nota 7.1 en 05-summary/README y 8.1 en
  docs/ASSESSMENT_ROM_194.md. Commit 2777913e pusheado a assessment/rom-194; PR #210 actualizado (comentario con resultados,
  MERGEABLE, diff 1.419 archivos, 0 no-docs).
- **Estado de la campaña: TODOS los entregables hechos** (5 PRs de unidad revisados/mergeados + PR final abierto). Pendiente
  fuera del alcance del orquestador: revisión/merge del PR #210 por el maintainer; HW-01..HW-08 BLOCKED/PENDING_HARDWARE
  (instrumentos, TX autorizado, medios de prueba de update/DB, 8 h) documentados y enlazados en el dossier.
- Decisiones menores del owner: ventana de banco-apps (3 barridos de apps de #196); adoptar el tip d4fb57f8 (rebuild/re-flash
  con bootanim v2 #206) o quedarse con 03c6f9e6.
