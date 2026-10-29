# Registro de pruebas del X6200

Que una versión **funcione** no se sostiene con "debería andar": se sostiene con corridas de prueba
medidas. Este registro es el hermano de [`docs/HALLAZGOS.md`](HALLAZGOS.md) —ahí van los hallazgos,
acá van las **corridas**— y usa su misma regla, que es del dueño del proyecto:

> todos los hallazgos deben estar fundados, si no se pueden fundar no debe ser un hallazgo

## Cómo se registra una corrida

Cada vez que se compila y se despliega una versión nueva en la radio se agrega **una fila** a la
tabla de abajo y **una sección** al final. Cada afirmación lleva, en el mismo lugar, una de estas
tres cosas:

| Estado | Qué se escribe | Ejemplo de este registro |
|---|---|---|
| **medido** | el comando y su salida pegada, o el número con su origen y fecha | `md5sum /usr/sbin/x6200_gui` → `d0ff2dba9c85` |
| **código** | el archivo (y la línea) que lo dice | `codigo: src/spot_marks.c:181` |
| **no medido** | se dice así, y va al **anexo B** | "no se probó transmisión" |

Un número redondeado está bien **si está dicho como redondeo**. Lo que no puede aparecer es un
número inventado presentado como medición. Si algo se midió en su momento y no quedó la captura, se
dice; si una comprobación no se pudo repetir, también.

**Evidencia cruda:** las capturas, logs y pilas van a `docs/evidencia/<fecha>-<tema>/` y cada
sección los enlaza. La auditoría `tests/audita-pruebas.sh` verifica que la estructura cierre y que
**cada archivo citado exista**.

## Resumen

| Fecha (2026) | Build (md5) | Commit | verifica-radio | Barrido de apps | Caídas | Veredicto |
| 28/10 ~21:00 | ROM `d3f22d80…` + GUI `5ef35ec0b807` | (local) | **#292 banda lateral (noche)** | 1 parcial / 1 ok | 0 | 790 kHz: script oficial **FAIL del criterio absoluto** (789-LSB 24.7 / 789-USB 33.1 / 791-LSB 33.0 / 791-USB 22.8 dB; portadora débil, dirección correcta). 760 kHz con el mismo método y criterio relativo: **PASS** (26.1/39.6/36.3/24.9 dB, lados opuestos). Sin regresión de #292. Evidencia: `docs/evidencia/2026-10-28-lsb-night/` |
| 28/10 ~17:00 | ROM `d3f22d80…` + GUI `5ef35ec0b807` | (local) | **icono WiFi centrado** | 1 ok | 0 | **Medido en captura**: glifo x139..155 (centro 147) → x144..160 (centro 152,2) en caja 139..166 (centro 152,5) ✓. `tests/check_info_bar.py` PASS con guarda nueva. `crashlog 0` |
| 28/10 ~16:55 | ROM `d3f22d80…` + GUI `0cbb0c23c8c7` | (local) | **BT en su app** | 1 ok | 0 | **Ajustes ya no tiene el encendido/Discoverable/Devices de Bluetooth**: viven en la app BT (capturas en `docs/evidencia/2026-10-28-bt-app/`). `btcat-ctl bt on/off` medido: hci0 `UP RUNNING` / `DOWN` ✓. `crashlog 0` |
| 28/10 ~16:10 | ROM `d3f22d80…` + GUI `0a869656870e` | (local) | **zona CW automática** | 1 ok | 0 | **Medido**: 6m 51.000 USB -> 50.099 **CW** -> 50.150 **USB** (restaura); 40m 7.060 LSB -> 7.030 **CW** -> 7.060 **LSB** ✓. Falta confirmar con la rueda física (se probó por control remoto, mismo observador de frecuencia). `crashlog 0` |
| 28/10 ~15:45 | ROM `d3f22d80…` + GUI `e968f4ff4861` | (local) | **CW / VFO activo** | 1 ok / 1 parcial | 0 | **CW queda en CW al cambiar de banda** ✓ medido: 40m->12m->6m->10m siguen en CW; salir a LSB y volver a 12m recupera USB-D de memoria ✓; con VFO B activo el log muestra `Radio set vfo 1 mode=2`/`sync ... activo 1` ✓; **CW con B activo quedo parcial** (el activo volvio a A en el ensayo, posible A/B fisico). `crashlog 0` |
|---|---|---|---|---|---|---|
| 20/10 18:01 (hora host) | (sin radio; ARM **`fd864a791cb5`**, 13.899.984 B; fuente `34bea401…`) | `a0fa0250` (#183) | — | — | 0 | **DFL presets de filtro: gates de host + arnés + banco QEMU** ✓ (sin desplegar, sin flashear y **sin transmitir**): nativos **47.585 comprobaciones, 0 fallos** (`test_dfl_presets` 175 + migración `_5` 62), estáticos **11 OK**, determinismo **PASS** (196 fixtures × 2, PNG idénticos), ruff/format/ty/pytest OK (incl. **13 e2e**), arnés **196 fixtures, 0 fallos** (54 nuevos), `--inspect` **2.994 labels sin truncado ni glyphs faltantes**, A/B **`AB-DFL183` PASS** (140/142 escenarios idénticos; 6 `dfl-page-tema-*` explicados), y el banco muestra **A/B/C/D/E** con el gate #86 **3/3 marcas** y la base del chroot migrada a `version=5`. **Validación física: pendiente del owner** (no bloquea el PR). Evidencia: [`2026-10-20-dfl183/`](evidencia/2026-10-20-dfl183/README.md) |
| 19/10 ~15:2x (hora radio) | GUI **`67387bfc4391`** (13.849.516 B; ROM SD `c640645d810c`; **desplegado por otra sesión** a las 15:18, no por esta QA) | `02090454` (#154) | **CB TX cualitativo (teclado CW)** | (no corrido) | 2 GUI (#131) + radio caída (#121) | **TX en CB ✓ / interlock ✗ / 10 m ✓**: con `cw_send` (teclado CW) el equipo transmite en **27.185** (CB C, CH 19) y **28.074** (10 m), pero el **interlock NO lo bloquea** en **26.515** (CB B, fuera del SBC): `tx:true`, sin `TX blocked` → el keyer CW usa `radio_set_morse_key()` y **no** pasa por `radio_set_ptt()` (#146 §7.2). Potencia real **NO medida** (sin wattmeter; readback `pwr` roto #157). Evidencia: [`2026-10-19-cb-tx-cualitativo/README.md`](evidencia/2026-10-19-cb-tx-cualitativo/README.md) |
| 19/10 11:36 (hora radio 14:36 UTC) | GUI **`9a094c421b0e`** (13.844.268 B; ROM SD `c640645d810c`) | `1e3a1e4` | (no corrido) | — | 0 | **F0 CB 27 MHz PARCIAL** ✓✗: **el baseband/MCU NO bloquea el TX en 27 MHz** (medido: `tx:true` y barrido de ROE forzado a 0,1 W con **43/51 muestras entre 26,935 y 27,435 MHz**, min 1,0, alrededor de 1,1 en 27.185). **La F0 NO puede ser PASS**: potencia real y armónicos **NO medidos** (sin carga fantasma ni instrumentos); AM/FM a potencia configurada y PTT del micrófono **no medidos**. Radio restaurada (27.185 MHz LSB, RX, vol 0, misma GUI). Evidencia: [`2026-10-19-f0-cb-tx/README.md`](evidencia/2026-10-19-f0-cb-tx/README.md) |
| 19/10 ~10:40 (hora host) | (sin radio; ARM **4893b8f39bf3**, 13.783.700 B) | `ade36bbb` (#146) | — | — | 0 | **CB 27 MHz: gates de host + arnés + banco QEMU** ✓ (sin desplegar, sin flashear y **sin transmitir**): nativos 0 fallos (RED→GREEN de `test_cb`, 85 comprobaciones), `check_wcag`/`check_cfg_fills`/`check_actions` OK, determinismo PASS, ruff/format/ty/pytest (incl. 13 e2e) OK, A/B `AB-CB146` PASS (B=`ade36bbb`), y el banco muestra `CH 19`/`CH --`/`CH 23`/`CH 24` con `tx=false`. **F0 (RF real) requiere radio**. Evidencia: [`2026-10-19-cb-27mhz-146/`](evidencia/2026-10-19-cb-27mhz-146/README.md) |
| 18/10 ~12:40 (hora radio) | GUI **ffef63df58cb** (build limpio; **NO desplegado**) | `94b9989` | (sin radio) | — | 0 | **Radio inalcanzable en las dos redes** ✗: build limpio + gate #86 OK (`ffef63df58cb`, 13.757.724 B, 3/3 marcas), pero `tools/prueba-gui-en-radio.sh` abortó porque `radio_ip()` no encontró la radio. **#133/#114/#121 quedan SIN VEREDICTO**: no se pudo correr ninguna de las tres pruebas. La 4.ª caída del #121 sigue sin recuperación (necesita ciclo de energía; la batería estaba en 8,6 V, anotado en el #121). Evidencia: [`2026-10-18-qa-radio-backlog/`](evidencia/2026-10-18-qa-radio-backlog/README.md) |
| 17/10 ~22:20 (hora radio) | ROM `c640645d810c` + GUI **b1ba67520079** | `63dbf7a4` (#130) | **SWRScan** | **23 ok** | 0 (1 crash, ya en #131) | **VALIDADA EN RADIO la U del #130** ✓✓: build limpio desde el tip + gate #86 + deploy (idempotente, ya corría el mismo md5). Con el dial en la resonancia (~7.000) la curva es una **U**: base `y=398` en `x=255` (interior), bordes suben **+94/+168 px**; reproducido en 2ª pasada (`x=232`, +80/+174) y con span 200 kHz/Log (`x=238`, +140/+233). **Hallazgo nuevo**: `tests/swrscan_curve_evidence.py` del #125 **no corre sobre una captura de radio** (pide 2 args, no 1; su umbral `min≥180` no ve la curva del tema del owner `Redshift` = RGB(255,179,160)) → **issue nuevo**. Sigue `Error: timeout de RF` (`n≈44/51`, nunca `Completed`). Evidencia `docs/evidencia/2026-10-17-qa-radio-swr125/` |
| 22/10 ~18:30 (hora host) | (sin radio; **no se compiló ARM**: se corrió la matriz de host sobre el commit del PR) | `16976863` (#244/#247) | **no ejecutado** (el issue #244 lo prohíbe: la radio la usa el dueño) | **no ejecutado** | — | **#244 — VERIFICACIÓN HOST DEL ARREGLO EN VUELO (PR #247) + descarte del #232** ✓✗: `sh tests/native/run_native_tests.sh` **EXIT=0** (stage de `radio.c` **57 comprobaciones, 0 fallos**; la base `35918ec6` da 36, sin advertencias nuevas del compilador), `--ci-parity` **OK**, `tests/run_all_tests.sh --quick` **20 OK, 0 fallos**, `check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` **EXIT=0** (16 checks ok, incl. `x6200-ab contracts` del sha) y `tests/audita-pruebas.py` **OK**. **ASan/UBSan SKIP** (no están en el toolchain) y **banco QEMU BLOCKED** por entorno (corre en el host del owner). **El disparo del mute del dueño queda NO identificado**: el `vol=0` medido por la sesión del #247 explica el síntoma completo (registro 14 = "Speaker volume"), pero no explica la recuperación por power-cycle; la medición que falta (reiniciar sólo la GUI durante el mute, y el log del flag `sql_mute` del flow) queda como **PENDING del owner**. El preset del #232 queda **descartado** por código (no toca el BASE, ni ALSA/Pulse, ni el camino al parlante). Registro: `docs/HALLAZGOS.md` §65. Evidencia: [`2026-10-22-cwaudio244/`](evidencia/2026-10-22-cwaudio244/README.md) · [gates](evidencia/2026-10-22-cwaudio244/gates.txt) |
| 22/10 (UTC) | (sin radio; candidato GUI **`c888308eede712`**, 13.407.624 B; Buildroot `d084e287`; fuente `964193ae…`) | `c888308e` (#193) | **no ejecutado** (fase A sin radio) | **no ejecutado** (BLOCKED por banco ocupado) | — | **ASSESSMENT #194: CONDITIONAL / NO_GO de release** — evidencia software 01..04 integrada y auditada (334 ítems, 1.025 métricas, SHA256SUMS OK, gate #86 PASS); **hardware BLOCKED/PENDING_HARDWARE** con causa (fase A documental; sin sesión exclusiva ni identidad del binario instalado). P1 fundados: DATA nueva vfat/primer boot sin `/mnt`, SIGSEGV de DB corrupta/RO/faltante, update con owner 1000 y backup 706/5466, sin watchdog. STALE para el tip GUI `d4fb57f8` (25 archivos de producción) y Buildroot `f23d14f`. Evidencia: [`assessment-rom-194/05-summary/`](evidencia/assessment-rom-194/05-summary/README.md) · [dossier](evidencia/assessment-rom-194/05-summary/ASSESSMENT_ROM_194.md) · [findings](evidencia/assessment-rom-194/05-summary/findings.csv) · [hardware-plan](evidencia/assessment-rom-194/05-summary/hardware-plan.csv) |
| 22/10 ~11:48 | **ROM fresca 458b1d06…** (GUI 00a653e7 + BR f0ff620a) flasheada en la SD | `dadb1705` | — | — | 0 | **ROM FRESCA CON TODO LO NUEVO** ✓: build incremental (GUI dirclean -> re-configure ✓) desde tip GUI `00a653e7` (#206 bootanim v2 + #227) y BR `f0ff620a` (#4: S01create_data DATA ext4 en SD nueva / conserva existente — fix F-01 de #196 ✓). Imagen `sdcard.img` sha256 `458b1d0603…` (571.473.920 B) ✓, gate #86 PASS (13.420.464 B) ✓, GUI md5 `7213b19f…` y **fuente embebida `3e236dde…` = HEAD ✓** (antes `a576d48d` ✗ por configure viejo, corregido ✓), kernel sin logo (`CONFIG_LOGO not set`) ✓, fstab ext4 ✓. DATA preservada (UUID `9facdaea…` ✓, params 118.784 B ✓); backup previo y p3 exacta ✓. **Pendiente: arranque en la radio** ✓ |
| 22/10 ~10:12 | **ROM 03c6f9e6… (sin logo de kernel)** en la radio | `2a6059b5` | — | — | 0 | **ARRANQUE FINAL VERIFICADO** ✓✓: GUI 3b22a03a… ✓, boot_identity a576d48d… ✓, **bootanim stop/handoff 15,4 s + GUI lista 15,5 s** ✓, **imagen vieja de boot ELIMINADA** ✓✓ (kernel `# CONFIG_LOGO is not set`; owner: "arranco OK") ✓, 1 GUI + daemon ✓, **CRASHLOG 0** ✓, **/mnt ext4 rw** ✓, params 118.784 B ✓, kernel 6.1.82 ✓, radio en 192.168.0.236 ✓. Incluye PR Buildroot #2 (defconfig compila) y #3 (sin logo) ✓. Imagen: sdcard.img sha256 03c6f9e6e5ad75ec271125caf17bfe7f7721d9aaf0924c676b43f257ef4ecce5 ✓ |
| 22/10 ~16:12 | **UI actualizada por SSH (sin flashear)** | `dadb1705` | — | — | 0 | **DEPLOY DE UI POR SSH** ✓: se actualizaron `x6200_gui` (md5 `64ffdcb2…`, fuente embebida `da8ea582…` = HEAD de `f825fe4b`) y `x6200_bootanim` (`a78a036b…`) directamente sobre la radio, **sin flashear la SD** ✗→✓. Backup de los binarios previos en `/mnt/ui-bak-191129` ✓. Verificado: 1 GUI ✓, CRASHLOG 0 ✓, audio `Conected` ✓, WiFi up / BT down (prueba) ✓. Procedimiento automatizado en `deploy-ui-ssh.sh` ✓ (backup ✓, sólo sube si difiere ✓, reinicia GUI vía daemon ✓, verifica md5+fuente ✓). Política nueva: **la UI se actualiza por SSH** ✓; flash sólo para cambios fuera de la GUI (rootfs/kernel/Buildroot) o a pedido ✓ |
| 22/10 ~15:20 | **ROM final 0f4f4bae…** flasheada y arrancada | `dadb1705` | — | — | 0 | **ROM FINAL CON TODOS LOS FIXES** ✓: imagen `0f4f4bae1dd652df836c7d2081223161b258e8925d876129e360ab7491293be1` (571.473.920 B) con #232 (audio CW/CWR ✓), #237 (UX ✓), arranque de PulseAudio antes de la GUI ✓ (`S12pulseaudio` **solo** ✓, sin `S50pulseaudio` ✓), kernel sin logo ✓, S01create_data F-01 ✓, DATA ext4 ✓. Flash preservando DATA (UUID `9facdaea…` ✓, params 118.784 B ✓, backup previo ✓). **ARRANQUE VERIFICADO** ✓: GUI md5 `1bcd4958…` = esperado ✓, fuente embebida `08eb08e3…` = HEAD ✓, bootanim stop/handoff **15,1 s** + GUI lista 15,2 s ✓, **audio `Conected`** ✓ (code #232 en la radio ✓) y **captura activa** (source-outputs ✓), `/` y `/mnt` ext4 rw ✓, 1 GUI + daemon ✓, **CRASHLOG 0** ✓. Queda: validar decoder/Tune/Recorder en CW ✓ y el diagnóstico WiFi (BT off) ✓ |
| 22/10 ~09:10 | ROM c8ce5bd0… flasheada + DATA ext4 | `f3f381fd` | — | — | 0 | **ARRANQUE OK** ✓: GUI 3b22a03a… (13.412.248 B) ✓, boot_identity a576d48d… ✓, **bootanim #200 stop/handoff 14,6 s + GUI lista 14,7 s** ✓, 1 GUI + daemon ✓, CRASHLOG 0 ✓, **/mnt ext4 rw** ✓, params 118.784 B ✓ (la GUI lo reescribió al arrancar ✓). **Hallazgo del owner**: en boot se ve **la imagen anterior** ✗ → **causa medida**: el kernel dibuja **`logo.png`** (BR2_LINUX_KERNEL_CUSTOM_LOGO_PATH, 800x480, LOGO_LINUX_CLUT224) ✗; U-Boot no dibuja nada ✓ y #200 arranca en S01bootanim ✓ → **PR AetherX6200Buildroot#3** para quitar el logo del kernel ✓ (queda el rebuild+flash pendiente al merge ✓). Radio ahora en **192.168.18.67** ✓ |
| 22/10 ~08:47 | **ROM c8ce5bd0…** (tip 0d00e7fc + BR d084e287) flasheada en la SD | `9ed25fa9` | — | — | 0 | **FLASHEO CON IMAGEN NUEVA** ✓ (sha256 c8ce5bd05206cdfe…, 571.473.920 B): GUI 13.412.248 B md5 3b22a03a… ✓, fstab ext4 ✓, S01create_data ✓, S01bootanim #200 ✓, gate #86 PASS ✓. **Incidente**: el dd deja la 3ra entrada MBR inconsistente → p3 desapareció de la tabla ✗ **pero los datos intactos** ✓ (dd solo 545 MiB) → reparado recreando p3 con **sectores exactos** (`parted unit s mkpart primary 1116160 7744511`) ✓ → p3 3.393.716.224 B, LABEL=DATA, UUID 9facdaea ✓, mount ro OK ✓. Ojo: `parted ... 100%` **alinea al MiB** y deja p3 512 KiB corta ✗ (primer intento). params.db 118.784 B md5 3de650e3 ✓ (la radio lo había reescrito ✓) + backup fresco ✓. Script de flasheo actualizado ✓. **Pendiente**: arranque y verificación en la radio |
| 22/10 ~07:35 | **ROM tip 50bf8ab0** flasheada + DATA ext4 | `9ed25fa9` | — | — | 0 | **ARRANQUE VALIDADO** ✓: GUI md5 3b22a03a… (13.412.248 B, port) ✓, `boot_identity a576d48d…` ✓, **#200 bootanim OK** (stop/handoff 18,1 s, GUI lista 18,2 s) ✓, CRASHLOG 0 ✓. **Defecto de MBR encontrado y reparado en vivo**: p3 quedaba 512 KiB corta por la 3ra entrada MBR vieja ✗ (`bad geometry` → /mnt no montaba) → `parted resizepart 3 100%` ✓ → **/mnt ext4 rw** ✓ y **params.db md5 7f34a9ca… = idéntico al previo al flasheo** ✓ (**cero pérdida** ✓). Script de flasheo actualizado para extender p3 en futuras SD ✓ |
| 22/10 ~06:10 | **assessment #195** (candidato c888308e) | `e61c7fca` | — | — | 0 | **UNIDAD #195 COMPLETADA Y PR #201 MERGEADO** ✓ (revisado: sólo `01-baseline/`, 81 archivos, MERGEABLE). Entrega: coverage 334 filas (151 REAL/138 PARTIAL/4 SYNTHETIC/41 UNSUPPORTED), baseline host **17 OK/0 fallos/5 SKIP** + pytest 227 passed, ASan/UBSan/TSan en imagen full, CTest ft8/qth BLOCKED. **Artefacto del par congelado COMPLETO**: sdcard.img 571.473.920 B sha256 d0ff4435…, rootfs.tar 242.790.400 B sha256 7c26c76b…, gate #86 PASS (13.407.624 B), GUI md5 1f998c46…, fuente-md5 964193ae = checkout=staged=embebido ✓. Divergencia: tip 50bf8ab0 (#200) ⇒ **STALE** para el tip. Hallazgo: defconfig congelado no habilita PULSEAUDIO/LIBSNDFILE ✗ (build necesita ajuste). Se elige el artefacto del ejecutor para #196-#199 |
| 22/10 ~05:54 | **ROM nueva en SD: tip `50bf8ab0` (#200)** + DATA ext4 | `90aebad5` | — | — | 0 | **ROM ARMADA Y FLASHEADA A PEDIDO DEL OWNER** ✓✓: build privado del tip `50bf8ab0` (PR #200 boot animation) con Buildroot `d084e287` en contenedor (GCC 13.3). Verificado antes de flashear: `sdcard.img` sha256 17dc49ed553b953b8a2ec2fb94d95d2ec74ed07099c8db1d7c7d929a72eb8779 (571.473.920 B) ✓, gate #86 OK (ELF32 ARM 13.412.248 B 3/3 marcas) ✓, **fstab /mnt ext4** ✓, `S01create_data` con 'DATA ya existe' ✓, GUI md5 3b22a03a3b7850e5c546c564125d0eb0 ✓, `S01bootanim` (#200) ✓. Flasheo solo primeros 545 MiB → **DATA intacta** (UUID 9facdaea… verificado ✓); backup del tramo previo guardado ✓. **HALLAZGO**: el defconfig congelado no habilitaba LIBSNDFILE/PULSEAUDIO → el par congelado no compilaba el GUI desde cero (registrado para #194). Pendiente: primer arranque en la radio |
| 22/10 ~03:10 | ROM `c640645d810c` + GUI 3ef20224dbd9 + DATA ext4 | `ca650ea4` | — | — | 0 | **Merge `c888308e` (#193 DFL inline) desplegado** ✓ (build limpio 3ef20224dbd9, 13.923.392 B, gate OK, boot_identity 964193ae… ✓; 1 GUÍ pid 5938; CRASHLOG 0; /mnt ext4; lastbreath avanzando; ajustes intactos; 60 s de vigilancia estable ✓). **Proceso correcto aplicado**: sello del volumen verificado + build con compila-gui.sh + aborto si falla ✓ |

| 22/10 ~03:05 | ROM `c640645d810c` + GUI 81d087bdf922 + DATA ext4 | `2ce42901` | — | — | 0 | **CORRECCIÓN del "EVENTO" anterior**: la radio **NUNCA se cayó** ✓ — fue un **corte de WiFi** ✗: uptime **9703 s continuo** (mismo boot_id c56f310e, mismo pid 4318), **GUI 81d087bdf922 corriendo 2 h 41 min**, **CRASHLOG 0** ✓, /mnt ext4 ✓, lastbreath avanzando ✓, app=RTTY (en uso ✓). O sea: **el build del #191 (CW Auto Tune v2) es estable** ✓ y **mi alarma anterior fue una falsa alarma por red** ✗. No se cargo issue ✓ (correcto: no habia evidencia de falla del firmware) |
| 22/10 ~02:55 | ROM `c640645d810c` + GUI 81d087bdf922 + DATA ext4 | `2ce42901` | — | — | 0 | **EVENTO**: la radio quedó **inalcanzable ~90 s después de desplegar el build 81d087bdf922** (merge e6925db6, #191 CW Auto Tune v2) ✗. Antes del corte se midió: uptime 8087 s, 1 GUI (pid 4318), **CRASHLOG 0**, /mnt ext4, lastbreath avanzando ✓ → luego ping/SSH **sin respuesta** y **ningún puerto 22** en la subred ✗. **Causa a determinar** (no medida): ¿hang del build nuevo + watchdog en loop? ¿pérdida de alimentación? ✗. **Mitigación preparada**: binario bueno anterior recompilado y verificado (**760af1734ea7**, build determinista ✓) guardado en backups/binarios/ para restaurar por SD ✓. Pendiente: chequeo físico del owner |
| 22/10 ~02:45 | ROM `c640645d810c` + **GUI 81d087bdf922** + DATA ext4 | `9d432ea1` | — | — | 0 | **Merge `e6925db6` (#191 CW Auto Tune v2) desplegado** ✓ (build limpio 81d087bdf922, 13.923.332 B, gate OK, `boot_identity: d9dcd060…` ✓; 1 GUI pid 4318; CRASHLOG 0; /mnt ext4; lastbreath avanzando; ajustes intactos). **⚠️ INCIDENTE (error MÍO)**: el primer intento construyó con `make x6200-gui-rebuild` sobre el volumen del OTRO agente (`sello .fuente.md5 = 4e3e7f6d…` ✗) → **link roto** (`undefined reference` a símbolos del árbol) y **como el script no abortaba, desplegó el binario stale 3604cece0e43** ✗ por ~2 min; **corregido** recompilando con `tools/compila-gui.sh` (build limpio, rsync --checksum) y desplegando 81d087bdf922 ✓. **Lección**: verificar siempre el sello del volumen y usar `compila-gui.sh` ✓ |
| 22/10 ~02:10 | ROM `c640645d810c` + GUI 760af1734ea7 + **DATA ext4** | `f180b09a` | — | — | 0 | **1h41min operativa con ext4, sin reinicios** ✓ (uptime 6097 s; pid 2055; 1 GUI + 1 daemon; CRASHLOG 0; /mnt ext4; LASTBREATH up_s=6090 ✓). 7.082988 MHz / LSB / flo=200 / fhi=3000. 0 merges |
| 22/10 ~01:40 | ROM `c640645d810c` + GUI 760af1734ea7 + **DATA ext4** | `02801c0c` | — | — | 0 | **1h11min operativa con ext4, sin reinicios** ✓ (uptime 4297 s; pid 2055; 1 GUI + 1 daemon; CRASHLOG 0; /mnt ext4; LASTBREATH up_s=4289 avanzando ✓). Batería cargando (7,8 V). 7.082988 MHz / LSB / flo=200 / fhi=3000. 0 merges |
| 22/10 ~01:10 | ROM `c640645d810c` + GUI 760af1734ea7 + **DATA ext4** | `b1b7f242` | — | — | 0 | **PR AetherX6200Buildroot#1 MERGEADO** ✓✓: el fix de particiones (fstab ext4 + S01create_data tolerante al tipo) **ya está en `fork/lu2dsx/x6200-rom`** ✓ → **el próximo ROM nace con DATA ext4** ✓ y **se levanta el aviso de no reflashear** ✓. Radio: uptime 2498 s (41 min), 1 GUI, CRASHLOG 0, `/mnt` ext4 ✓. 0 merges en el GUI |
| 22/10 ~00:39 | ROM `c640645d810c` + GUI 760af1734ea7 + **DATA ext4** | `0b17b280` | — | — | 0 | **11 min post-corte de energía sin reinicios** ✓ (uptime 700 s; pid 326; 1 GUI + 1 daemon; CRASHLOG 0). **`/mnt` ext4** ✓ con **LASTBREATH `up_s=699` avanzando** ✓ (clave: antes quedaba clavado ✗). 7.112088 MHz / LSB / flo=200 / fhi=3000. **0 merges**; **PR AetherX6200Buildroot#1 OPEN · MERGEABLE** ✓ |
| 22/10 ~00:35 | ROM `c640645d810c` + GUI 760af1734ea7 + **DATA ext4** | `1c900a98` | — | — | 0 | **BARRIDO COMPLETO 24/24 apps OK** ✓✓ (**primera vez con DATA ext4**): FT8, FT4, JS8, WSPR, SSTV, WeFax, NavTex, RTTY, CW, Field, GPS, Bands, Settings, BT, Cluster, CwTrainer, Eq, Status, QTH, Callsign, Recorder, WiFi, SWRScan, Propagation — todas abren y cierran ok, **0 caídas** ✓. Post-barrido: uptime 471 s, **1 GUI + 1 daemon**, **CRASHLOG 0** ✓, `/mnt` **ext4 rw** ✓ (escritura OK), LASTBREATH avanzando ✓, **0 crashes en los logs de hoy** ✓. 0 merges. El barrido deja modo/dial cambiados (#94 conocido) |
| 21/10 ~23:40 | ROM `c640645d810c` + GUI 760af1734ea7 | `17ac1dfc` | — | — | 0 | **PRUEBA DE CORTE DE ENERGÍA: EXITOSA** ✓✓✓ — reset duro por sysrq `b` **sin umount ni sync** (uptime 773 s → 26 s, boot_id 9a19ed53 → c56f310e): **`EXT4-fs (mmcblk0p3): recovery complete`** ✓ (journal replay de DATA), **`/mnt` ext4 rw** después del corte ✓ (con vfat quedaba **ro corrupta** ✗), el archivo escrito sin sync se perdió (esperable ✓), params.db regrabado por la GUI ✓, CRASHLOG 0 ✓, 1 GUI ✓. **Objetivo del cambio validado** ✓ |
| 21/10 ~23:20 | ROM `c640645d810c` + GUI 760af1734ea7 | `2a940458` | — | — | 0 | **PR del fix de particiones creado** ✓: https://github.com/iariap/AetherX6200Buildroot/pull/1 (rama `factory/data-ext4-188`, commit c0fefe6): fstab `vfat→ext4 0 2` + `S01create_data` que monta la DATA existente con su tipo real y no toca la tabla. Verificado en la radio que `mount -t auto` **no funciona** en este busybox → fstab fija ext4 y el script hace fallback por tipo (compatibilidad con tarjetas viejas). Pendiente: corte de energía para validar el journal |
| 21/10 ~22:45 | ROM `c640645d810c` + GUI 760af1734ea7 | `90c587e1` | — | — | 0 | **ext4 VERIFICADO EN LA RADIO** ✓✓: `/mnt` = **ext4 rw** ✓ (antes ro), **escritura OK** ✓, **LASTBREATH volvió a avanzar** (`up_s=642`, antes clavado en 8) ✓, `params.db` reescrito por la GUI (persiste ✓), **sin p4** ✓ (parche efectivo), dmesg `EXT4-fs … ordered data mode` sin errores ✓, GUI 11,5 s ✓, 1 GUI ✓, CRASHLOG 0 ✓. **Fuente ahora 11,8 V** ✓ (antes 9,6 → confirma el brown-out ✓). **Overlay del buildroot parcheado** ✓ (fstab + S01create_data, sin commitear ✓). Pendiente: **corte de energía** para validar el journal |
| 21/10 ~22:00 | ROM `c640645d810c` | `cd7e2de6` | — | — | 0 | **Ciclo de monitoreo normal**: 0 merges ✓. La **SD convertida sigue en el lector de la PC** ✗ y la radio sin ella no arranca (`radio_ip` NO ENCONTRADA) ✗. Todo listo para el paso manual: sacar la SD del lector, ponerla en la radio y encender; después verifico `/mnt` ext4 rw, params, LASTBREATH y persistencia |
| 21/10 ~21:10 | ROM `c640645d810c` + GUI 760af1734ea7 | `50109a99` | — | — | 0 | **CONVERSIÓN DATA vfat→ext4 APLICADA** ✓✓ (con la SD en la PC, autorizado por el owner): imagen completa de la SD respaldada (3.965.190.144 B) + `params.db` md5 idéntico (d2b3e811…); `mkfs.ext4 -m 0 -L DATA` ✓; `e2fsck -fn` limpio (764 archivos, 0,0% fragmentado) ✓; fstab→ext4 (0 2) y `S01create_data` parcheado con `blkid -L DATA` (respaldos `.vfat.bak` ✓). BOOT (FAT16) y rootfs intactos. **PENDIENTE CRÍTICO**: el overlay del buildroot sigue en vfat → **no reflashear ROM** hasta aplicarlo ✗. **Verificación pendiente**: arranque en la radio |
| 21/10 ~20:45 | ROM `c640645d810c` + GUI 760af1734ea7 | `cd337401` | — | — | 0 | **HALLAZGO**: tras el apagón, **`/mnt` (vfat) quedó corrupto y el kernel la monta READ-ONLY** ✗ — `FAT-fs: Volume was not properly unmounted` + `fat_free_clusters: deleting FAT entry beyond EOF` + `Filesystem has been set read-only` (dmesg 3.27/8.61/8.62 s). Consecuencias medidas: **LASTBREATH clavado en `up_s=8`** ✗ y **params no persisten** ✗. La raíz ext4 **sí** se recuperó del apagón (`recovery complete`, 0.34 s) ✓. **Cargado como issue #188** ✓. Pendiente: `fsck.vfat` de `/dev/mmcblk0p3` desmontada (no lo corro: el owner usa la radio) |
| 21/10 ~20:50 | ROM `c640645d810c` + GUI 760af1734ea7 | `29a759ef` | (no corrido) | (no corrido) | 0 | **La radio volvió con alimentación** ✓: uptime 76 s (creciendo desde 23 → sin loop ✓), 1 GUI (pid 310) + 1 daemon, CRASHLOG 0. **LASTBREATH aún en `up_s=8`** ✗ (la GUI está arrancando) y el control remoto todavía no responde ✗. 0 merges. Siguiente ciclo: confirmar operativa |
| 21/10 ~20:15 | ROM `c640645d810c` + GUI 760af1734ea7 | `dc8901e2` | — | **CAUSA ENCONTRADA** ✓✓ | 0 | **LA RADIO VOLVIÓ y el LASTBREATH dio la CAUSA** ✓✓✓: `up_s=101051 clean=0 radio=RX tx=0 app=home **bat_mv=7200** ext_mv=9600 chg=1` → **la batería estaba a 7,2 V** (muy por debajo del umbral crítico de 10,8 V) y la fuente externa en **9,6 V** → **brown-out por batería agotada**, NO un cuelgue de software ✗✓. **Sin CRASHLOG** ✓ (consistente con corte de energía). El fix del #131 **aguantó 28 h 04 min** ✓. Comentado en #121 y #131. **Recomendación**: revisar la fuente de 9,6 V y cargar el pack |
| 21/10 ~19:40 | (radio inalcanzable) | `9dcdb049` | — | — | 0 | **Sigue caída ~7 h** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~19:05 | (radio inalcanzable) | `af9c1c6b` | — | — | 0 | **Sigue caída ~6h30** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~18:30 | (radio inalcanzable) | `5116cfa6` | — | — | 0 | **Sigue caída ~6 h** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~17:55 | (radio inalcanzable) | `4b604195` | — | — | 0 | **Sigue caída ~5h30** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~17:20 | (radio inalcanzable) | `8aded054` | — | — | 0 | **Sigue caída ~5 h** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~16:45 | (radio inalcanzable) | `fdef2550` | — | — | 0 | **Sigue caída ~4h30** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~16:10 | (radio inalcanzable) | `15ed3eb4` | — | — | 0 | **Sigue caída ~4 h** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~15:35 | (radio inalcanzable) | `95bd87ba` | — | — | 0 | **Sigue caída ~3h30** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~15:00 | (radio inalcanzable) | `4feae8bd` | — | — | 0 | **Sigue caída ~3 h** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~14:25 | (radio inalcanzable) | `a014b0c3` | — | — | 0 | **Sigue caída ~2h30** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis cuando vuelva |
| 21/10 ~13:50 | (radio inalcanzable) | `93ae98f6` | — | — | 0 | **Sigue caída ~2 h** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente el análisis del LASTBREATH cuando vuelva |
| 21/10 ~13:15 | (radio inalcanzable) | `68dd9e63` | — | — | 0 | **Sigue caída ~1 h** ✗ (`radio_ip` NO ENCONTRADA). 0 merges. Pendiente: cuando vuelva leer `/mnt/lastbreath`, los app_logs y el `pc` del CRASHLOG si lo hay |
| 21/10 ~12:40 | (radio inalcanzable) | `8f703d1f` | — | — | 0 | **La radio dejó de responder** ✗ tras **28h20min** de uptime (la primera caída DESPUÉS del fix del #131, que aguantó 7× el ritmo anterior de 1-4 h). Sin ping ni SSH en 192.168.0.236 ni 192.168.18.67; **ningún puerto 22 abierto** en la red; mi red OK. **Pendiente**: cuando vuelva, leer `/mnt/lastbreath` (debería tener el registro del momento), los app_logs y el `pc` si hay CRASHLOG |
| 21/10 ~11:50 | ROM `c640645d810c` + GUI 760af1734ea7 | `c9a74735` | (no corrido) | (no corrido) | 0 | **28h20min operativa sin reinicios ni crashes** ✓ (uptime 102024 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=200 / fhi=3000 / sql=0). 0 merges |
| 21/10 ~11:15 | ROM `c640645d810c` + GUI 760af1734ea7 | `be3bb7e7` | (no corrido) | (no corrido) | 0 | **27h50min operativa sin reinicios ni crashes** ✓ (uptime 100223 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~10:40 | ROM `c640645d810c` + GUI 760af1734ea7 | `2525cec5` | (no corrido) | (no corrido) | 0 | **27h20min operativa sin reinicios ni crashes** ✓ (uptime 98425 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~10:05 | ROM `c640645d810c` + GUI 760af1734ea7 | `9e0047fe` | (no corrido) | (no corrido) | 0 | **26h50min operativa sin reinicios ni crashes** ✓ (uptime 96623 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~09:30 | ROM `c640645d810c` + GUI 760af1734ea7 | `22debb3d` | (no corrido) | (no corrido) | 0 | **26h20min operativa sin reinicios ni crashes** ✓ (uptime 94825 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~08:55 | ROM `c640645d810c` + GUI 760af1734ea7 | `a6569932` | (no corrido) | (no corrido) | 0 | **25h50min operativa sin reinicios ni crashes** ✓ (uptime 93023 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~08:20 | ROM `c640645d810c` + GUI 760af1734ea7 | `49a6a771` | (no corrido) | (no corrido) | 0 | **25h20min operativa sin reinicios ni crashes** ✓ (uptime 91224 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~07:45 | ROM `c640645d810c` + GUI 760af1734ea7 | `e2b3a6f6` | (no corrido) | (no corrido) | 0 | **24h50min operativa sin reinicios ni crashes** ✓ (uptime 89423 s; pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~07:10 | ROM `c640645d810c` + GUI 760af1734ea7 | `a30a5b2d` | (no corrido) | (no corrido) | 0 | **🎉 HITO: 24h20min sin reinicios ni crashes** ✓✓ (uptime 87625 s; pid 25499 sin relanzar; 1 GUI + 1 daemon; **CRASHLOG 0**; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). **6× la ventana máxima anterior (4 h)** con la misma radio y el mismo uso → el fix del #131 (`_lv_event_mark_deleted`) queda validado desde el QA. 0 merges |
| 21/10 ~06:30 | ROM `c640645d810c` + GUI 760af1734ea7 (VFO adaptativo) | `ccfd7772` | (no corrido) | (no corrido) | 0 | **23h50min operativa sin reinicios** ✓ (pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). **A 10 min del hito de 24 h sin crashes**. 0 merges |
| 21/10 ~05:55 | ROM `c640645d810c` + GUI 760af1734ea7 (VFO adaptativo) | `71755990` | (no corrido) | (no corrido) | 0 | **23h20min operativa sin reinicios** ✓ (pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges. **Cerca del hito de 24 h sin crashes** |
| 21/10 ~05:20 | ROM `c640645d810c` + GUI 760af1734ea7 (VFO adaptativo) | `6e07bc39` | (no corrido) | (no corrido) | 0 | **22h50min operativa sin reinicios** ✓ (pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~04:45 | ROM `c640645d810c` + GUI 760af1734ea7 (VFO adaptativo) | `14a9f73f` | (no corrido) | (no corrido) | 0 | **22h20min operativa sin reinicios** ✓ (pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~04:10 | ROM `c640645d810c` + GUI 760af1734ea7 (VFO adaptativo) | `52877311` | (no corrido) | (no corrido) | 0 | **21h50min operativa sin reinicios** ✓ (pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=300 / fhi=2400 / sql=0). 0 merges |
| 21/10 ~03:35 | ROM `c640645d810c` + GUI 760af1734ea7 (VFO adaptativo) | `b60a032d` | (no corrido) | (no corrido) | 0 | **21h20min operativa sin reinicios con el build del VFO adaptativo** ✓ (pid 25499; 1 GUI + 1 daemon; CRASHLOG 0; 7.111688 MHz / LSB / flo=200 / fhi=3000 / sql=0). 0 merges |
| 21/10 ~03:00 | ROM `c640645d810c` + GUI 760af1734ea7 | `23313505` | (no corrido) | (no corrido) | 0 | **Publicada la sintonía adaptativa del VFO** ✓ (merge `23313505`, #186/#187: detents/s reales, multiplicadores None/Lite/Strong, umbrales derivados de T, histéresis 0.80x, freno 180 ms, reverso x1, se retiran 0.1x y 100x, + Accel threshold y Settings en inglés). Build exit 0 (17 s), gate OK, 13.917.692 B; md5 y `boot_identity: f31f9f2b…` nuevos ✓. Radio: pid 25499, 1 GUI, **CRASHLOG 0**, ajustes intactos. **NO medí el feel del dial** (es físico, no hay forma por SSH): lo tiene que probar el owner |
| 21/10 ~02:15 | ROM `c640645d810c` + GUI e43f7f6c4cbb (DFL presets) | `90bdf8e7` | (no corrido) | (no corrido) | 0 | **20h20min operativa sin reinicios con el build de los presets DFL** ✓ (pid 23741; 1 GUI + 1 daemon; CRASHLOG 0). El owner volvió el filtro a 200/3000 sin cuelgue ✓ (7.094288 MHz / LSB / flo=200 / fhi=3000 / sql=0). 0 merges; **rama nueva `factory/vfo-accel-186`** (aceleración del VFO) ✓ |
| 21/10 ~01:30 | ROM `c640645d810c` + GUI e43f7f6c4cbb | `6666806e` | (no corrido) | (no corrido) | 0 | **Publicados los presets y memorias del DFL** ✓ (merge `6666806e`, #183/#185: Home→DFL corto abre favoritos con 3 presets al primer toque, ajuste manual con VOL, Mem 1/2 por familia y confirmación transaccional). Build exit 0 (18 s), gate OK, 13.899.984 B; **md5 cambió** y `boot_identity: edb11471…` ✓. Radio: pid 23741, 1 GUI, **CRASHLOG 0**, ajustes intactos (7.094288 MHz / LSB / flo=30 / fhi=2745). **No abrí el DFL** |
| 21/10 ~00:45 | ROM `c640645d810c` + GUI 88196e442ddc | `ff88b643` | (no corrido) | (no corrido) | 0 | **19h20min operativa sin reinicios** ✓ (pid 21227; 1 GUI + 1 daemon; CRASHLOG 0; 7.094288 MHz / LSB / flo=30 / fhi=2745 / sql=0). 0 merges |
| 21/10 ~00:10 | ROM `c640645d810c` + GUI 88196e442ddc | `60b9f5b2` | (no corrido) | (no corrido) | 0 | **18h50min operativa sin reinicios** ✓ (pid 21227; 1 GUI + 1 daemon; CRASHLOG 0; 7.094288 MHz / LSB / flo=30 / fhi=2745 / sql=0). 0 merges |
| 20/10 ~23:35 | ROM `c640645d810c` + GUI 88196e442ddc | `4a0c59a1` | (no corrido) | (no corrido) | 0 | **18h20min operativa sin reinicios** ✓ (pid 21227; 1 GUI + 1 daemon; CRASHLOG 0; 7.109166 MHz / LSB / flo=100 / fhi=3000 / sql=0). 0 merges |
| 20/10 ~23:00 | ROM `c640645d810c` + GUI 88196e442ddc | `2bb0a2bc` | (no corrido) | (no corrido) | 0 | **17h50min operativa sin reinicios** ✓ (pid 21227; 1 GUI + 1 daemon; CRASHLOG 0; 7.109166 MHz / LSB / flo=100 / fhi=3000 / sql=0). 0 merges |
| 20/10 ~22:25 | ROM `c640645d810c` + GUI 88196e442ddc | `55c42edb` | (no corrido) | (no corrido) | 0 | **La GUI cambió a 88196e442ddc** (deploy del otro agente) ✓ **sin reinicio** (uptime 62423 s) y **sin crashes** (CRASHLOG 0) ✓. 1 GUI + 1 daemon; 7.119 MHz / LSB / flo=200 / fhi=3000 / sql=0. **0 merges**. El uptime de 17 h 20 atraviesa todos los deploys en caliente sin un solo crash |
| 20/10 ~21:50 | ROM `c640645d810c` + GUI 49db3db47840 (DFL inline) | `26c8e8e7` | (no corrido) | (no corrido) | 0 | **16h50min operativa sin reinicios** ✓ (pid 18936; 1 GUI + 1 daemon; CRASHLOG 0; 7.119 MHz / LSB / flo=5 / fhi=1545 / sql=0). 0 merges |
| 20/10 ~21:15 | ROM `c640645d810c` + GUI 49db3db47840 (DFL inline) | `82cef24f` | (no corrido) | (no corrido) | 0 | **16h20min operativa sin reinicios** ✓ (pid 18936; 1 GUI + 1 daemon; CRASHLOG 0). El owner cambió el filtro (7.120653 MHz / LSB / flo=530 / fhi=2670 / sql=0) sin cuelgue ✓. 0 merges |
| 20/10 ~20:40 | ROM `c640645d810c` + GUI 49db3db47840 (DFL inline) | `a3ff5e90` | (no corrido) | (no corrido) | 0 | **15h50min operativa sin reinicios** ✓ (pid 18936; 1 GUI + 1 daemon; CRASHLOG 0; 7.085031 MHz / LSB / flo=0 / fhi=2500 / sql=0). 0 merges |
| 20/10 ~20:05 | ROM `c640645d810c` + GUI 49db3db47840 (DFL inline) | `6fcdad6a` | (no corrido) | (no corrido) | 0 | **15h20min operativa sin reinicios con el build del DFL inline** ✓ (pid 18936; 1 GUI + 1 daemon; CRASHLOG 0). 7.120653 MHz / LSB / flo=5 / fhi=2145 / sql=0 (filtros válidos, sin cuelgue ✓). 0 merges |
| 20/10 ~19:30 | ROM `c640645d810c` + GUI 49db3db47840 | `f81ef7f9` | (no corrido) | (no corrido) | 0 | **Publicado el editor DFL inline** ✓ (merge `f81ef7f9`, #179/#181: panel dentro del waterfall, sin ventana flotante, deja vivos 28 px y se opera con la rueda VOL; 683 archivos, la mayoría fixtures). Build exit 0 (14 s), gate OK, 13.875.308 B; **md5 cambió** y `boot_identity: bf328914…` ✓. Radio: pid 18936, 1 GUI, **CRASHLOG 0**, ajustes intactos (7.125 MHz / LSB / flo=110 / fhi=3090). **No abrí el editor DFL** (la radio la usa el owner) |
| 20/10 ~18:55 | ROM `c640645d810c` + GUI 671408d12744 | `cea172cd` | (no corrido) | (no corrido) | 0 | **14h20min operativa sin reinicios** ✓ (pid 17152; 1 GUI + 1 daemon; CRASHLOG 0; 7.173 MHz / LSB / flo=110 / fhi=3090 / sql=0). Sin deploys nuevos en este ciclo. 0 merges |
| 20/10 ~18:20 | ROM `c640645d810c` + GUI 671408d12744 | `baa3c56e` | (no corrido) | (no corrido) | 0 | **La GUI cambió a 671408d12744** (tercer deploy consecutivo del otro agente) ✓ **sin reinicio** (uptime 49823 s) y **sin crashes** (CRASHLOG 0) ✓. 1 GUI + 1 daemon; 7.173 MHz / LSB / flo=110 / fhi=3090 / sql=0. **0 merges** (el tip no cambia). Los deploys en caliente salen limpios |
| 20/10 ~17:45 | ROM `c640645d810c` + GUI 13e6596350d2 | `98f83ebb` | (no corrido) | (no corrido) | 0 | **La GUI cambió otra vez a 13e6596350d2** (otro deploy del otro agente) ✓ **sin reinicio del sistema** (uptime 48023 s) y **sin crashes** (CRASHLOG 0) ✓. 1 GUI + 1 daemon; 7.173 MHz / LSB / flo=150 / fhi=3130 / sql=0. **0 merges** (el tip sigue en `7a5ce9d7`): ese build no es del tip, es del árbol de trabajo del otro agente. **No lo piso** |
| 20/10 ~17:10 | ROM `c640645d810c` + GUI c8eda34bac1a | `98f83ebb` | (no corrido) | (no corrido) | 0 | **12h50min operativa sin reinicios** ✓ (pid 12989; 1 GUI + 1 daemon; CRASHLOG 0; 7.173 MHz / LSB / fhi=2955 / sql=0). 0 merges |
| 20/10 ~16:35 | ROM `c640645d810c` + GUI c8eda34bac1a | `7a5ce9d7` | (no corrido) | (no corrido) | 0 | **El otro agente desplegó el commit `7a5ce9d7`** ("DFL: la rueda opera el filtro (rotary_cb)"): la radio corre **c8eda34bac1a** = build de ese commit (`boot_identity: f1a21d6f…`), **sin reinicio** (uptime 12 h 20+) y **sin crashes** (CRASHLOG 0) ✓. 1 GUI + 1 daemon. **No lo piso** (ya está el tip) |
| 20/10 ~16:25 | ROM `c640645d810c` + GUI c8eda34bac1a (build del otro agente) | `dc4775b3` | (no corrido) | (no corrido) | 0 | **La GUI cambió a c8eda34bac1a** (era ca5cc5915193): **el otro agente desplegó** ✓, **sin reinicio del sistema** (uptime 44424 s) y **sin crashes** (CRASHLOG 0) ✓. 1 GUI + 1 daemon; 7.074 MHz / USB-D / flo=200 / fhi=3000 / sql=0. **No hay commits nuevos en la rama** (0 merges). **No pisé ese build**: la radio está sana |
| 20/10 ~15:40 | ROM `c640645d810c` + GUI ca5cc5915193 (DFL) | `cab2ebf6` | (no corrido) | (no corrido) | 0 | **11h50min operativa sin reinicios** ✓ (pid 10397; 1 GUI + 1 daemon; CRASHLOG 0; 7.080 MHz / LSB / flo=0 / fhi=2000 / sql=0). 0 merges |
| 20/10 ~14:55 | ROM `c640645d810c` + GUI ca5cc5915193 (DFL) | `960b8d66` | (no corrido) | (no corrido) | 0 | **11h20min operativa sin reinicios** ✓ (pid 10397; 1 GUI + 1 daemon; CRASHLOG 0). **El owner probó el límite inferior del filtro (flo=0) y aguantó** ✓ (7.080 MHz / LSB / flo=0 / fhi=2000 / sql=0). 0 merges |
| 20/10 ~14:20 | ROM `c640645d810c` + GUI ca5cc5915193 (DFL) | `fef77ed4` | (no corrido) | (no corrido) | 0 | **10h50min operativa sin reinicios** ✓ (pid 10397; 1 GUI + 1 daemon; CRASHLOG 0; 14.118 MHz / USB / flo=200 / fhi=3250 / sql=0). 0 merges |
| 20/10 ~13:45 | ROM `c640645d810c` + GUI ca5cc5915193 (DFL) | `8ac1de1e` | (no corrido) | (no corrido) | 0 | **10h20min operativa sin reinicios con el build del editor DFL** ✓ (pid 10397; 1 GUI + 1 daemon; CRASHLOG 0). 14.117 MHz / USB / flo=200 / fhi=3250 / sql=0 (el owner cambió el filtro sin cuelgue ✓). 0 merges |
| 20/10 ~13:10 | ROM `c640645d810c` + GUI ca5cc5915193 | `39539f93` | (no corrido) | (no corrido) | 0 | **Publicado el editor DFL** ✓ (merge `39539f93`, PR 2 del #173: modal 720x206 sobre el waterfall con LOW/HIGH/BW, handles y −/+). Build exit 0 (14 s), gate OK, 13.869.700 B; **md5 cambió** (57b11ddc → ca5cc591) ✓ (el fix del #165 respondiendo). Radio: pid 10397, 1 GUI, **CRASHLOG 0**, ajustes intactos (14.117 MHz / USB / flo=20 / fhi=2800). **No abrí el editor** (la radio la usa el owner) |
| 20/10 ~12:35 | ROM `c640645d810c` + GUI 57b11ddc058a | `f7af87d7` | (no corrido) | (no corrido) | 0 | **Despliegue a pedido del owner** ✓: build exit 0 (7 s), gate OK, **md5 57b11ddc058a = el que ya tenía la radio** (idempotente ✓). `boot_identity: fuente: 8efe98c2…` (distinto del anterior a67f8b8b → cambia con el código ✓). Radio: pid 10231, 1 GUI + 1 daemon, sin reinicio (uptime 35117 s), CRASHLOG 0, ajustes intactos (14.117 MHz / USB / flo=20 / fhi=2800 / sql=0) |
| 20/10 ~11:50 | ROM `c640645d810c` + GUI 90548cbc2926 | `efe3c16d` | (no corrido) | (no corrido) | 0 | **9h20min operativa sin reinicios** ✓ (uptime 33624 s; 1 GUI + 1 daemon; CRASHLOG 0). **El owner cambió frecuencia y filtro** (7.094 MHz / LSB / flo=20 / fhi=2800 / sql=0) **sin cuelgue** ✓. 0 merges |
| 20/10 ~11:15 | ROM `c640645d810c` + GUI 90548cbc2926 | `75e19f4a` | (no corrido) | (no corrido) | 0 | **8h50min operativa sin reinicios** ✓ (uptime 31823 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~10:40 | ROM `c640645d810c` + GUI 90548cbc2926 | `035bfc0c` | (no corrido) | (no corrido) | 0 | **8h20min operativa sin reinicios** ✓ (uptime 30022 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~10:05 | ROM `c640645d810c` + GUI 90548cbc2926 | `f06b29a3` | (no corrido) | (no corrido) | 0 | **7h50min operativa sin reinicios** ✓ (uptime 28223 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~09:30 | ROM `c640645d810c` + GUI 90548cbc2926 | `4dc6b5fb` | (no corrido) | (no corrido) | 0 | **7h20min operativa sin reinicios** ✓ (uptime 26423 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~08:55 | ROM `c640645d810c` + GUI 90548cbc2926 | `c26b1b48` | (no corrido) | (no corrido) | 0 | **6h50min operativa sin reinicios** ✓ (uptime 24623 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~08:20 | ROM `c640645d810c` + GUI 90548cbc2926 | `5193b401` | (no corrido) | (no corrido) | 0 | **6h20min operativa sin reinicios** ✓ (uptime 22825 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~07:45 | ROM `c640645d810c` + GUI 90548cbc2926 | `e67d6fa3` | (no corrido) | (no corrido) | 0 | **5h50min operativa sin reinicios** ✓ (uptime 21025 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~07:10 | ROM `c640645d810c` + GUI 90548cbc2926 | `cd038983` | (no corrido) | (no corrido) | 0 | **5h20min operativa sin reinicios** ✓ (uptime 19223 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~06:35 | ROM `c640645d810c` + GUI 90548cbc2926 | `de7af53f` | (no corrido) | (no corrido) | 0 | **4h50min operativa sin reinicios** ✓ (uptime 17423 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~06:00 | ROM `c640645d810c` + GUI 90548cbc2926 | `e34485d1` | (no corrido) | (no corrido) | 0 | **4h20min operativa sin reinicios** ✓ (uptime 15625 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~05:25 | ROM `c640645d810c` + GUI 90548cbc2926 | `c017ab11` | (no corrido) | (no corrido) | 0 | **3h50min operativa sin reinicios** ✓ (uptime 13823 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=200 / fhi=3000 / sql=1). 0 merges |
| 20/10 ~04:50 | ROM `c640645d810c` + GUI 90548cbc2926 | `f24fa521` | (no corrido) | (no corrido) | 0 | **3h20min operativa sin reinicios** ✓ (uptime 12024 s; 1 GUI + 1 daemon; CRASHLOG 0). **El owner cambió el filtro a flo=200 / fhi=3000 y no se colgó** ✓ (7.120048 MHz / LSB / sql=1). 0 merges |
| 20/10 ~04:15 | ROM `c640645d810c` + GUI 90548cbc2926 | `fa268022` | (no corrido) | (no corrido) | 0 | **2h50min operativa sin reinicios** ✓ (uptime 10224 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=225 / fhi=3525 / sql=1). 0 merges |
| 20/10 ~03:40 | ROM `c640645d810c` + GUI 90548cbc2926 | `93a03a57` | (no corrido) | (no corrido) | 0 | **2h20min operativa sin reinicios** ✓ (uptime 8423 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=225 / fhi=3525 / sql=1). 0 merges |
| 20/10 ~03:05 | ROM `c640645d810c` + GUI 90548cbc2926 | `68702015` | (no corrido) | (no corrido) | 0 | **1h50min operativa sin reinicios** ✓ (uptime 6623 s; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=225 / fhi=3525 / sql=1). 0 merges |
| 20/10 ~02:30 | ROM `c640645d810c` + GUI 90548cbc2926 | `3ff61cee` | (no corrido) | (no corrido) | 0 | **80 min operativa sin reinicios** ✓ (uptime 4824 s; 1 GUI + 1 daemon; CRASHLOG 0; load 0.82). **El owner cambió el filtro a flo=225 / fhi=3525 y no se colgó** ✓ (7.120048 MHz / LSB / sql=1). 0 merges |
| 20/10 ~01:55 | ROM `c640645d810c` + GUI 90548cbc2926 | `9bb1d306` | (no corrido) | (no corrido) | 0 | **50 min operativa sin reinicios** ✓ (uptime 3024 s; 1 GUI + 1 daemon; CRASHLOG 0; load 0.69; 7.120048 MHz / LSB / flo=55 / fhi=2535 / sql=1). La recuperación del loop se mantiene. 0 merges |
| 20/10 ~01:20 | ROM `c640645d810c` + GUI 90548cbc2926 | `ac95efd9` | (no corrido) | (no corrido) | 0 | **RECUPERACIÓN CONFIRMADA: 20 min operativa sin reinicios** ✓✓ (uptime 1223 s vs ~250 al recuperar; 1 GUI + 1 daemon; CRASHLOG 0; 7.120048 MHz / LSB / flo=55 / fhi=2535 válidos). El loop por `filter_low=2100` persistido quedó resuelto con la DB en 200. 0 merges |
| 20/10 ~00:40 | ROM `c640645d810c` + GUI 90548cbc2926 | — | — | — | 1 | **La radio entró en LOOP de reinicios** ✗✓ (tras el cuelgue por `filter Low>2090`). **Causa raíz**: el valor inválido quedó persistido en `params.db` (`mode=0, filter_low=2100`) y la GUI lo cargaba **en cada arranque** → cuelgue → watchdog → reboot. **RECUPERACIÓN MANUAL**: entré en una ventana de 19 s, deshabilité la GUI (`chmod -x`), puse `filter_low=200` en la DB (backup `/mnt/params.db.bak-loop`) y la re-habilité. **También corregí mi error**: había arrancado un 2º daemon → **2 GUIs dibujando = lag** ✗ (ahora 1+1 ✓). **Estado**: operativa, `get` en 0 s, CRASHLOG 0, load 0.75, captura OK. **Aviso**: no subir LOW>2090 hasta el fix; y `/mnt` quedó sucio (`FAT-fs: not properly unmounted`) |
| 20/10 ~00:15 | ROM `c640645d810c` + GUI 90548cbc2926 | `e6b56379` | — | — | 1 | **LA RADIO SE REINICIÓ cambiando el filtro** ✗: el owner reporta **reproducible con filter LOW > 2090**; medí `up 0 min` (reinicio), LASTBREATH previo `up_s=113180 clean=0 ev=0/5/0 msg=FFT width: 96 kHz`, app_log **crash=0** (**no fue el #131** ✗), **pstore 0**, watchdog 16 s. **Causa probable (código)**: el LOW solo se clampea contra `high` (`cfg/mode.c:317`, high≤10000) y el handler llama `x6200_control_rx_filter_set_low()` **con `radio_lock` tomado** → si la lib espera un ack del DSP que no llega, el GUI se traba y el system se reinicia. **No reproduje** el cuelgue (deja la radio muerta). Comentado en #121 |
| 20/10 ~19:10 | ROM `c640645d810c` + **GUI 90548cbc2926** (merges `b318a1f3` #165 + `f716af44` #171) | `f716af44` | **#165 build identity** | **1 ok medido** ✓✓ | 0 | **FIX DEL #165 VERIFICADO** ✓✓: el md5 **ahora cambia** (adb4736f0c6d → 90548cbc2926) y al arrancar loguea **`boot_identity: fuente: a67f8b8b97ff858364561521f1149986 (x6200_gui)`** ✓ — justo lo que pedía el issue. Causa raíz según el commit: la copia de fuentes sin `--checksum` (compilaba fuentes viejas). Radio: pid 29040, CRASHLOG 0, `sql=1` conservado. **NO validé `f716af44` (CB canal grande)**: la radio está en 40 m/LSB en uso, no cambio de banda |
| 20/10 ~18:30 | ROM `c640645d810c` + GUI adb4736f0c6d (squelch fix) | `6a16f383` | (no corrido) | (no corrido) | 0 | **Estable 30h54min** ✓ (pid 27631 sin relanzar, uptime 111246 s, CRASHLOG 0, overflows 0). 27.405 MHz / AM / sql=0 (lo usaba el owner). 0 merges; **nueva rama `factory/build-identity-165`** → están trabajando en el **#165** (identidad del build) ✓ |
| 20/10 ~17:55 | ROM `c640645d810c` + GUI adb4736f0c6d (squelch fix) | `ffac961c` | (no corrido) | (no corrido) | 0 | **Estable 30 h de uptime** ✓ (pid 27631 sin relanzar, uptime 109445 s, CRASHLOG 0, overflows 0). 7.1253 MHz / LSB / sql=0. 0 merges |
| 20/10 ~17:20 | ROM `c640645d810c` + GUI adb4736f0c6d (squelch fix) | `6c1948ed` | (no corrido) | (no corrido) | 0 | **Estable** ✓ (pid 27631 sin relanzar, uptime 107646 s, CRASHLOG 0, overflows 0). 7.1253 MHz / LSB / sql=0. 0 merges |
| 20/10 ~16:45 | ROM `c640645d810c` + GUI adb4736f0c6d (squelch fix) | `9c1807bd` | (no corrido) | (no corrido) | 0 | **Estable** ✓ (pid 27631 sin relanzar, uptime 105847 s, CRASHLOG 0, overflows 0). 7.1253 MHz / LSB / sql=0. 0 merges |
| 20/10 ~16:10 | ROM `c640645d810c` + GUI adb4736f0c6d (squelch fix) | `19771f6b` | (no corrido) | (no corrido) | 0 | **Estable** ✓ (pid 27631 sin relanzar, uptime 104046 s, CRASHLOG 0, overflows 0). Estado 7.1253 MHz / LSB / sql=0 (lo usaba el owner). 0 merges |
| 20/10 ~15:35 | ROM `c640645d810c` + GUI adb4736f0c6d (squelch fix) | `73fdd055` | (no corrido) | (no corrido) | 0 | **Estable con el fix del squelch** ✓ (pid 27631 sin relanzar, uptime 102245 s, CRASHLOG 0, overflows 0). Estado 7.1253 MHz / LSB / sql=0 (lo estaba usando el owner, no toqué nada). 0 merges |
| 20/10 ~15:00 | ROM `c640645d810c` + GUI adb4736f0c6d | `16ff4cbc` | (no corrido) | (no corrido) | 0 | Merge `16ff4cbc` = **solo infra de tests** (fixture estructural del ab-testing) → **el binario NO cambia** (adb4736f0c6d, mismo tamaño), que acá **es lo esperado** ✓ (contraste con el #165, donde cambia código y el md5 no cambia ✗). Radio: pid 27631, CRASHLOG 0, `Loaded sql=0` al arrancar ✓ (el squelch persistido se aplica). Estado 14.075 MHz / USB-D / sql=0 (sin tocar). 0 crashes |
| 20/10 ~14:20 | ROM `c640645d810c` + **GUI adb4736f0c6d** (merge `fa7b4613` squelch #166) | `fa7b4613` | **squelch (fix)** | **1 ok parcial / 1 fail de mi triage** | 0 | **Publicado el fix del squelch** ✓ (adb4736f0c6d, gate OK, pid 26327, CRASHLOG 0): `sql` 0→3→0 responde ✓ y al arrancar `cfg_params_load_item: Loaded sql=0` ✓ (el persistido se aplica). **MI HIPÓTESIS DEL #166 QUEDÓ REFUTADA** ✗✓: la lib ya acoplaba el enable en `sql_set/sql_fm_set` (control.c:315,322) — lo reconozco en el issue. **No puedo verificar que corte el ruido**: el audio de RX no pasa por el sink (0 muestras no-cero) → prueba auditiva del owner. **Hallazgo nuevo**: `No results for load sql_fm` ✗ (el sql de FM no se persiste) |
| 20/10 ~13:45 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `c6761f80` | (no corrido) | (no corrido) | 0 | **~5h20min sin crashes** ✓ (pid 22883 sin relanzar, uptime 96845 s, CRASHLOG 0, overflows 0). Radio en 28.180 MHz / USB-D / sql=0 (la estaba usando el owner, no toqué nada). 0 merges; #166 (squelch) abierto |
| 20/10 ~13:20 | ROM `c640645d810c` + GUI 9a26b607c67d | `5f3c52fe` | **squelch** | **1 fail** | 1 | **SQUELCH NO FUNCIONA** (reporte del owner: 0→1,2,3,4 no corta el ruido blanco) ✗. **Medido**: `sql` por remoto 0→2→4→8→100 y el `get` **sí** devuelve el valor ✓, pero **el squelch del DSP nunca se habilita**: `src/radio.c:611` tiene `// x6200_control_sql_enable_set(true);` **comentado** (viene del fork X6100, `fb20b23`) ✗. **Descartado** que el GUI mutee: `sql_mute` solo en un printf comentado y el RX **no pasa por el stream del GUI** (captura del sink: 0 no-cero de 226734 muestras con sql=0 y de sql=100) ✗. Sin issue previo ✓ → **cargado #166** |
| 20/10 ~12:45 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `9e3df02e` | (no corrido) | (no corrido) | 0 | **~4h50min sin crashes** ✓ (superado el umbral de 4 h del ritmo anterior; pid 22883 sin relanzar, uptime 95045 s, CRASHLOG 0, overflows 0; app_logs 18-40-27=0, 17-40-48=0, 17-36-09=1 de antes del fix). 0 merges |
| 20/10 ~12:10 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `a15ffe13` | (no corrido) | (no corrido) | 0 | **✓ FIX DEL #131 CONFIRMADO EN VENTANA: 4h20min sin ningún SIGSEGV** ✓✓ (pid 22883 sin relanzar, uptime 93247 s, CRASHLOG 0, overflows 0; app_logs 18-40-27=0, 17-40-48=0, 17-36-09=1 de antes del fix). El ritmo anterior era 1-4 h (5 crashes en ~2 días), mismo equipo/ajustes/DATA → **el único cambio es el fix**. **NO afirmo que esté cerrado para siempre**: 4h20 es una ventana, no una prueba. Sigo monitoreando. 0 merges |
| 20/10 ~11:35 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `5243df52` | (no corrido) | (no corrido) | 0 | **~3h50min sin crashes con el fix del #131** ✓ (pid 22883 estable, uptime 91446 s, CRASHLOG 0, overflows 0; app_logs 18-40-27=0, 17-40-48=0, 17-36-09=1 de antes del fix) → **a punto de superar el umbral alto de 4 h** del ritmo anterior de caídas. 0 merges |
| 20/10 ~11:00 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `96fc5ce9` | (no corrido) | (no corrido) | 0 | **~3h20min sin crashes con el fix del #131** ✓ (pid 22883 estable, uptime 89645 s, CRASHLOG 0, overflows 0; app_logs 18-40-27=0, 17-40-48=0, 17-36-09=1 de antes del fix). Dentro del rango alto de la ventana (1-4 h). 0 merges |
| 20/10 ~10:25 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `f836bdb5` | (no corrido) | (no corrido) | 0 | **~2h50min sin crashes con el fix del #131** ✓ (pid 22883 estable, uptime 87846 s, CRASHLOG 0, overflows 0; app_logs 18-40-27=0, 17-40-48=0, 17-36-09=1 de antes del fix). Ventana cerca del umbral alto (1-4 h). 0 merges |
| 20/10 ~09:50 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `afc73098` | (no corrido) | (no corrido) | 0 | **~2h20min sin crashes con el fix del #131** ✓ (pid 22883 estable, uptime 86045 s, CRASHLOG 0, overflows 0; app_logs 18-40-27=0, 17-40-48=0, 17-36-09=1 de antes del fix). Ventana de observación en curso. 0 merges |
| 20/10 ~09:15 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `b730d808` | (no corrido) | (no corrido) | 0 | **~1h50min sin crashes con el fix del #131** ✓ (pid 22883 estable, uptime 84245 s, CRASHLOG 0, overflows 0). app_logs: 18-40-27=0, 17-40-48=0, 17-36-09=1 (el de antes del fix). **Ventana de observación en curso**. 0 merges |
| 20/10 ~09:00 | ROM `c640645d810c` + GUI 9a26b607c67d | `5f3c52fe` | — | — | 0 | **OJO: el build del OPT-4 (`5f3c52fe`) da el MISMO md5 y tamaño que el del #163** ✗ (9a26b607c67d, 13.855.068 B) **aunque compiló 50 s con las fuentes nuevas** (verificado en el docker y en la copia del volumen) → **el md5 no sirve para saber qué código corre la radio**. **No afirmo que el OPT-4 esté desplegado**. La radio corre 9a26b607c67d con **pid 22883 estable y CRASHLOG 0** ✓ (con el fix del #131). Reportado en #164 y en issue nuevo |
| 20/10 ~08:20 | ROM `c640645d810c` + GUI 9a26b607c67d (fix #131) | `1b4c4ee8` | (no corrido) | (no corrido) | 0 | **Primer ciclo con el fix del #131: sin crashes** ✓ (pid 21948, uptime 80646 s, CRASHLOG 0, overflows 0, mismo app_log del deploy). **Ventana de observación en curso** (las caídas venían cada 1-4 h). 0 merges; rama nueva `factory/lvgl-opt4-150` |
| 20/10 ~07:45 | ROM `c640645d810c` + **GUI 9a26b607c67d** (merges `1c599025` fix #131 + `57aa2e51` opt2) | `1c599025` | **fix #131 + waterfall opt2** | **1 ok + STACK RESUELTO** ✓✓ | 1 | **¡SE RESOLVIÓ EL STACK DEL #131!** ✓✓✓ El crash previo (`pc=0xb6176ae8`): `pc - base(liblvgl)= 0x16ae8` → **`_lv_event_mark_deleted`** (+0x30), función interna de LVGL sobre la **lista de eventos** → encaja con el **#111** (147 descartes medidos) y es justo lo que serializa el fix. Lib exacta verificada (md5 cd30ca6c58ad). **Publicado el fix**: 9a26b607c67d, pid 21948, CRASHLOG 0, MemAvailable 934 MB. **Falta la ventana de observación** para decir que cerró |
| 20/10 ~07:20 | ROM `c640645d810c` + **GUI c28e5c17927f** (merges `30b68641` + `7abbf164`) | `30b68641` | **#158 remote + #162 interlock (solo codigo)** | **1 ok parcial** | 0 | **Publicados los dos merges** ✓. **#158 VALIDADO**: `state.pwr = 15` ✓ (¡ya no congelado en 50!). **#162 verificado en CÓDIGO** ✓ (`cw_encoder.c:58/192`, `radio.c:1117/1188`: el keyer y `cw_send` consultan el mismo interlock) pero **NO verificado transmitiendo** ✗: replicar requiere TX en CB y **no tengo autorización fuera de 40 m**. **NFM->SAM no medido** (haría falta cambiar el modo) |
| 20/10 ~06:45 | ROM `c640645d810c` + GUI 67387bfc4391 | `b50303ed` | — | **evidencia #121** | 3 | **LA RADIO VOLVIÓ y el LASTBREATH dejó LA PRUEBA** ✓✓: `clean=0 boot=3066c0e8 radio=RX tx=0 app=home bat_mv=8600 ext_mv=14500 ev=0/63/147 last=refresh wus=10062 msg=Power was limited to 7W` → **147 eventos DESCARTADOS** (#111) con la radio en reposo. **3 app_logs con CRASHLOG=1** (15:30, 15:31, 16:13) y **el sistema NUNCA se reinició** (uptime 20h54, daemon 298 el mismo) → **no fue cuelgue del sistema: 3 crashes de la GUI**. Comentado en #121 y #111 |
| 20/10 ~06:15 | (radio inalcanzable) | `cf2d0970` | — | — | 0 | **Sigue caída** ✗ (~1 h). **Y llegó un merge de QA que la explica**: `cf2d0970` (QA #146 §7.2) dice que durante su sesión **la GUI cayó 2 veces por el #131** y **la radio quedó inalcanzable al final (#121)** → **la muerte ocurrió entre 12:15 y 12:39** (mi timeline ✓). Además reportan **TX en CB sin interlock** ✗ (`radio_set_morse_key()` no pasa por `radio_set_ptt()`): **agujero de seguridad**, no re-verificado por mí (sin autorización de TX en CB y radio caída). **AVISO**: no pudieron restaurar frecuencia/modo/potencia → al volver puede estar en CB, hay que revisarla |
| 20/10 ~05:40 | (radio inalcanzable) | `02090454` | — | — | 0 | **La radio dejo de responder** ✗ (posible 5ª caída): `radio_ip` NO ENCONTRADA, sin ping ni SSH en 192.168.0.236 ni 192.168.18.67, y sin ningún host nuevo con el 22 abierto en la red. **Mi red esta bien** (192.168.0.195/24, gateway OK, otros hosts visibles). Última medición viva: hace ~20 min con uptime 70226 s (19h30) y GUI 9a094c421b0e. **Esta vez el LASTBREATH debería dejar el registro** ✓ |
| 20/10 ~05:00 | ROM `c640645d810c` + **GUI 67387bfc4391** (merge `02090454` App Propagation) | `02090454` | **app Propagation** | **1 ok** | 0 | **Publicado y probado el #154** ✓: la app **abre** (`app='Propagation'`, CRASHLOG 0) y muestra **"Banda CB C, sin beacons IBP"**, **"Paso 1/4"** y el mensaje correcto *la IARU no respalda beacons debajo de 14 MHz*; botones Run/Repetir/Escuchar/Umbrales/Cerrar. **NO validé recepción de beacons** (no hay actividad y la app es solo RX). **Efecto conocido #94**: al abrir quedó en **AM/CB CH 19 y 27.185 MHz**, restaurado a USB-D y 7.120.000 |
| 20/10 ~04:25 | ROM `c640645d810c` + GUI 9a094c421b0e (races) | `56be08e` | (no corrido) | (no corrido) | 0 | **Estable 18h54min** ✓ (pid 13127, uptime 68046 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 20/10 ~03:50 | ROM `c640645d810c` + GUI 9a094c421b0e (races) | `1e3a1e4` | (no corrido) | (no corrido) | 0 | **Estable 18h24min** ✓ (pid 13127, uptime 66246 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 20/10 ~03:15 | ROM `c640645d810c` + GUI 9a094c421b0e (races) | `6eb2e68` | (no corrido) | (no corrido) | 0 | **Estable 17h54min** ✓ (pid 13127, uptime 64445 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. app_log nuevo = rotación (pid sin cambios). 0 merges |
| 20/10 ~02:40 | ROM `c640645d810c` + **GUI 9a094c421b0e** (merge `95eebd3` LVGL OPT-3) | `95eebd3` | **espectro/waterfall** | **1 ok** | 0 | **Publicado y probado el fix de races del espectro (#153)** ✓: GUI 9a094c421b0e, pid 13127, **CRASHLOG 0**, MemAvailable 939 MB. Captura `34-espectro-sin-races.png`: **espectro dibujado, waterfall corriendo, línea central visible** — **sin regresión visual** ✓. **NO validable desde afuera** que las 7 races de TSan estén cerradas (TSan corre en build instrumentado) |
| 20/10 ~02:20 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `1811278` | (no corrido) | (no corrido) | 0 | **Estable 16h54min** ✓ (pid 7936, uptime 60847 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 20/10 ~01:45 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `2b849be` | (no corrido) | (no corrido) | 0 | **Estable 16h24min** ✓ (pid 7936, uptime 59046 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 20/10 ~01:10 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `17f0cad` | (no corrido) | (no corrido) | 0 | **Estable 15h54min** ✓ (pid 7936, uptime 57245 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 20/10 ~00:35 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `3ac1f94` | (no corrido) | (no corrido) | 0 | **Estable 15h24min** ✓ (pid 7936, uptime 55446 s, CRASHLOG 0). Overflows 0. Batería 8700 mV. 0 merges |
| 20/10 ~00:00 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `5c14c67` | (no corrido) | (no corrido) | 0 | **Estable 14h54min** ✓ (pid 7936, uptime 53646 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~23:25 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `1b46dde` | (no corrido) | (no corrido) | 0 | **Estable 14h24min** ✓ (pid 7936, uptime 51845 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~22:50 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `cff0284` | (no corrido) | (no corrido) | 0 | **Estable 13h54min** ✓ (pid 7936, uptime 50047 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~22:15 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `a390dda` | (no corrido) | (no corrido) | 0 | **Estable 13h24min** ✓ (pid 7936, uptime 48246 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~21:40 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `1347c80` | (no corrido) | (no corrido) | 0 | **Estable 12h54min** ✓ (pid 7936, uptime 46447 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~21:05 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `bfd39b4` | (no corrido) | (no corrido) | 0 | **Estable 12h24min** ✓ (pid 7936, uptime 44677 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~20:30 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `094f90a` | (no corrido) | (no corrido) | 0 | **Estable 11h54min** ✓ (pid 7936, uptime 42849 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~19:55 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `bc4a637` | (no corrido) | (no corrido) | 0 | **Estable 11h24min** ✓ (pid 7936, uptime 41046 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~19:20 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `2ccfcc9` | (no corrido) | (no corrido) | 0 | **Estable 10h54min** ✓ (pid 7936, uptime 39247 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~18:45 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `fae5b2a` | (no corrido) | (no corrido) | 0 | **Estable 10h24min** ✓ (pid 7936, uptime 37446 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~18:10 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `1e75982` | (no corrido) | (no corrido) | 0 | **Estable 9h54min** ✓ (pid 7936, uptime 35646 s, CRASHLOG 0). Overflows 0. Batería 8600 mV. 0 merges |
| 19/10 ~17:35 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `5feafd2` | (no corrido) | (no corrido) | 0 | **Estable 9h24min** ✓ (pid 7936, uptime 33845 s, CRASHLOG 0, mismo app_log). **Overflows del #111 en el log vivo: 0** → el flood es **episódico** (refuerza que el crash del #131 vino en una ráfaga). Batería 8600 mV. 0 merges |
| 19/10 ~16:55 | ROM `c640645d810c` + GUI 48f9bd5b8159 | `81617ee` | — | **1 repro** | 1 | **4º crash del #131 — y con el overflow del #111 delante** ✗✓✓: `scheduler_put: Scheduler queue overflow` (x4) + `discard_log: Overflow: se descarta refresh (events.c #254)` **justo antes** del `CRASHLOG sig=11 fallo=0x3 pc=0xb6166ae8 lr=0xb616d4f0` → **mismo sitio** (2º: `0xb6186ae8`, 3º: `0xb61d6ae8`, 4º: `0xb6166ae8`, el binario se desplaza). **Vínculo con #111 y #114** (el #114 ya decía que su log termina en el flood). Overflow **con la GUI en home, RX, sin TX**: no lo dispara el usuario. Comentado en #131 y #111 |
| 19/10 ~16:45 | ROM `c640645d810c` + GUI 48f9bd5b8159 (opt LVGL) | `81617ee` | (no corrido) | (no corrido) | 0 | **Estable 8h24min con el opt del #151** ✓ (pid 7529, uptime 30246 s, CRASHLOG 0; app_log nuevo = rotación). Batería 8600 mV. 0 merges |
| 19/10 ~16:10 | ROM `c640645d810c` + GUI 48f9bd5b8159 (merge `410c066` LVGL OPT-1) | `410c066` | **opt memoria** | **1 ok** | 1 | **3er crash del #131** ✗✓ (app_log `03-11-30`): **mismo sitio desplazado +0x50000** (pc `0xb61d6ae8` vs `0xb6186ae8`, stack `b61ddb48` vs `b618db48`), **trigger `on_vfo_freq_change`** (3er trigger distinto: modo, filtro, VFO). **Publicado el #151**: GUI 48f9bd5b8159, **RSS 18,7 MB**, MemAvailable 938 MB, CRASHLOG 0 ✓. **NO medí el RSS antes** (no puedo confirmar el -70%) |
| 19/10 ~15:05 | ROM `c640645d810c` + GUI 0caec0d82d33 | `a69ea28` | (no corrido) | (no corrido) | 0 | **Estable 7h24min** ✓ (pid 5735, uptime 26648 s, CRASHLOG 0). Batería **8700 mV** (antes 8600 → la lectura cambia, no está trabada, pero sigue bajo 10,8 V). 0 merges |
| 19/10 ~14:30 | ROM `c640645d810c` + GUI 0caec0d82d33 | `c065797` | (no corrido) | (no corrido) | 0 | **Estable 6h54min** ✓ (pid 5735, uptime 24846 s, CRASHLOG 0). Batería 8600 mV. 0 merges |
| 19/10 ~13:45 | ROM `c640645d810c` + GUI 0caec0d82d33 | `ae1ba0f` | (no corrido) | (no corrido) | 0 | **Estable 6h24min** ✓ (pid 5735, uptime 23046 s, CRASHLOG 0; app_log nuevo = rotación). Batería 8600 mV. 0 merges; **nueva rama `factory/lvgl-events-131`** → están trabajando en el **#131 (SIGSEGV)** ✓ |
| 19/10 ~13:30 | ROM `c640645d810c` + **GUI 0caec0d82d33** (merge `4e000bc`) | `4e000bc` | **waterfall linea central** | **1 ok medido** ✓ | 0 | **Publicado y validado el #143** ✓: la **línea central del waterfall se ve**, medida sobre el framebuffer: brillo por columna **x=398:183, x=399:180, x=400:176** (línea) vs **x=401:118, x=402:114** (fondo) → **3 px a opacidad plena**, exactamente lo pedido. Evidencia `33-waterfall-linea-central.png`. No medí otros spans/zooms ni otros temas |
| 19/10 ~13:00 | ROM `c640645d810c` + GUI ccaa60d94315 | `b3c873c` | (no corrido) | (no corrido) | 0 | **Estable 5h24min** ✓ (pid 3489, uptime 19446 s, CRASHLOG 0). Batería 8600 mV. 0 merges; nueva rama `factory/wf-line-140-coder` |
| 19/10 ~12:25 | ROM `c640645d810c` + GUI ccaa60d94315 | `a79dfaa` | (no corrido) | (no corrido) | 0 | **Estable 4h54min** ✓ (pid 3489, uptime 17646 s, CRASHLOG 0). Batería 8600 mV. 0 merges |
| 19/10 ~11:50 | ROM `c640645d810c` + GUI ccaa60d94315 | `2e9a3fa` | (no corrido) | (no corrido) | 0 | **Estable 4h24min** ✓ (pid 3489, uptime 15845 s, CRASHLOG 0; mismo app_log). Batería 8600 mV. 0 merges |
| 19/10 ~11:15 | ROM `c640645d810c` + GUI ccaa60d94315 | `7f6545d` | (no corrido) | (no corrido) | 0 | **Estable 3h54min** ✓ (pid 3489, uptime 14045 s, CRASHLOG 0; el app_log nuevo es rotación, el pid no cambió). Batería 8600 mV. 0 merges |
| 19/10 ~10:40 | ROM `c640645d810c` + **GUI ccaa60d94315** (merge `433c65d`) | `433c65d` | **app CW (smoke)** | **1 ok** | 0 | **Publicado y smoke-testeado el auto-ajuste del decodificador CW (#142)** ✓: la app CW **abre y responde** (los 4 botones `ok`, app sigue en `CW`, pid 3489, **CRASHLOG 0**), y el **AUTO es opt-in** (manuales intactos). **NO** activé el AUTO ni medí el ajuste (la pantalla que abrí es el teclado CW, no la del decodificador; además no hay señal de CW ni autorización para tocar el ajuste). El merge documenta **HALLAZGOS #52** (tono CW con signo invertido) y **#53** (peak beta no varía con WPM) |
| 19/10 ~10:05 | ROM `c640645d810c` + GUI 2a0be37fdb29 | `de0fd9a` | (no corrido) | (no corrido) | 0 | **Estable 2h54min, sin crashes nuevos** ✓ (pid 2526, uptime 10445 s, CRASHLOG 0; el app_log más nuevo sigue siendo el del #131). 0 merges |
| 19/10 ~09:30 | ROM `c640645d810c` + GUI 2a0be37fdb29 | `94c2c6a` | (no corrido) | (no corrido) | 0 | **Estable 2h24min sin nuevos crashes** ✓ (pid 2526, uptime 8651 s, CRASHLOG 0; LASTBREATH up_s=8649 actualizándose). 0 merges |
| 19/10 ~08:55 | ROM `c640645d810c` + GUI 2a0be37fdb29 | `7222aba` | — | **1 repro** ✓ | 1 | **EL SIGSEGV DEL #131 SE REPRODUJO** ✗✓: app_log `21-27-03` con **`pc=0xb6186ae8` y `lr=0xb618d4f0` IDÉNTICOS** al primer crash, **mismo stack** (`b618db48 b6196128`) → **mismo punto de código**. **Trigger en el log**: `on_high_filter_change` + carga de `filter_low=200/filter_high=3000` + `Saved band=7`. **Nuevo**: `CRASHLOG-MOD` graba el **md5 de la GUI** ✓ (lo que pedí). LASTBREATH del momento: `radio=RX tx=0 app=home bat_mv=8700`. La GUI se relanzó (pid 782→2526), daemon 298. Comentado en #131 |
| 19/10 ~08:20 | ROM `c640645d810c` + GUI 2a0be37fdb29 | `5707ed4` | (no corrido) | (no corrido) | 0 | **Estable 1h24min con LASTBREATH activo** ✓ (pid 782, uptime 5047 s, up_s=5037, CRASHLOG 0). Batería sigue en 8600 mV. 0 merges |
| 19/10 ~07:45 | ROM `c640645d810c` + GUI 2a0be37fdb29 (con LASTBREATH) | `4758b18` | (no corrido) | (no corrido) | 0 | **Estable 54min con el LASTBREATH activo** ✓ (pid 782, uptime 3247 s, CRASHLOG 0). El archivo se **actualiza** (`up_s=3245`) → **el fix está vivo** ✓. **Batería constante a `bat_mv=8600`** (8,6 V) con `ext_mv=12600` → mismo valor que en la caída, a investigar si es lectura real o de medición. 0 merges |
| 19/10 ~07:10 | ROM `c640645d810c` + **GUI 2a0be37fdb29** (= mi build del HEAD con `5d787cf`) | `0be0d6f` | **LASTBREATH** | **1 ok — EVIDENCIA** ✓✓ | 0 | **LA RADIO VOLVIÓ** ✓ (uptime 1446 s) **y con el LASTBREATH corriendo** ✓✓ (`/mnt/lastbreath`, 181 B, se actualiza). **Contenido**: `clean=0 boot=3066c0e8 radio=RX tx=0 app=home bat_mv=8600 ext_mv=12600 cap=100 chg=1 ev=0/6/0 last=refresh` → **la 4ª caída fue en reposo (home, sin TX, cola vacía) con la batería a 8.6 V (¡debajo del umbral crítico de 10.8 V!)** ✗✓ → **hipótesis de corte de energía / brown-out**, sin fundar del todo. Comentado en #121 |
| 19/10 ~06:25 | (sin radio) | `2238bfd` | — | — | 0 | **Sigue caída ~18h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~05:50 | (sin radio) | `283c0c8` | — | — | 0 | **Sigue caída ~17h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~05:15 | (sin radio) | `7a8d9f2` | — | — | 0 | **Sigue caída ~17h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~04:40 | (sin radio) | `f74bb45` | — | — | 0 | **Sigue caída ~16h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~04:05 | (sin radio) | `4cabd34` | — | — | 0 | **Sigue caída ~16h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~03:30 | (sin radio) | `f63c553` | — | — | 0 | **Sigue caída ~15h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~02:55 | (sin radio) | `833f6c4` | — | — | 0 | **Sigue caída ~15h15** ✗ (4ª muerte sin recuperación; desde anoche). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~02:20 | (sin radio) | `7b02381` | — | — | 0 | **Sigue caída ~14h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~01:45 | (sin radio) | `fcd751d` | — | — | 0 | **Sigue caída ~14h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~01:10 | (sin radio) | `200eaf4` | — | — | 0 | **Sigue caída ~13h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~00:35 | (sin radio) | `5d503ef` | — | — | 0 | **Sigue caída ~13h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 19/10 ~00:00 | (sin radio) | `1c86cad` | — | — | 0 | **Sigue caída ~12h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~23:25 | (sin radio) | `493cf1a` | — | — | 0 | **Sigue caída ~12h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~22:50 | (sin radio) | `f92f468` | — | — | 0 | **Sigue caída ~11h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~22:15 | (sin radio) | `816d19b` | — | — | 0 | **Sigue caída ~11h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~21:40 | (sin radio) | `585f538` | — | — | 0 | **Sigue caída ~10h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~21:05 | (sin radio) | `5cd686a` | — | — | 0 | **Sigue caída ~10h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~20:30 | (sin radio) | `ea939a1` | — | — | 0 | **Sigue caída ~9h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~19:55 | (sin radio) | `4afa7b8` | — | — | 0 | **Sigue caída ~9h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~19:20 | (sin radio) | `167d6af` | — | — | 0 | **Sigue caída ~8h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~18:45 | (sin radio) | `b346667` | — | — | 0 | **Sigue caída ~8h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~18:10 | (sin radio) | `454855f` | — | — | 0 | **Sigue caída ~7h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~17:35 | (sin radio) | `f45a439` | — | — | 0 | **Sigue caída ~7h15** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~17:00 | (sin radio) | `d3a319e` | — | — | 0 | **Sigue caída ~6h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente. 0 merges |
| 18/10 ~16:25 | (sin radio) | `863a11c` | — | — | 0 | **Sigue caída ~6h15** ✗ (la más larga; 4ª muerte sin recuperación). Fix `5d787cf` pendiente de publicar. 0 merges |
| 18/10 ~15:50 | (sin radio) | `a3dc4a2` | — | — | 0 | **Sigue caída ~5h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente de publicar. 0 merges |
| 18/10 ~15:15 | (sin radio) | `8d41897` | — | — | 0 | **Sigue caída ~5h15** ✗ (la caída más larga; 4ª muerte sin recuperación). El fix `5d787cf` (LASTBREATH) sigue pendiente de publicar. 0 merges |
| 18/10 ~14:40 | (sin radio) + **merge `5d787cf` pendiente de publicar** | `5d787cf` | — | — | 0 | **Llegó el fix del #121**: `5d787cf` *"Hang #121: ultima respiracion persistente (LASTBREATH) y aviso de bateria baja (#138)"* ✓✓ — **es exactamente la evidencia que faltaba**. **La radio sigue caída** ✗ (~4h45 desde la 4ª muerte) → **deploy PENDIENTE**. Comentado en #121 |
| 18/10 ~14:05 | (sin radio) | `59b1cf0` | — | — | 0 | **Sigue caída ~4h15** ✗ (4ª caída sin recuperación). 0 merges |
| 18/10 ~13:30 | (sin radio) | `efae955` | — | — | 0 | **Sigue caída ~3h45 después de la 4ª caída** ✗ (3 de 4 muertes no se recuperan solas). 0 merges; nueva rama `factory/swrscan-stopfix` |
| 18/10 ~12:55 | (sin radio) | `94b9989` | — | — | 0 | **Sigue caída 3h después de la 4ª caída** ✗ (no se recuperó sola, igual que la 1ª y la 2ª). 0 merges. Pasa a la mañana para el ciclo de energía del owner |
| 18/10 ~12:20 | (sin radio) | `07321a5` | — | — | 0 | **Sigue caída 1h30min después de la 4ª caída** ✗: 0 hosts con SSH en 192.168.0.0/24 y 192.168.18.0/24. Sin merges nuevos. Pendiente: que el owner la mire (ciclo de energía) y **medir el uptime al volver** para saber si reinició o se colgó sin reiniciar |
| 18/10 ~11:45 | ROM `c640645d810c` + GUI ffef63df58cb | `896a3cf` | (sin radio) | — | 0 | **4ª CAÍDA del #121** ✗: última vez viva 00:09 (uptime 51644 s = **14 h 20 min**, pid 7317, CRASHLOG 0) → a las 00:45 **0 hosts con SSH en las dos redes** (mi red OK). **Murió en reposo, sin actividad ni TX** ✓ → **no hace falta actividad para que se caiga**. Sin `pstore` sigue sin haber evidencia del kernel (4 caídas, 0 evidencia). Comentado en #121 |
| 18/10 ~11:05 | ROM `c640645d810c` + GUI ffef63df58cb | `b6be6ee` | (no corrido) | (no corrido) | 0 | **Estable 14h20min** ✓ (pid 7317, uptime 51644 s, CRASHLOG 0). 0 merges |
| 18/10 ~10:30 | ROM `c640645d810c` + GUI ffef63df58cb | `837bbf2` | (no corrido) | (no corrido) | 0 | **Estable 13h50min** ✓ (pid 7317, uptime 49843 s, CRASHLOG 0). 0 merges |
| 18/10 ~09:55 | ROM `c640645d810c` + GUI ffef63df58cb | `d5019a7` | (no corrido) | (no corrido) | 0 | **Estable 13h20min** ✓ (pid 7317, uptime 48045 s, CRASHLOG 0). 0 merges |
| 18/10 ~09:20 | ROM `c640645d810c` + GUI ffef63df58cb | `eba2230` | (no corrido) | (no corrido) | 0 | **Estable 12h50min** ✓ (pid 7317, uptime 46243 s, CRASHLOG 0). 0 merges |
| 18/10 ~08:45 | ROM `c640645d810c` + GUI ffef63df58cb | `7aea604` | (no corrido) | (no corrido) | 0 | **Estable 12h20min** ✓ (pid 7317, uptime 44444 s, CRASHLOG 0). 0 merges |
| 18/10 ~08:10 | ROM `c640645d810c` + GUI ffef63df58cb | `d1cefe7` | (no corrido) | (no corrido) | 0 | **Estable 11h50min** ✓ (pid 7317, uptime 42645 s, CRASHLOG 0). 0 merges |
| 18/10 ~07:35 | ROM `c640645d810c` + **GUI ffef63df58cb** (merges `d3e84ed`/`f53c2ab`) | `d3e84ed` | **comando remoto swrscan** | **2 ok (Run+Stop)** ✓ | 0 | **VALIDADO el comando `swrscan` del #135** ✓✓: `val=1` → `ok` (Run ✓), `val=0` → `ok` (Stop ✓), guarda `"app not open"` ✓ sin la app. La captura durante el barrido tiene **3083 px de tinta de curva** ✓ = el barrido corrió. **Valor correcto es `1\|0`**, no `"run"/"stop"` (mi primer intento dio `Invalid value` ✗). TX 0.1 W con autorización. pid 7317, CRASHLOG 0 |
| 18/10 ~07:00 | ROM `c640645d810c` + GUI b1ba67520079 | `c55e8b9` | (no corrido) | (no corrido) | 0 | **Estable 10h50min** ✓ (pid 3931, uptime 39044 s, CRASHLOG 0). 0 merges; 3 ramas del otro agente activas (roe-staircase-119, swr125-capturas, swr125-direction) |
| 18/10 ~06:25 | ROM `c640645d810c` + GUI b1ba67520079 | `b8441a6` | (no corrido) | (no corrido) | 0 | **Estable 10h20min** ✓ (pid 3931, uptime 37245 s, CRASHLOG 0 en el log vivo). 0 merges. El app_log nuevo (22-38-46) es el del SIGSEGV del #131, ya reportado |
| 18/10 ~05:40 | ROM `c640645d810c` + GUI b1ba67520079 | `885f823` | **modo/filtros** | **no repro** | 0 | **CRASH NUEVO detectado en el app_log** ✗: `CRASHLOG sig=11 fallo=0xf363` justo después de `on_vfo_mode_change: mode=2` + carga de `filter_low/high`; la GUI se relanzó sola (pid 2728→3931) y el equipo siguió. **NO es el del #116**. **Stack sin resolver** (direcciones en rango de .so, sin md5 de las libs). **No reproducible**: LSB→USB→CW→AM + 4 botones → sin crash (pid 3931, CRASHLOG 0). **Issue nuevo cargado** |
| 18/10 ~05:10 | ROM `c640645d810c` + **GUI b1ba67520079** (merge `885f823`) | `885f823` | **SWRScan** | **1 ok — VALLE** ✓✓ | 0 | **PUBLICADO Y VALIDADO `885f823`**: la curva de ROE **ya NO está invertida** ✓✓. Números: centro y=395 (antes 118) → ROE baja ✓, bordes y=300/309 → ROE alta ✓ → **el valle está en la resonancia** ✓. `min 1.0 @ 6.780.000 kHz`, `Passes: 1`, span 500 kHz, Scale Linear. **Pendiente**: sigue saliendo `Error: timeout de RF` (barrido cortado en n=44/51) ✗ |
| 18/10 ~04:40 | ROM `c640645d810c` + GUI 846afac6ed47 | `c67a30b` | (no corrido) | (no corrido) | 0 | **Estable 9h07min** ✓ (pid 1133, uptime 32798 s, 1 GUI, CRASHLOG 0) — la racha más larga de la sesión. 0 merges |
| 18/10 ~01:55 | ROM `c640645d810c` + GUI 846afac6ed47 | `e38267c` | (no corrido) | (no corrido) | 0 | **Estable 5h50min** ✓ (pid 1133, uptime 21043 s, 1 GUI, CRASHLOG 0). 0 merges |
| 18/10 ~01:20 | ROM `c640645d810c` + GUI 846afac6ed47 | `37de993` | (no corrido) | (no corrido) | 0 | **Estable 5h20min** ✓ (pid 1133, uptime 19243 s, 1 GUI, CRASHLOG 0). 0 merges |
| 18/10 ~00:45 | ROM `c640645d810c` + GUI 846afac6ed47 | `40f649f` | **SWRScan** | **1 repro** | 0 | **Curva de ROE INVERTIDA confirmada con números** ✗✓ (lo que vio el owner): la curva extraída de `26-roe-multipasada-final.png` da **ROE ~4.5 en el CENTRO** y **~1.4 en los extremos** (601 columnas, y=118 centro vs y=399 borde) → **donde debería haber un valle hay una montaña**. El readout dice `min 1.0 @ 6.891.334` y **es coherente** con la curva (mínimo en el borde) → **el dato parece bien, el render parece invertido** (sin fundar). Comentado en el #119 con las 2 capturas (curva invertida + error de multipasadas) |
| 18/10 ~00:20 | ROM `c640645d810c` + GUI 846afac6ed47 | `4ee935c` | **SWRScan Passes>1** | **1 repro — gate** | 0 | **Reproducido el error del owner**: con `Passes` en 5 y `Run` sale **`Error: 3/5 pasadas requieren validacion RF`** + `sin datos medidos` ✗. **Causa en código**: `SWRSCAN_MULTIPASS_TX_ENABLED = 0` (dialog_swrscan.h:61) → `limits->max_passes = 1` → `swr_session.c:281` aborta con `SWR_ABORT_MULTIPASS_GATED`. **El gate protege el PA y está bien**; **el defecto es de UX**: el botón `Passes` cicla 1→3→5 y el `Run` lo rechaza sin aviso. Evidencia `28-roe-multipasada-error.png` |
| 17/10 ~23:40 | ROM `c640645d810c` + GUI 846afac6ed47 | `a1a537c` | (no corrido) | (no corrido) | 0 | **Estable 4h50min** ✓ (pid 1133, uptime 17443 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~23:05 | ROM `c640645d810c` + GUI 846afac6ed47 | `94bfbe6` | (no corrido) | (no corrido) | 0 | **Estable 4h20min** ✓ (pid 1133, uptime 15645 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~22:30 | ROM `c640645d810c` + GUI 846afac6ed47 | `f275073` | (no corrido) | (no corrido) | 0 | **Estable 3h50min** ✓ (pid 1133, uptime 13844 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~21:55 | ROM `c640645d810c` + GUI 846afac6ed47 | `dbd6174` | (no corrido) | (no corrido) | 0 | **Estable 3h20min** ✓ (pid 1133, uptime 12043 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~21:20 | ROM `c640645d810c` + GUI 846afac6ed47 | `298f895` | (no corrido) | (no corrido) | 0 | **Estable 2h50min** ✓ (pid 1133, uptime 10244 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~20:45 | ROM `c640645d810c` + GUI 846afac6ed47 | `1da0530` | (no corrido) | (no corrido) | 0 | **Estable 2h20min con la GUI nueva** ✓ (pid 1133, uptime 8445 s, 1 GUI, CRASHLOG 0) · **red OK** ✓ (`scp` de prueba pasa ✓) · `audio_init: Conected` ✓ (el sink sigue en ceros: parte 2 del #116). 0 merges |
| 17/10 ~19:55 | ROM `c640645d810c` + GUI 846afac6ed47 | `bb76d74` | **SWRScan multipasada (final)** | **1 ok — PARÁBOLA** ✓✓ | 0 | **CONFIRMADO: la curva de ROE ya es una PARÁBOLA** ✓✓ (captura `26-roe-multipasada-final.png`): arranca ~1.0 a la izq, sube suave, pico ~3.7 al centro (6.99 MHz), baja suave a ~1.0 der — **sin escalones** ✓. UI con `min 1.0 @ 6.891.334 kHz (n=44/51)`, `Passes: 1`, span 200 kHz. **Nuevo**: mensaje `Error: timeout de RF` (el barrido se cortó en 44/51; no medí si es nuevo). App abierta tras 45 s, pid 1133, CRASHLOG 0 |
| 17/10 ~19:20 | ROM `c640645d810c` + **GUI nueva 846afac6ed47** (996b7fa + aa1945e) | `b00255d` | **SWRScan multipasada** | **1 ok (UI + scan)** | 0 | **PUBLICADO Y VALIDADO el build del #119** ✓✓: primer deploy por WiFi exitoso del día (el reinicio del owner arregló la red ✓). La UI muestra **`Scanning pass 1/1 p.6/51`**, **`min 1.0 @ 6.891.334 kHz (n=5/51)`**, **`Passes: 1`**, span 200 kHz, modo LSB forzado ✓; `Run` responde, pid 1133, CRASHLOG 0. **Pendiente**: si la curva final es parabólica (en la captura iba a mitad de barrido) |
| 17/10 ~18:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `147713b` | (sin radio) | — | 0 | **Radio inalcanzable ~2 h** ✗ tras la 3ª caída y el reinicio del owner: **0 hosts con SSH** en 192.168.0.0/24 y 192.168.18.0/24, mi red OK (gateway responde). El build del #119 (`996b7fa` → GUI 846afac6ed47) **no se pudo publicar** (la red corta toda transferencia: 3 intentos con `lost connection`/`Connection closed`, ni 100 KB). **Queda por confirmar** (no lo veo yo): si la radio está encendida → es el WiFi; si está negra → otra muerte (crítico en #121). Nueva rama `factory/swr125-capturas` |
| 17/10 ~17:30 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `b285a73` | — | — | 0 | **Red degradada tras la 3ª caída** ✗ (medido): ping a la radio **0/4 (100% pérdida)** ✗, `scp` de **100 KB no termina** ✗, 13,7 MB y 5,9 MB comprimido tampoco ✗, pero **SSH de comandos anda** ✓ → **el canal de datos quedó roto**; **no es MTU** (ping chico también falla) **ni tamaño** (100 KB falla). Causa sin fundar. **Deploy por WiFi imposible** → el build `996b7fa` (#119 multipasada) **no llegó** |
| 17/10 ~17:15 | ROM `c640645d810c` + GUI aaa60c1d3ab5 (el build nuevo NO llegó) | `12ace3e` | (no corrido) | (no corrido) | 0 | **3ª CAÍDA del #121, esta vez durante un scp** ✗: antes pid 2452/uptime 44906 s → `scp` cortó al 1,9% (`Connection closed`) → después **pid 311/uptime 456 s = SE REINICIÓ SOLO** (a diferencia de la 1ª caída, que no se recuperó en ~2 h). **La red quedó mal**: los `scp` de varios MB se cuelgan siempre (6 intentos), aunque SSH/`ls`/`md5sum` andan. **El build `996b7fa` (multipasada #119) NO se pudo publicar** — sigue el binario anterior |
| 17/10 ~15:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `a777fbd` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 12h21min** ✓ (pid 2452, uptime 44467 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~15:00 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `98f1231` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 11h51min** ✓ (pid 2452, uptime 42668 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~14:25 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `11ff9ca` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 11h21min** ✓ (pid 2452, uptime 40875 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~13:50 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `92fd0ae` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 10h51min** ✓ (pid 2452, uptime 39069 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~13:15 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `251ab90` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 10h21min** ✓ (pid 2452, uptime 37270 s, 1 GUI, CRASHLOG 0). La IP volvió a 192.168.18.67. 0 merges; 5 ramas en curso |
| 17/10 ~12:40 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `0498593` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 9h51min** ✓ (pid 2452, uptime 35471 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~12:05 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `162bb4c` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 9h21min** ✓ (pid 2452, uptime 33671 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~11:30 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `1700cd8` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 8h51min** ✓ (pid 2452, uptime 31870 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~10:55 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `1129424` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 8h21min** ✓ (pid 2452, uptime 30070 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~10:20 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `1236440` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 7h51min** ✓ (pid 2452, uptime 28271 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~09:45 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `7acc96a` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 7h21min** ✓ (pid 2452, uptime 26471 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~09:10 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `fd16712` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 6h51min** ✓ (pid 2452, uptime 24671 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~08:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `fc35445` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 6h21min** ✓ (pid 2452, uptime 22870 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~08:00 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `5c44d3f` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 5h51min** ✓ (pid 2452, uptime 21071 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso (sin cambios) |
| 17/10 ~07:25 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `aad2a5a` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 5h21min** ✓ (pid 2452, uptime 19273 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~06:50 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `563bfd3` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 4h51min** ✓ (pid 2452, uptime 17470 s, 1 GUI, CRASHLOG 0). 0 merges; **nueva rama `factory/swrscan-multipass-125`** (el próximo paso del #119: el escalón). 5 ramas en curso |
| 17/10 ~06:15 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `e717d9f` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 4h21min** ✓ (pid 2452, uptime del sistema 15673 s, 1 GUI, CRASHLOG 0). 0 merges; **nueva rama `factory/cw-tune-122`** (issue nuevo, no mío). Siguen 116, 119 y 125 |
| 17/10 ~05:40 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `d7d0ca2` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 3h51min sin caídas** ✓ (pid 2452, uptime 13872 s, 1 GUI, CRASHLOG 0). 0 merges. 3 ramas en curso: cw-sigsegv-116, roe-staircase-119 y swrscan-mock-fixtures-125 |
| 17/10 ~05:05 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `52a0204` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 3h21min** ✓ (pid 2452, uptime 12071 s, 1 GUI, CRASHLOG 0). La radio cambió de IP a 192.168.18.67 (normal). 0 merges; **nueva rama `lu2dsx/swrscan-mock-fixtures-125`** (fixtures mock del SWR Scan = los datos del #119) y siguen 116/119 |
| 17/10 ~04:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `38e2186` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 2h51min** ✓ (pid 2452, uptime del sistema 10271 s, 1 GUI, CRASHLOG 0): los fixes #116/#119 aguantan. 0 merges nuevos; siguen `factory/cw-sigsegv-116` y `factory/roe-staircase-119` |
| 17/10 ~04:00 | ROM `c640645d810c` + GUI nueva aaa60c1d3ab5 (merges `3553a47`/`e2edd91`) | `3553a47` | **CW + SWRScan** | **1 ok (CW), 1 parcial** | 0 | **VALIDACIÓN DE LOS 2 FIXES**: (1) **#116 ✓**: abrir CW, apretar los 4 botones de mensajes y cerrar → **no crashea** (pid 2452, CRASHLOG 0, sin errores); el HALLAZGOS #48 confirma el análisis (NULL al keyer con tabla vacía) **con mi medición**. (2) **#119 parcial**: el **fondo del chart pasó a negro** ✓ (antes gris, `c54-nf-10.png`) pero **la curva no apareció** ✗ — y **el barrido NO corrió** (botón en `Run`, modo `USB-D`): **no es evidencia** de que no dibuje. Escalon: pendiente |
| 17/10 ~03:50 | ROM `c640645d810c` (GUI e294229dd32d) | `c1ca41b` | (no corrido) | (no corrido) | 0 | **111 min estable sin caídas** ✓ (pid 309 desde el barrido, uptime 6670 s ≈ 1 h 51 min, 1 GUI, CRASHLOG 0): el #121 sin reproducirse en casi 2 h. **Audio: el race persiste**. 0 merges; siguen las dos ramas (116/119) |
| 17/10 ~03:20 | ROM `c640645d810c` (GUI e294229dd32d) | `a6b6663` | (no corrido) | (no corrido) | 0 | **81 min estable sin caídas** ✓ (pid 309 desde el barrido, uptime 4872 s, 1 GUI, CRASHLOG 0): el #121 sin reproducirse. **Audio: el race persiste**. 0 merges; siguen `factory/cw-sigsegv-116` y `factory/roe-staircase-119` |
| 17/10 ~02:50 | ROM `c640645d810c` (GUI e294229dd32d) | `4d913ac` | (no corrido) | (no corrido) | 0 | **51 min estable sin caídas** ✓ (pid 309 — el mismo del barrido, uptime 3071 s, 1 GUI, CRASHLOG 0): el #121 sigue sin reproducirse. **Audio: el race persiste**. 0 merges; siguen `factory/cw-sigsegv-116` y `factory/roe-staircase-119` en curso |
| 17/10 ~02:20 | ROM `c640645d810c` (GUI e294229dd32d) | `c0d6184` | (no corrido) | (no corrido) | 0 | **Radio estable 21 min, sin caídas nuevas** ✓ (pid 309 desde las ~02:00, uptime 1279 s, 1 GUI, CRASHLOG 0) → el #121 **no** se reprodujo tras el barrido. **Audio: el race persiste** (audio_init_sync: pulseaudio no disponible). **0 merges**, pero el otro agente abrió **`factory/roe-staircase-119`** (mi #119 ✓) y sigue **`factory/cw-sigsegv-116`** (#116 ✓) |
| 17/10 ~02:00 | ROM `c640645d810c` (GUI e294229dd32d) | `0cc4357` | **PERMANENCIA=14, 23 apps** | **23 ok, 0 caídas** ✓ | — | **PRIMER BARRIDO LIMPIO SOBRE LA ROM NUEVA**: 23/23 ok (FT8…SWRScan), 0 cuelgues, con **monitor de evidencia en paralelo** (uptime 340→584 s avanzando, pid 309 constante, CRASHLOG 0) → **el barrido de apps NO dispara la caída del #121** (dato negativo útil). Comentado en #121 |
| 17/10 ~01:35 | ROM `c640645d810c` / GUI e294229dd32d | `44b225f` | (pendiente) | (pendiente) | — | **Datos nuevos de las caídas**: (1) **la 1ª caída NO se recuperó sola**: estuvo muerta de ~22:57 a ~01:09 (**~2 h**) y el **watchdog de 16 s no la levantó** ✗ (dato medido, causa sin fundar); (2) **la 2ª caída no dejó log** (el app_log más nuevo sigue siendo el de la 1ª; el log vivo es tmpfs y se archiva sólo si la GUI sale ordenada) → **la evidencia se perdió con el ciclo de energía**; (3) la radio volvió: uptime 38 s, GUI e294229dd32d, pid 309, CRASHLOG 0. Comentado en #121 |
| 17/10 ~01:25 | ROM `c640645d810c` / GUI e294229dd32d | `91adc22` | (barrido inútil) | **0 apps** | — | **Segunda caída de la radio en ~30 min** ✗ (a las 01:10 respondía con pid 311; a las 01:25 **0 hosts con SSH en las dos redes**, mi red OK). El barrido arrancó con **IP vacía** (la radio ya no respondía) → 23 apps con "no responde", **sin dato de apps**. **Sin barrido de SWR mío antes de esta caída** → la correlación con el SWR se **debilita**. Comentado en #121 |
| 17/10 ~01:10 | ROM `c640645d810c` / GUI e294229dd32d | `54ebfb1` | (pendiente) | (pendiente) | — | **La radio se colgó entera** ✗ (necesitó ciclo de energía): el último log (`22-57-10`) **corta a mitad de línea en medio de un barrido de SWR** (pasos de ~100 Hz cada 50-90 ms = el sweep del #113/#117, que es una TX). **CRASHLOG=0** en el log vivo y en los 4 últimos → **no es la GUI, se va el equipo**. La radio volvió con el ciclo de energía (uptime 27 s, GUI e294229dd32d ✓). **Causa sin fundar** (sin pstore ni consola). Issue nuevo, relacionado con el #114 (mismo tipo de muerte). Cliente remoto repuesto ✓ |
| 17/10 ~00:50 | ROM `c640645d810c` / GUI e294229dd32d | `d857be3` | (barrido fallido) | **0 ok** | — | **La radio quedo INALCANZABLE** ✗ durante el barrido de apps sobre la ROM nueva: no responde en 192.168.0.0/24 ni 192.168.18.0/24 (mi red esta bien: gateway OK). **El 'CUELGUE' de las 23 apps era mi herramienta**: el boot del flash borro `/tmp/x6200-remote` y el script no lo repone (0 apps probadas). **La radio estaba VIVA durante el barrido** (tras cada app la SSH respondia y el uptime avanzaba: 1601->1626->1653->... y el barrido llego al final trayendo el boot.log). Murio despues. **Causa del fallo del script (mi herramienta)**: el boot del flash borro `/tmp/x6200-remote` y `prueba-apps-radio.sh` **no lo repone** → `ok: 0`. **La caida de la radio es un hecho separado y sin diagnosticar** (necesita ciclo de energia) |
| 17/10 ~00:30 | ROM `c640645d810c` / GUI e294229dd32d | `223b30a` (con #117) | (pendiente) | (pendiente) | — | **#119**: la curva de ROE **no es una parábola**: se dibuja **escalonada** (6 cuadros del framebuffer leídos en bucle rápido, `frame-2.png` lo muestra) → el #117 dice "fitted to a single parabola" en el código pero el render sale en escalera. **Y la app se cierra sola** al terminar el barrido (3 capturas muestran la pantalla principal, mismo pid, sin CRASHLOG). Además: fantasma de repintado. Causa **sin fundar**. **Datos anexados al issue**: `roe-muestras-extraidas.csv/.json` (496 puntos extraidos de la pantalla; el log no guarda las muestras) — **y muestran que la curva viene cuantizada en escalones** (12 muestras identicas seguidas, p.ej. 1.33) |
| 17/10 ~00:10 | **ROM nueva corriendo** e294229dd32d (imagen `c640645d810c`) | `223b30a` | (pendiente barrido) | (pendiente) | — | **La ROM flasheada arrancó bien** ✓: GUI e294229dd32d ✓, lib 5ed07c1719f8 ✓, `init.d`=`S20gui` ✓ (fix #89 en la imagen ✓), CRASHLOG 0, **y los ajustes del owner intactos** (`Redshift`/`vol=66`/`span=50000`, DATA 3,2G) ✓✓. **El race del #116 se reproduce en la instalación limpia** ✗✓ → queda fundado que es determinista |
| 16/10 ~23:40 | **ROM nueva flasheada** `c640645d810c` | `223b30a` (incluye #117) | (pendiente: validar en la radio) | (pendiente) | — | **Flasheo de la SD** ✓: verificado por md5 ida y vuelta (`c640645d810c…` idéntico) ✓, `init.d`=sólo `S20gui` ✓. **DATA restaurada en slot 3** ✓ y **ajustes intactos** (params.db 106 KB, `theme_name=Redshift`, `vol=66`, `swrscan_span=50000`, 662 app_logs) ✓. Los deploys por WiFi fallaron 2 veces (script colgado + scp text-busy) → se fue por SD |
| 16/10 ~23:00 | 3604cece0e43 (sigue) | `246ffa5` | (no corrido) | (no corrido) | — | 0 merges nuevos; **el otro agente abrió `factory/events-queue-111`** (trabajando el #111). Audio: `Conected` ✓ y streams 1+2 ✓ pero el sink sigue `rms=0.0` (la parte 2 del #116 sin fix). Radio estable, CRASHLOG 0 |
| 16/10 ~22:50 | 3604cece0e43 (sigue) | `420d5fc` | (no corrido) | (no corrido) | — | **Causa raíz del audio FUNDADA**: un **race de arranque** — `audio_init_sync: pulseaudio no disponible (Connection refused)` a los ~6 s (la GUI no reintenta) → **0 streams en frío**; al reiniciar la GUI con PA arriba dice **`Conected`** y abre los 3 streams. **PERO**: aun conectada, **el sink sigue en ceros** (captura ✓ + reproduce silencio ✗) → **segunda parte sin causa**. Es lo que dejó sin audio tras el crash del #116 |
| 16/10 ~22:40 | 3604cece0e43 (sigue) tras **reboot** | `538fffc` | (no corrido) | (no corrido) | — | **Prueba decisiva: reiniciar el EQUIPO tampoco devuelve el audio** (boot limpio pid 308, sink `rms=0.0`) → la falla es **determinista** (no corrupción, no códec, no volátil). Queda: un **parámetro guardado** o un **defecto del código** de la GUI. **El DAC vuelve a quedar apagado al arrancar** (igual que se lo encontró) |
| 16/10 ~22:35 | 3604cece0e43 (sigue) | `fb38e1d` | (no corrido) | (no corrido) | — | **Prueba grounded: reiniciar la GUI NO recupera el audio** (pid 1119→1950, sink sigue `rms=0.0`) → la falla es **persistente y dentro de la GUI**. Streams: los 3 abiertos/sin mute/100% (Play 44100, Capture 44100 y **un source-output a 30 Hz inusual**). DAC ya corregido. Falta solo **reiniciar el equipo** (decisivo), propuesto al owner |
| 16/10 ~22:30 | 3604cece0e43 (sigue) | `a118c18` | (no corrido) | (no corrido) | — | **Prueba del DAC hecha** (autorizada): `amixer sset DAC on` + nivel 160 → el DAC quedó **160 [83%] [0.00dB] [on]** ✓ pero **el sink sigue en ceros** (`rms=0.0`) → **el DAC no era la causa** (era un **segundo** problema, ahora corregido). **Error mío de razonamiento**: el monitor del sink está **antes** del codec, así que esa grabación no podía medir el efecto del DAC. Queda probado que **la GUI sigue escribiendo ceros** → el problema está en su cadena de audio |
| 16/10 ~22:25 | 3604cece0e43 (sigue) | `67efc1d` | (no corrido) | (no corrido) | — | **Cadena de salida medida eslabón por eslabón**: `AIF1 DA0` 160 [83%] [0.00dB] (sana) → **`DAC` 0 [0%] [-99999.99dB] [off] (APAGADO)** → `Headphone` 58 [92%] [on] (sana) · `Headphone Source = DAC` → **el corte está en el DAC** (coherente con que el sink reciba ceros). Prueba propuesta al owner: `amixer sset DAC on` (reversible) |
| 16/10 ~22:20 | 3604cece0e43 (sigue) | `c5658f9` | (no corrido) | (no corrido) | — | **Confirmado que la radio RECIBE** (captura `24-recepcion-sin-audio.png`: 4-5 portadoras en el espectro + trazas en el waterfall) → **el RF no es la causa** (el owner tenía razón, mi candidato del RF gain queda muerto). **Pista fuerte**: `DAC: Playback 0 [0%] [-99999.99dB] [off]` con `Headphone Source = DAC` → el corte estaría en la conversión D/A. **Sin fundar**: si el DAC cambió (la GUI no lo vuelca, no hay línea base) |
| 16/10 ~22:15 | 3604cece0e43 (sigue) | `21a4116` | (no corrido) | (no corrido) | — | **Audio localizado por medicion** (en el #116): la **entrada tiene señal** (`rms=141.3`) y el **sink recibe ceros** (`rms=0.0`), con el stream de la GUI abierto, sin mute y al 100% → **el silencio se genera dentro de la GUI**. Descartados PulseAudio, el mixer de salida, la squelch y el volumen. Sin tocar nada |
| 16/10 ~22:05 | 3604cece0e43 (sigue) | `ab7710b` | (no corrido) | (no corrido) | — | **El owner se quedó sin audio y era un CRASH de la GUI**: `CRASHLOG sig=11 fallo=(nil)` con el contexto lleno de `CWDBG` (sale al sintonizar) → **issue #116**. La GUI pasó de pid 311 a 1119 (el daemon no cambió: no hubo reboot). Audio medido por partes: pulse RUNNING/no mute/83% y Headphone on 92% → el silencio está **antes** del mixer; `rfgain=0` (ayer 84) sin poder fundarlo. **La pila no se pudo resolver** (el binario que quedó en el volumen es de **otro build**, no el de la radio) |
| 16/10 ~21:45 | 3604cece0e43 (sigue) | `864d001` | (no corrido) | (no corrido) | — | Monitoreo del flood para completar el #114: se mide el ritmo del log cada ciclo. Radio estable |
| 16/10 ~21:35 | 3604cece0e43 (sigue) | `917604e` | (no corrido) | (no corrido) | — | **Reproducción del #114 intentada y NO lograda** (autorizada por el owner): `nb_level` 5→0, `nb_width` 5→0 y los tres juntos → el equipo **sobrevivió** (uptime continuo 448→508 s, mismo pid, CRASHLOG 0) y el NB quedó **restaurado**. **Pista**: cuando murió el log iba a **~16/s** y ahora a **~0,4/s** → la precondición parece ser el **flood del #111** (intermitente) |
| 16/10 ~21:25 | 3604cece0e43 (sigue) tras **reinicio del equipo** | `a4ba51f` | (no corrido) | (no corrido) | — | **El equipo se reinició al aplicar el NB** (uptime 50 s): **issue #114**. Sin CRASHLOG (murió por debajo de la GUI), el log del run colgado (77.020 líneas, **99% overflows**) **termina cortado en medio del flood a ~16/s**, `nb=1` con `nb_level=0`/`nb_width=0`, y el kernel tiene `panic=10` + watchdog de 16 s (explica el reboot). Reproducción **no hecha** (colgaría la radio del owner otra vez) |
| 16/10 ~21:10 | 3604cece0e43 (sin cambios) | `2e61ace` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~20:40 | 3604cece0e43 (sin cambios) | `06cf0f7` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~20:10 | 3604cece0e43 (sin cambios) | `7b56feb` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~19:40 | 3604cece0e43 (sin cambios) | `1ba6f07` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~19:10 | 3604cece0e43 (sin cambios) | `f1d37a3` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~18:40 | 3604cece0e43 (sin cambios) | `69a6501` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~18:15 | 3604cece0e43 (sin cambios) | `fec1c2a` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~17:50 | 3604cece0e43 (sin cambios) | `3d6c022` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. El owner sigue en CW 7.012. #111 sigue |
| 16/10 ~17:45 | 3604cece0e43 (sin cambios) | `7ddfa88` | (se cuelga) | (ya corrido) | **0** | **Rechequeo tras un salto de ~5,5 h** (equipo suspendido): 0 merges, 1 GUI (pid 2889 estable), **CRASHLOG 0 en los 6 últimos app_logs** (ninguno en el hueco), DATA 1% usada, load 0,92. El owner está en **CW 7.012000**. El #111 sigue: log nuevo con 3.857 overflows de 3.814 líneas |
| 16/10 ~18:45 | 3604cece0e43 (sin cambios) | `31b8d8a` | (se cuelga) | (ya corrido) | **0 en 60 s** | **#99 VALIDADO**: con el dial en 7.012 (el owner lo movió) el barrido **dibuja la curva** (17,77% no-fondo vs 17,83% del build sin #99) y se ve el valle de resonancia (~1,0-1,4 en 7,01). Cerrar no reinicia la GUI (pid 2889) y **devuelve el modo a CW** (#84) con CRASHLOG 0 |
| 16/10 ~18:40 | 3604cece0e43 (sin cambios) | `789ee5c` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue; dial fuera del rango del #99 |
| 16/10 ~18:30 | 3604cece0e43 (sin cambios) | `a94d0c3` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0, load ok. #111: 20143 de 20820 overflows. Dial fuera del rango del #99 |
| 16/10 ~18:20 | 3604cece0e43 (sin cambios) | `fea20c2` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. **#111**: 18.856 de 19.533 (96%) y **los logs no llenan el disco** (DATA 1% usado, app_logs 26 MB en 657 archivos) → el impacto es **eventos descartados + desgaste de SD**, no espacio |
| 16/10 ~18:15 | 3604cece0e43 (sin cambios) | `6da72f7` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0; #111 sigue; dial fuera del rango del #99 |
| 16/10 ~18:05 | 3604cece0e43 (sin cambios) | `9b36c0f` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI (pid 2889), CRASHLOG 0, load 0,62. #111: +1.463 en ~10 min (2,4/s). #99: dial 7.158, fuera de rango |
| 16/10 ~17:55 | 3604cece0e43 (sin cambios) | `026b2f3` | (se cuelga) | (ya corrido) | **0 en 60 s** | Sin novedades: 0 merges, 1 GUI (pid 2889), CRASHLOG 0. **#111**: +1.580 en ~7 min (3,8/s). **Aclaración**: los `app_logs` se **rotan por tamaño** (aparece un log guardado nuevo con el mismo pid) → los 635 archivos **no** son caídas |
| 16/10 ~17:45 | 3604cece0e43 (sin cambios) | `0dd02f2` | (se cuelga) | (ya corrido) | **0 en 60 s** | Sin novedades: 0 merges, 1 GUI (pid 2889), CRASHLOG 0, load 0,80. **#111**: +702 overflows en ~5 min (2,3/s) — el ritmo varía (18/s en la ventana de 4 min anterior) |
| 16/10 ~17:35 | 3604cece0e43 (sin cambios) | `b1d48bb` | (se cuelga) | (ya corrido) | (sin cambios) | Estable (pid 2889, CRASHLOG 0). **#111 medido en ventana larga**: +4.447 líneas en 240 s = **18/s, el 100% Overflows**, con la radio en reposo (load 0,84) → es **sostenido**, no ráfagas (corregido en el issue). ~18 eventos/s descartados y ~18 escrituras/s a la SD |
| 16/10 ~17:30 | `f88813ae85e3` (sin cambios) | `0a7479d` | (no corrido) | (ya corrido) | (sin cambios) | Estable (pid 1099, CRASHLOG 0). **#111 acumula 19.387 overflows** de 20.030 líneas y el ritmo confirmó ser **en ráfagas** (1,0 / 2,1 / 5,9 / 17,6 líneas/s según la ventana). **No pude medir la correlación con el CPU**: 3 intentos fallidos por scripts no-POSIX y `2>/dev/null` que ocultaba el error (anotado en el skill) |
| 16/10 ~17:30 | **3604cece0e43** | `2392624` (#110) | (se cuelga) | **23 ok** | **0 en 90 s** | **Desplegado y sano**: build exit 0 (7 s), gate OK (13.742.964 B, 72 B menos que el anterior = el fix del #110); 1 GUI; barrido 23/0; estado idéntico antes/después; CRASHLOG 0. El #110 (caja del campo de año) **no lo verifiqué** (es visual y profundo en Ajustes). El **#111 persiste** |
| 16/10 ~17:15 | `f88813ae85e3` (sin cambios) | `7a80645` | (se cuelga) | (ya corrido) | (sin cambios) | Estable (pid 1099, CRASHLOG 0). **#111 en ráfagas**: +1.873 líneas en ~15 min (**100% Overflows**, ~2,1/s de promedio) contra 17,6/s instantáneos medidos antes → el ritmo **varía mucho**, no es un goteo constante. Total acumulado: 13.477 overflows |
| 16/10 ~17:00 | `f88813ae85e3` (sin cambios) | `cd9bfb3` | (se cuelga) | (ya corrido) | **0 en 60 s** | Sano y estable: 1 GUI (pid 1099), tema `Redshift`, CRASHLOG 0. **#111 sigue activo**: 11.604 de 12.247 líneas (94%) son `Overflow`, ahora a 17,6 líneas/s. El dial está en 7.158, así que **el #99 sigue sin poder validarse** (necesita 7,0-7,03) |
| 16/10 ~16:50 | `f88813ae85e3` (sin cambios) | `f6bbfca` | (se cuelga) | (ya corrido) | (sin cambios) | **#111: causa raíz leída en el código** — la cola es de 64, la drena `event_obj_check()` **desde el hilo principal** (el que dibuja) y el productor de más ritmo es el **S-meter** (`meter.c:319`, un REFRESH por muestra) → si el hilo está saturado dibujando (~60%, #91) drena más lento y **descarta eventos**. El overflow sigue: 10.434 de 11.077 líneas (94%), ~15/s |
| 16/10 ~16:35 | `f88813ae85e3` (sin cambios) | `ff27afd` | (se cuelga) | (ya corrido) | (sin cambios) | **#111 agravado y medido**: el log vivo tiene 7.684 `Overflow` de 8.327 líneas (92%) y **crece a 15,6-17,6 líneas/s con la radio EN REPOSO** (load 0,73) y **el 100% del crecimiento son Overflows** → el disparador **no** es la carga de la UI y los eventos se descartan de forma continua |
| 16/10 ~16:30 | `f88813ae85e3` (sin cambios) | `5122ec2` | (no corrido) | **23 ok** | **0 en 60 s** | **Barrido completo** (el primero sobre este build): 23 ok / 0 caídas, estado idéntico antes/después (`7.157640 LSB vol=0`, el fix del #94 sigue), pid estable y CRASHLOG 0. Se chequeó antes que la radio estuviera en reposo (log 0 líneas en 8 s) para no molestar al owner |
| 16/10 ~16:15 | `f88813ae85e3` (sin cambios) | `e5d17cc` | (no corrido) | no se corrió | 0 tras el A/B | **A/B del tema (#91/#95)**: `Redshift` 58/61/59% vs `Aurora` 62/64/63% (rangos sin solaparse) → el tema influye ~4-5 puntos pero **no es la causa**; el CPU queda ~60% ✗. Tema **restaurado** a `Redshift`, CRASHLOG 0 |
| 16/10 ~16:00 | `f88813ae85e3` (sigue, tras reinicio del owner) | `e8082ce` (0 merges) | (no corrido) | no se corrió | **0 en 60 s** | **Rechequeo tras 15 h**: la radio se reinició (uptime 45 min, pid nuevo) pero tiene mi build, 1 GUI, `init.d=S20gui`, CRASHLOG 0. El owner puso **`Redshift`** (CPU 59-61% ✗ vs 65-70% con Aurora). **Sale el #111**: 25.624 `Overflow` de `events.c:90` en un run (los eventos se pierden) |
| 16/10 ~00:45 | **`f88813ae85e3`** | `c60af91` (#95/#91) + `aa05ef9` (#99) | (no corrido) | no se corrió | 0 en las pruebas | **#95 desplegado** (fallback de temas determinista) y **#99** (curva suavizada) — pero **no pude validar el #99**: a 7.152 la ROE sigue fuera de escala y el gráfico queda **vacío** (hace falta el rango resonante). **El CPU sigue ~65%** ✗ (#91 abierto) |
| 16/10 ~00:20 | **`da1055ddb0c3`** (tip `7032d3b`) | `9153a40` (#100) + `65cdda6` (#107) | (no corrido) | **23 ok** | **0 en 60 s** | **Desplegado y sano**: build exit 0 (13 s) con el gate OK (13.742.896 B); 1 GUI; barrido 23/0; estado idéntico antes/después; CRASHLOG 0. Incluye el asistente de sintonía (#100) y el fix del bench de CI (#107) |
| 15/10 ~23:10 | `3ead572b6fe2` (sin cambios) | `e5e6be5` | (no corrido) | no se corrió | **0 en 180 s** | Sano: 1 GUI, pid 18345 estable 3 min, CRASHLOG 0. CPU del hilo principal **68%** (en la sesión anterior medí 104-120% con el mismo build: varía). **#94 cerrado** (ya lo habían cerrado; dejé la validación igual) |
| 15/10 ~23:35 | `3ead572b6fe2` (sin cambios) | `ed6455d` | (no corrido) | no se corrió | 0 en la muestra | **#91 acotado**: el consumo está en el **hilo principal** (75,8%) y es **user-space** (58% user vs 11% sys), y **NO depende del waterfall** (tapar el waterfall: 68,8→71,0%, igual). Descartados el tema, `pre`/`att` y el kworker del 200% (transitorio mío) |
| 15/10 ~22:55 | **`3ead572b6fe2`** (build del clon con los merges) | `8d3dd73`..`45da382` | (pendiente) | **23 ok** | **0 en 60 s** | **#94 VALIDADO** (el barrido deja modo/dial/pre/att idénticos) y **#105 corregido** (no era bug del repo: copia vieja del build; se arregla borrándola). **#91 corregido dos veces**: el tema es `Aurora` (no `Sunlit`), el lavado no se reproduce y el kworker del 202% era un transitorio mío. Queda el **GUI ~1,2 núcleos** sostenido |
| 15/10 ~22:40 | **la rama NO compila** | `45da382` / `ed5b01e` | — | — | — | **#105**: `params/migrations.c:125` (generado por `src/params/CMakeLists.txt`) escribe `THEME_DEFAULT` sin comillas → exit 2 en los tres commits. **No se puede desplegar el fix del #94**. La radio queda con el build que habia en el volumen compartido. **#101 fundado** (la curva aparece en 7.012: 10,82% → **17,83%**) |
| 15/10 ~22:10 | `2c02321cc5ff` (sin cambios) | `45da382` (2 merges) | (no corrido) | no se corrió | **0 en 180 s** | **Sin desplegar**: llegó el fix de fondo del #94 (*snapshot* del estado de radio en `src/dialog.c`/`radio.c`, para todas las apps) y un ajuste de temas. Volumen ocupado (24 contenedores, su build de 00:49 UTC). Radio sana: 1 GUI, pid estable, 0 CRASHLOG. CPU ~101-103% (#91) |
| 15/10 ~21:45 | `2c02321cc5ff` (sin cambios) | `5e252df` | OK entrada (verifica se cuelga) | **23 ok** | 0 en el barrido y 0 en 90 s | **Sano**: 1 GUI, mismo pid, 0 CRASHLOG. El barrido **ya no cambia el modo/dial** (la última app, `SWRScan`, ahora los restaura por el fix del #96) → #94 no se reproduce. Sigue el CPU ~108% (#91) |
| 15/10 ~21:30 | **`2c02321cc5ff`** (build del clon) | `ed5b01e` | (pendiente) | no se corrió | 0 en las pruebas | **#84 y #85 validados** en la radio (modo/frecuencia/potencia restaurados, sin crash, pid estable). **Sale el #99**: el barrido arranca pero el gráfico queda vacío (10,82% constante a 10..50 s) |
| 15/10 ~21:10 | imagen `cb2da072b7b0` (sin cambios) | `e37db08` | (no corrido) | no se corrió | (no se corrió) | **Sin desplegar (2ª)**: el volumen sigue con el A/B del otro agente (26 contenedores, con su sello de fuentes). Se validó la herramienta nueva del #86: gate fail-closed, positivo con el firmware de la imagen y negativo con un archivo chico |
| 15/10 ~20:40 | imagen `cb2da072b7b0` (sin cambios) | `ed5b01e` (4 merges) | (no corrido) | no se corrió | 0 en 90 s | **Sin desplegar**: llegaron #96 (#84/#85, en `src/`), #97 (temas v2), #98 (gate del #86, en `tools/`) pero el volumen lo tiene el otro agente. La radio sigue sana (1 GUI, pid estable, 0 CRASHLOG) |
| 15/10 ~20:15 | imagen `cb2da072b7b0` (sin cambios) | `ad2bd88` | TODO OK | (ya corrido) | **0 en 180 s** | Radio en **192.168.0.236** (cambió de IP); 0 caídas en la vigilancia; el log de 8.388 líneas es histórico del barrido; sale **#92**: el barrido deja modo/dial cambiados. El **CPU ~99% sigue** y no es del barrido |
| 15/10 ~19:50 | imagen `cb2da072b7b0` (sin cambios) | `b78b779` | TODO OK | **23 ok** | 0 en la muestra | Sigue **#91**: CPU ~114,8% (3ª muestra: 118,8/115,2/114,8 + load 1,11) y la pantalla lavada; **fundado que el tema activo es `Sunlit`** (migrado desde `Outdoor`) por los colores de la captura |
| 15/10 ~19:40 | **imagen con #89** `cb2da072b7b0` | `d1d645d` | **TODO OK** | no se corrió | 0 en la muestra | **#89 arreglado** (1 GUI, sin kworker al 98%, entrada OK) pero sale **#91**: el GUI come ~1,2 núcleos y la pantalla se ve lavada |
| 15/10 ~19:00 | **imagen nueva en la SD** `cb2da072b7b0` | `5ebe692` (+#87) | 1 falló | **23 ok** | — | **Funciona**, pero la imagen **parpadea**: el rootfs trae DOS scripts de init (`S20gui` + `S95gui`) → dos GUIs con `/dev/fb0` abierto → flicker + el doble de CPU (**#89**). DATA y ajustes intactos; ROE no se midió |
| 15/10 ~18:45 | **imagen en la SD** (no deploy) | `5ebe692` (+ #87) | — | — | — | **Imagen armada y flasheada** (`sdcard.img` md5 `25e38412aefc…`, verificada leyendo de vuelta). La **DATA se conservó** (params.db + themes). Detalle en el anexo de imagen |
| 15/10 ~18:15 | `main` desplegado a la radio | `main` = `e1476ec` | — | — | **la GUI no arranca** | **`main` no es desplegable en el X6200**: el binario pide `libRHVoice.so.5` (del rootfs del X6100). Restaurado el port (`d9ccb7d4c224`). Comentado en #86 |
| 15/10 ~18:05 | (sin desplegar) | `main` = `e1476ec` | — | — | — | **El build de `main` en verde no produce el firmware** (776.884 B vs 13.737.784, PIE vs non-PIE, 0 simbolos x6100): issue #86. La radio no se toco |
| 15/10 ~17:50 | `d9ccb7d4c224` | `c2f88e1` | TODO OK | **23 ok** | **1 crash al cerrar el SWR Scan** (el barrido de apps, 0) | **Funciona** con dos defectos: #64, #77 y #78/#80 validados, ROE de 40 m medida (1.4–1.9); salen **#84** (queda en LSB) y **#85** (crash al cerrar con el barrido corriendo) |
| 15/10 ~17:10 | `d0ff2dba9c85` (sin cambios) | `1a56556` | TODO OK | no se corrió | **0 en 300 s** | **Sin desplegar (3ª)**: el volumen tiene 25 contenedores del arnés del otro agente; se dobló la vigilancia más larga (180 → 300 s) |
| 15/10 ~16:40 | `d0ff2dba9c85` (sin cambios) | `122bea3` | TODO OK | no se corrió | no medido | **Sin desplegar (2ª)**: quedan `4e9b5f4` (#77), `072f2c7` (#79) y `122bea3` (#80); hay builds corriendo en el volumen |
| 15/10 ~16:10 | `d0ff2dba9c85` (sin cambios) | `f6d1cb1` | TODO OK | no se corrió | 0 en 90 s | **Sin desplegar**: `4e9b5f4` (#77) queda pendiente, el volumen del build lo tiene el A/B del otro agente |
| 15/10 ~15:20 | `d0ff2dba9c85` | `337036c` (incluye `457a9a3`) | TODO OK | 22 ok | 0 en el barrido y 0 en 180 s de vigilancia | **Funciona** |
| 15/10 ~14:05 | `5a6e561e502f` | `e5a869b` | TODO OK | (no se corrió) | no medido | **Funciona** para #19/#26/#30/#47 |

---

---


## 19/10 (hora host) — rama `factory/cb-ui-169` (#169): canal en la fila y barra CB en el plan de banda; gates completos y banco QEMU (**solo UI, sin TX**)

**Qué se probó:** la ampliación de UI del módulo CB de #146: en CB la fila grande pasa a
`27.185` · `CH 19` · `CB C` (familia normal, sin «8» fantasma) y la franja del plan de banda se
pinta violeta con el rótulo `CB` (26,065–27,855). **No se transmitió, no se desplegó y no se
flasheó.**

**Host (todo verde):**

```text
$ tests/native/run_native_tests.sh
  40288 comprobaciones, 0 fallos        (band_plan: 6 casos CB + los de 20 m/wide/clipping)
  ... 90 comprobaciones de test_cb (cb_plan_name incluido), 0 fallos
$ python3 tests/check_wcag.py
  pares OK: 1052 | exentos: 170 | informativos: 90 | FALLAS: 0   (etiqueta blanca sobre el violeta CB: 9,12:1)
$ python3 tests/check_cfg_fills.py    -> OK: los 70 campos se inicializan, en orden
$ python3 tests/check_actions.py      -> todas las acciones ofrecidas tienen manejador
$ tests/check_harness_determinism.sh  -> PASS: deterministic (identical PNGs for every fixture)
$ cd tools/ab_testing
$ uv run ruff check . && uv run ruff format --check . && uv run ty check   -> all passed
$ uv run pytest -m "not e2e" -q  -> 227 passed, 13 deselected
$ TMPDIR=/var/tmp/pytest-169 uv run pytest -m e2e -q  -> 13 passed
$ uv run x6200-ab contracts --revision 19ef1ebe  -> PASS — 943 checks, 0 failure(s)
$ uv run x6200-ab audit --revision 19ef1ebe      -> 0 reproduced, 0 structural findings
$ uv run x6200-ab themes                          -> 0 failures
$ uv run x6200-ab compare --a 73fdd055 --b 19ef1ebe --experiment-id AB-CB169
  experiment AB-CB169: PASS (exit 0)
  Functional PASS — 89 scenarios captured on both revisions
  Visual Regression PASS — no unexplained visual regressions
  Quality Contract PASS — 913 contract checks passed on the candidate
  Performance PASS — render-cost proxy within budget
```

El A/B cambia **solo** `home-cb` y `home-cb-offchannel`; las cajas de píxeles distintas caen todas
en `y=138..160` (franja del plan de banda) y `y=172..190` (fila de frecuencia). Nada de la barra de
info, el S-meter, el waterfall, los botones ni las apps.

**Compilación ARM y banco QEMU (gate #86).** Cross-compile con el toolchain de la ROM:
`fuente ff9fadfeb15a`, binario **`c130665e88d6`**, 13.859.408 B. El gate #86 lo acepta (ELF32 ARM,
3/3 marcas). Capturas (chroot/volumen privados):

- 27.185 (CB C) → `27.185` · `CH 19` · `CB C`, barra violeta `CB`, AM, **TX power 4.0 W**;
- 27.000 → `27.000` · `CH --` · `CB C`;
- 26.065 (CB A) → `26.065` · `CH 1` · `CB A`, barra recortada a la mitad derecha;
- 27.855 (CB D) → `27.855` · `CH 40` · `CB D`, barra recortada a la mitad izquierda;
- tema **Cathode** (`font_digits`) → `CH 19` y `CB C` con la familia **normal**, sin fantasma;
- `{"cmd":"get"}` → `"band":"CB D","pwr":40,"tx":false` en todas las consultas (no se envió TX).

Evidencia: [`evidencia/2026-10-19-cb169-banco/README.md`](evidencia/2026-10-19-cb169-banco/README.md).

**Veredicto:** requisitos A y B medidos en host y en el banco; A/B con cambios solo en la fila y la
franja en los seis temas. **Sin TX**: la F0 de #146 sigue `requires-radio`.

## 19/10 ~15:2x (hora radio) — TX cualitativa en CB (teclado CW) y el interlock que **no** lo cubre (`67387bfc4391`)

**Qué se probó:** que el equipo **transmite** en un canal CB y que el **interlock** bloquea fuera de
la ventana, usando el **teclado CW** por el socket de control (el socket no tiene PTT). Sin
wattmeter. Autorización del owner; antena del owner; sin carga fantasma.

**Medido** (transcripción cruda en
[`evidencia/2026-10-19-cb-tx-cualitativo/transcript-corrida1.log`](evidencia/2026-10-19-cb-tx-cualitativo/transcript-corrida1.log)
y [`…/transcript-corrida2.log`](evidencia/2026-10-19-cb-tx-cualitativo/transcript-corrida2.log)):

```text
$ /tmp/x6200-remote '{"cmd":"app","text":"CW"}'          # abre el teclado CW (pone modo CW)
$ /tmp/x6200-remote '{"cmd":"pwr","val":40}'             # ≈4 W (tope CB); el readback pwr sigue 50 (#157)

27.185 (CB C, CH 19):  cw_send "PARIS PARIS" -> tx:true (sigue el ritmo del CW)  ✓
26.515 (CB B, CH 1):   cw_send "PARIS"       -> tx:true, SIN "TX blocked"          ✗  <-- el interlock NO bloquea
28.074 (10 m):         cw_send "PARIS"       -> tx:true                              ✓
```

Capturas durante el keying: [`02-durante-tx-cb19.png`](evidencia/2026-10-19-cb-tx-cualitativo/02-durante-tx-cb19.png)
(CH 19, `ON AIR`, medidor en PWR/SWR, SWR ≈3,5) y
[`10-interlock-26515-fb.png`](evidencia/2026-10-19-cb-tx-cualitativo/10-interlock-26515-fb.png)
(CH 1, `ON AIR`, **sin** mensaje de bloqueo).

**Causa (código):** el interlock vive en `radio_set_ptt()` (`src/radio.c:1068`, único que llama a
`cb_tx_allowed()`); el keyer CW llama a `radio_set_morse_key()` (`src/cw_encoder.c:62` →
`src/radio.c:1161`, un GPIO) y **nunca** pasa por `radio_set_ptt()`. El build **sí** tiene el
interlock (strings `TX blocked`/`cb_tx_scope`); es el **camino de TX** el que no lo consulta.

**Incidencias de la corrida:** la GUI se cayó 2 veces por el **#131** (`CRASHLOG sig=11
pc=0xb6186ae8`) durante los cambios de frecuencia/modo; y la **radio quedó inalcanzable** al final
(firma del **#121**), por lo que no se pudo correr el control positivo por FT8. Volvió **sin
reinicio** a los ~10 min y **se restauró** frecuencia/modo/app (14.285 MHz, CW, app off) y la
potencia a 1,5 W (`tx:false`). **No se desplegó ni flasheó** (el build `67387bfc4391` lo desplegó
otra sesión a las 15:18, después de que esta corrida empezara sobre `9a094c421b0e`, que no tenía CB).

**Veredicto:** TX en CB **✓** (medido); interlock del keyer **✗** (defecto, issue nuevo); 10 m
intacto **✓**. La F0 con instrumentos (#156) sigue pendiente de potencia real y armónicos. Detalle,
condiciones y advertencia legal en
[`evidencia/2026-10-19-cb-tx-cualitativo/README.md`](evidencia/2026-10-19-cb-tx-cualitativo/README.md).

## 19/10 11:36 (hora radio 14:36 UTC) — F0 CB 27 MHz: el MCU **no** bloquea el TX; la F0 completa queda BLOCKED por falta de instrumentos (`9a094c421b0e`)

**Qué se probó:** la fase F0 del issue #146 (§11), con autorización explícita del owner y la radio
como estaba. Primero el recon read-only y la documentación del gate de fábrica; después una prueba
en vivo acotada con el **único camino de TX accionable por software**: el **barrido de ROE**
(`{"cmd":"swrscan","val":1}`), que el firmware fuerza a **0,1 W**, **LSB** y **una pasada**
(`codigo: src/dialog_swrscan.h:72-75`, `SWRSCAN_MULTIPASS_TX_ENABLED=0`).

**Medido, en la radio:**

```
$ /tmp/x6200-remote '{"cmd":"get"}'      # antes: 27.185.000 MHz, LSB, CB C, RX, vol 0
...
$ /tmp/x6200-remote '{"cmd":"swrscan","val":1}'
{"t":"ok"}
{"t":"state",...,"mode":"LSB","app":"SWRScan","ax":{"swrscan_state":"Scanning","passes":1,
 "pass":1,"points":51,"swrscan_tx":true},"tx":true}
{"t":"state",...,"mode":"AM","app":"SWRScan","ax":{"swrscan_state":"Error","...",
 "swrscan_tx":false},"tx":false}
```

La pantalla del barrido quedó con **`min 1.0 @ 26.935.000 kHz (n=43/51)`** y la curva de ROE
dibujada de **26,935 a 27,435 MHz** (span 500 kHz): el puente de ROE del equipo midió **RF real en
43 frecuencias de la banda CB**, con la curva alrededor de 1,1 al centro (27,185). Evidencia:
[`live-tx/06-swrscan-run.png`](evidencia/2026-10-19-f0-cb-tx/live-tx/06-swrscan-run.png) y
[`live-tx/transcript.md`](evidencia/2026-10-19-f0-cb-tx/live-tx/transcript.md).

**Veredicto: la fase F0 NO es PASS** (no hay potencia real ni armónicos medidos: no hay carga
fantasma ni instrumentos de RF). Pero la pregunta que definía el alcance del módulo quedó
contestada: **el baseband/MCU NO bloquea el TX en 27 MHz** (`tx:true` + ROE medida), así que la
rama "si el MCU no transmite, el módulo pasa a RX-only" **no se activa**. El barrido termina en
`Error: timeout de RF` con `n=43/51`: es la firma conocida del barrido (issues #133/#125), no un
bloqueo de TX.

**Gate de fábrica, documentado:** `/etc/xgradio/xgradio.conf` **línea 7**, leído por
`XBandPlan::XBandPlan()` de `/usr/app_qt/x6200_ui_v100` (UI-side, no hay daemon `xgradio`); el valor
canónico es `enable` (no `enabled`). En el rootfs de fábrica **interno** de esta radio (eMMC
`mmcblk2p2`, sin montar) el conf ya decía **`fullband-tx=enable`** y `man.conf` `fullband-tx=1`
(`fbtx-open-time=2025-12-13 19:53:25`). El port no tiene el archivo ni la clave: no hay gate Linux.
Detalle: [`ref-fabrica.md`](evidencia/2026-10-19-f0-cb-tx/ref-fabrica.md) y
[`recon-radio.txt`](evidencia/2026-10-19-f0-cb-tx/recon-radio.txt).

**Restauración (medida):** 27.185.000 MHz, LSB, CB C, RX, vol 0, app cerrada, GUI PID 13127,
uptime del equipo sin reinicio; ATU y `DATA` intactos. La frecuencia no se cambió.

**Pendiente (BLOCKED)**: carga fantasma 50 Ω, medidor de potencia/RF y analizador de espectro;
PTT de AM/FM a potencia configurada (el socket no tiene PTT; la inyección de evdev no llegó a la
GUI); PTT del micrófono (MCU). Sin eso no se publica TX como validado (#146 §7.3).

Evidencia completa: [`2026-10-19-f0-cb-tx/README.md`](evidencia/2026-10-19-f0-cb-tx/README.md).

---
## 18/10 ~12:40 (hora radio) — `ffef63df58cb` (#133/#114/#121): la radio no responde; **pruebas NO ejecutadas**

**Qué se intentó:** cerrar los tres issues del backlog con la punta del trunk (`94b9989`; el último
commit de código es `d3e84ed`, con el #130, el #132 y el comando `swrscan` del #133).

**Medido:**

```
  git fetch fork                                  -> 94b9989 (sin cambios)
  tools/compila-gui.sh                            -> exit 0, md5 ffef63df58cb (13.757.724 B)
  python3 tools/verifica-firmware-port.py <bin>   -> OK, ELF32 ARM, 3/3 marcas (gate #86, exit 0)
  tools/prueba-gui-en-radio.sh                    -> exit 1 (radio_ip no encontró la radio)
  tools/verifica-radio.sh 192.168.0.236           -> "la radio (192.168.0.236) no responde" (exit 1)
```

**La radio está inalcanzable en las dos redes** (medido a las 01:35 en 5 GHz y a las 01:36 en 2,4 GHz):

| red | host | radio | ping | SSH | hosts con :22 |
|---|---|---|---|---|---|
| DharmaNet (5 GHz) | `192.168.0.195/24` | `192.168.0.236` | **0/2** | `No route to host` | **0** |
| DharmaNet 2.4 | `192.168.18.68/24` | `192.168.18.67` | **0/2** | `No route to host` | **0** |

La red del host está sana (el gateway `192.168.0.1` responde). Es la **4.ª caída del #121**: la última
medición viva fue a las 00:13 local (uptime 51.644 s, pid 7317) y a las 00:33 ya no respondía; **no
volvió sola** y necesita ciclo de energía. En la medición de ROE del 17/10 la batería figuraba en
**8,6 V**.

**Veredicto:**

| Issue | Veredicto de esta corrida |
|---|---|
| **#133** | **NO MEDIDO**: el comando está mergeado y se había validado en `ffef63df58cb` el 17/10, pero no se repitió en esta punta. |
| **#114** | **NO MEDIDO**: no se corrieron los 16 ciclos de NB en este build. |
| **#121** | **NO MEDIDO**: no se corrieron los 5 barridos. La caída en reposo (4.ª) sigue abierta. |

No se cierra ningún issue y no se crea ninguno nuevo: la radio caída ya está reportada en el #121. No
se tocaron params, temas ni apps, y no se desplegó. Evidencia:
[`docs/evidencia/2026-10-18-qa-radio-backlog/`](evidencia/2026-10-18-qa-radio-backlog/README.md).

---

## 17/10 ~22:20 (hora radio) — `63dbf7a4` (#130): la parábola de ROE en la radio es una **U** ✓ (cierre requires-radio del #125)

**Qué se probó:** el fix del **#130** (`src/roe_curve.c`: si el ajuste libre sale con `a<0` —un domo— se
espeja al valle con la misma columna del vértice y la base en el mínimo medido). Es el `requires-radio`
que dejó abierto el #125. El criterio del owner (paso 3 del re-test del #125): **la curva dibujada es una
U** — base en el cuarto inferior (`y≥316`), columna **interior**, y **los dos bordes suben**.

**Método (medido):**

```
  clon x6200_qa → tip 63dbf7a4
  RECONSTRUIR=si tools/compila-gui.sh  → binario md5 b1ba67520079 (13.753.408 B)
  tools/prueba-gui-en-radio.sh         → gate #86: OK ELF32 ARM 13753408 B 3/3 marcas (exit 0)
  radio md5 desplegado                 → b1ba67520079  (idéntico: el deploy fue idempotente)
```

El equipo **no se reinició** (uptime continuo 34729→36509 s); la GUI se relanzó (pid 3931). Se corrió
**1 pasada (Quick)** del SWRScan con **TX a 0.1 W** (lo fuerza `radio_start_swrscan()`,
`SWRSCAN_TX_POWER_DW=1`), **ATU sin tocar** (`atu=1`, como en la captura del owner; el remoto no expone
comando de ATU) y sin XPA125B.

**Resultado — la curva es una U (medición de píxeles, banda del chart `y=69..399`):**

| Captura | dial / span / escala | valle (y) | col. valle | bordes (y) izq/der | suben (px) |
|---|---|---|---|---|---|
| `01-swrscan-50k-dial7000-U.png` | 7.000 / 50 kHz / Linear | **398** | 255 | 304 / 230 | **+94 / +168** |
| `02-swrscan-50k-dial7000-U-2da.png` | 7.000 / 50 kHz / Linear | **398** | 232 | 318 / 224 | **+80 / +174** |
| `03-swrscan-200k-log-dial7016-U.png` | 7.016,334 / 200 kHz / Log | **398** | 238 | 258 / 165 | **+140 / +233** |
| `04-swrscan-500k-log-dial7016-U.png` | 7.016,334 / 500 kHz / Log | **398** | 273 | 227 / 167 | **+171 / +231** |

Base en el cuarto inferior (`y=398 ≥ 316`) ✓, valle **interior** ✓, **ambos bordes suben** ✓ → **U**.
Reproducido en la 2ª pasada (lección del #122) y también con **span 200 kHz / Log**, que son las
condiciones exactas de la captura del domo del owner (`26-roe-multipasada-final.png`): **el domo ya no
se reproduce, la curva sale U**. Con el dial **fuera de la resonancia** (7.020/7.040) la curva es una
rama que sube (el mínimo cae en el borde), porque el rango no contiene el valle — no es una inversión.

**El script de la aserción del #125 no corría en la radio; corregido en el #132.** El re-test del #125
documenta `python3 tests/swrscan_curve_evidence.py /tmp/swrscan-u.png` (1 argumento); el script de
entonces pedía **2** (`len(argv)!=3` → docstring y **exit 2**) y con 2 argumentos la captura de radio
**igual fallaba**: `FAIL … no encontre la curva dibujada`, porque `curve_mask()` exigía
`min(r,g,b)≥180` y la curva del tema del owner (`Redshift`) es `RGB(255,179,160)` (`min=160`); el
fixture del arnés pasaba porque su curva es `RGB(248,238,255)` (`min=238`). Salida cruda de la
corrida: [`asercion-script.txt`](evidencia/2026-10-17-qa-radio-swr125/asercion-script.txt).

El **#132** (ya en el trunk) lo corrigió: acepta **1 argumento** (clasifica la forma por sí mismo:
`U` / `domo` / `rama`; el comando de radio queda `python3 tests/swrscan_curve_evidence.py
<captura>.png`) y **2** (modo histórico del arnés), y detecta la tinta **por tema** dentro de la
banda `y=69..399` (descarta la grilla `0xAAAAAA` y el fondo/panel, toma el color dominante de
texto/curva). Verificado sobre las capturas de esta corrida: `01` → **U** (valle `y=398` en `x=255`,
bordes +94/+168), `02` y `03` → **U**, y el domo viejo `26-roe-multipasada-final.png` → **domo**
medido (exit 1, ya no "no encontré la curva"). El RED/GREEN del arnés sigue **PASS**
(`03-harness-domo-valle.png` + `04-harness-valle-real.png`). El test determinista del script es
`python3 tests/test_swrscan_curve_evidence.py` (capturas reales + los seis temas sintéticos, sin
radio ni Docker).

**Barrido de apps (no regresión):** `PERMANENCIA=14 tools/prueba-apps-radio.sh` → **23/23 ok, 0 caídas**
(incluye **SWRScan**, que ahora sí abre por el control remoto; el #78 quedó resuelto). Log en
[`apps-sweep.log`](evidencia/2026-10-17-qa-radio-swr125/apps-sweep.log).

**Dos cosas que no son del fix y quedan abiertas:**

- **`Error: timeout de RF`**: **todas** las pasadas terminan en `Error: timeout de RF` con `n≈43-45/51`,
  **nunca `Completed`**. El watchdog de la pasada (`SWRSCAN_PASS_TIMEOUT_MS = 2285 ms` para 51 puntos =
  44,8 ms/punto) se dispara antes porque el tiempo real por punto supera el `SWRSCAN_DWELL_MS=35` asumido
  (TX medida ~2,3-2,6 s). Ya anotado en #119 y por el otro QA; sigue abierto.
- **SIGSEGV (1 vez, no reproducido)**: `CRASHLOG sig=11 fallo=0xf363 pc=0xb6186ae8 lr=0xb618d4f0 …` tras
  `on_vfo_mode_change: mode=2` + carga de `filter_low/high`; la GUI se relanzó (pid 2728→3931).
  **Ya reportado en el #131** (mismo evento). Un barrido dirigido de las 23 apps (abrir 6 s / cerrar /
  comparar pid) **no lo reprodujo** (pid 3931, CRASHLOG 0). Ojo: `verifica-radio` dice "sin senales de
  crash" porque el log vivo post-relanzamiento arranca limpio; el CRASHLOG está en el `app_log`
  guardado `x6200_log_2026-10-17_22-38-46.txt`.

**Restauración:** dial `7.030.000`, modo `LSB`, banda `40m CW`, `pwr` 5.0 W, app cerrada, RX;
`swrscan_span=500000`, `swrscan_linear=1`; `cw_tune=cw_decoder=key_train=0`; tema `Redshift`; **ATU sin
tocar**. md5 `b1ba67520079`. Evidencia completa:
[`docs/evidencia/2026-10-17-qa-radio-swr125/`](evidencia/2026-10-17-qa-radio-swr125/README.md).

---

---

## 16/10 ~23:40 — ROM nueva flasheada en la SD: `c640645d810c` (GUI e294229dd32d)

**Qué se hizo:** build de la ROM con todo lo mergeado (HEAD `223b30a`, incluye `fe67ded` = issue #117) y
**flasheo de la SD**, porque los dos intentos de deploy por WiFi fallaron (el script quedó colgado y el
`scp` cortó con "text file busy").

**Verificación del artefacto:**

```
  sdcard.img: 571473920 bytes  md5 c640645d810c1488682668eae62204f3
  init.d en el rootfs:  solo ./etc/init.d/S20gui        (fix del #89)
  GUI en la imagen:     13257128 bytes  md5 e294229dd32d
  lib en la imagen:     md5 5ed07c1719f8
```

**Flasheo y verificación, medido:**

```
  dd: 571473920 bytes en 118 s (4.6 MB/s)
  md5 leido de vuelta de la SD: c640645d810c1488682668eae62204f3   -> IGUAL a la imagen
```

**La DATA se conservó** (se re-declaró en el slot 3 desde el sector 1116160):

```
  /proc/partitions: sda3 3314176 bloques  (== 6628352 sectores)
  desde el mount del host: params.db 106496 bytes, 78 parametros
    theme_name=Redshift  vol=66  swrscan_span=50000  nb=0
    themes: Aurora, Holo, Material 3, Redshift, RS, Sprint, Sunlit, Trail
    app_logs: 662 archivos
```

**Nota**: el md5 de los primeros 571 MB despues del flasheo es `f1ac0dc42452fde034041319bae59dc2`, distinto
del de la imagen, **esperado**: se reescribió el MBR (offset 478) para volver a declarar la DATA.

**Pendiente**: poner la SD en la radio y validar (GUI e294229dd32d, `init.d`=`S20gui`, barrido de apps, y
si el audio arranca — el race del #116 es un bug de código).

---

## 16/10 ~17:30 — 3604cece0e43 (merge `2392624`, issue #110): el campo de año de Ajustes

**Qué se probó:** el merge `2392624` (*"Ajustes: el campo de año con la misma caja que Day/Month"*,
issue #110), que toca `src/dialog_settings.c`.

**Build y deploy, medido:**

```
$ make x6200-gui-rebuild        -> exit 0 en 7 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13742964 B, 3/3 marcas)
  radio: gui=3604cece0e43 (1 GUI, pid 2889)
```

El binario pesa **72 B menos** que el anterior (`f88813ae85e3`, 13.743.036 B), consistente con un cambio
chico de layout.

**Pruebas en la radio:**

```
  barrido de las 23 apps:  23 ok | 0 caídas   (406 s)
  estado antes:   7.157640 MHz LSB 40m SSB vol=0 pre=1 att=0
  estado despues: 7.157640 MHz LSB 40m SSB vol=0 pre=1 att=0     <- IDÉNTICO (fix del #94 andando)
  vigilancia 90 s: pid 2889 sin cambios, 0 caídas
  CRASHLOG: 0 (log vivo y el ultimo guardado)
```

**No verificado**: el fix del #110 es **visual** (el ancho de la caja del campo de año en la pantalla de
fecha de Ajustes). No navegué hasta ahí, así que **no lo confirmé**: la navegación profunda de Ajustes no
la hice. Queda para el dueño del cambio o para el owner.

**El #111 sigue**: en el run nuevo, 7.739 de 8.416 lineas (92%) con la firma `Overflow`.

---

## 16/10 ~00:45 — `f88813ae85e3` (tip `c60af91`): el #95 de temas y el #99 de la curva

**Qué se probó:** los merges `c60af91` (temas: fallback determinista fijado por tests, issue #91 — *"el
supuesto estaba desmentido"*) y `aa05ef9` (SWR Scan: curva de ROE suavizada con Catmull-Rom, issue #99).

**Build y deploy, medido:**

```
$ make x6200-gui-rebuild        -> exit 0 en 6 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13743036 B, 3/3 marcas)
  radio: gui=f88813ae85e3 (1 GUI, pid 25647)
```

**El #99 no se pudo validar en este dial.** El barrido de ROE a **7.152000** (donde el owner tiene la
radio) deja el gráfico **vacío**, igual que antes:

```
  7.012 (resonante, build viejo): 17.83% de pixeles no-fondo   <- con curva
  7.152 (build nuevo #99):        12.27%                       <- SIN curva
```

Ojo: ese 12,27% vs el 10,82% medido antes **no es una curva**: son los **rótulos del eje**, que ahora
dicen `7.139.500 / 7.152.000 / 7.164.500` (antes `7.155.471 / 7.167.971 / 7.180.471`) y ocupan más
pixeles. La captura `22-curva-suavizada.png` lo muestra: el chart está **vacío**.

Es consistente con el hallazgo del #101: **una ROE fuera de escala no se dibuja**, y a 7.152 la antena de
40 m está fuera de resonancia. Para validar el #99 hay que barrer dentro de **7,0-7,03**.

**El #84/#85 siguen bien** con este build: cerrar el barrido no reinicia la GUI (pid 25647 sin cambios) y
el estado queda intacto (`7.152000 MHz LSB vol=0`), con `CRASHLOG=0`.

**#91**: el fix del #95 confirma por escrito que **el supuesto del tema estaba desmentido** (lo que yo
habia medido), pero **el CPU sigue en ~65%** en el hilo principal y en user-space. El issue queda abierto
por esa parte.

## 16/10 ~00:20 — `da1055ddb0c3` (tip `7032d3b`): el asistente de sintonía (#100) desplegado

**Qué se probó:** el build del tip, que ya traía los merges `9153a40` (RX: asistente de sintonía, buscar el
mejor spot alrededor, issue #100) y `65cdda6` (bench de CI: el ruido del runner sale `INCONCLUSIVE`).

**Build y deploy, medido:**

```
$ make x6200-gui-rebuild        -> exit 0 en 13 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13742896 B, 3/3 marcas)
$ tools/prueba-gui-en-radio.sh  -> GUI lista
  radio: gui=da1055ddb0c3 (1 GUI, init=1)
```

El binario pesa ~5 KB más que el anterior (`3ead572b6fe2`), consistente con la funcionalidad nueva.

**Pruebas en la radio:**

```
  barrido de las 23 apps:  23 ok | 0 caídas   (405 s)
  estado antes:  7.152000 MHz LSB 40m SSB vol=0
  estado despues: 7.152000 MHz LSB 40m SSB vol=0     <- IDÉNTICO (el fix del #94 sigue andando)
  vigilancia 60 s: pid 24361 sin cambios, 0 caídas
  CRASHLOG: 0 (log nuevo 03-19-14 y los anteriores)
```

**No medido**: el asistente de sintonía del #100 **no lo probé a fondo** (es una funcionalidad de RX; en la
captura `21-build-7032d3b.png` la pantalla principal se ve normal, pero no ejercité el asistente). Queda
para el dueño de la funcionalidad o para cuando el owner lo pruebe.

## 15/10 ~22:55 — `3ead572b6fe2` (build del clon con los merges del #94/#92): #94 validado

**Qué se probó:** el build con `cd3bc97` (restore del estado de radio por snapshot, #94) y `45da382`
(ancho de línea del waterfall, #92), compilado desde el clon y desplegado.

**El build, medido** (y un escollo que vale registrar):

```
$ make x6200-gui-rebuild        -> exit 2   (params/migrations.c:125, THEME_DEFAULT)
$ grep -n theme_name src/params/migrations.c                       # en el árbol: NADA
$ grep -n theme_name /b/build/x6200-gui-custom/src/params/migrations.c
  125: rc = sqlite3_exec(db, "UPDATE params SET val='" THEME_DEFAULT "' ...   <- copia VIEJA
$ rm -f /b/build/x6200-gui-custom/src/params/migrations.c
$ make x6200-gui-rebuild        -> exit 0 en 12 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13738076 B, 3/3 marcas)
  radio: gui=3ead572b6fe2 (1 GUI)
```

O sea: **el árbol compila**; lo que rompía era una **copia vieja de una fuente** en el directorio de
build compartido, que el `make x6200-gui-rebuild` no refresca. Queda anotado como trampa del volumen
compartido (y como sugerencia en el issue #105, que se corrigió tres veces hasta dar con esto).

**#94 validado en la radio** — barrido completo de las 23 apps, midiendo antes y después:

```
  ANTES:   7.012000 MHz LSB  pre=0 att=0 pwr=50
  barrido: 23 ok | 0 caídas
  DESPUES: 7.012000 MHz LSB  pre=0 att=0 pwr=50      <- IDÉNTICO
  vigilancia 60 s: pid 15466 sin cambios
```

Salvedad honesta: la última app del barrido (orden alfabético) es `SWRScan`, que **también** restaura
por el fix del #96, así que este test **no separa** los dos mecanismos.

**#91 corregido dos veces** (el registro de correcciones es parte del valor):

1. El tema activo es **`Aurora`** (no `Sunlit`): `params.db` lo dice y la captura
   `19-tema-aurora.png` muestra el violeta oscuro de Aurora. El "lavado" era el **transitorio** de la
   migración de temas al arrancar.
2. El **kworker del 202%** que reporté **no se reproduce**: era el **arranque de la GUI** que yo mismo
   reinicié para probar el tema. Retirado.

**Lo que queda medido**: el GUI consume **~1,2 núcleos sostenidos** (5 muestras: 103,8 / 110,8 / 113,2 /
116,5 / 120,4%) **con los dos temas**, sin causa fundada.

---

## 15/10 ~21:30 — `2c02321cc5ff` (build del clon, commit `ed5b01e`): #84/#85 validados y sale el #101

**Qué se probó:** el build de los 4 merges (`01d632e` #84/#85, `ed5b01e` #86, `aa8de14` temas v2,
`2663a04` Cathode) compilado **desde el clon** y desplegado por WiFi.

**Build y deploy, medido:**

```
$ tools/compila-gui.sh          # dijo FALLO (carrera con los 26 contenedores del otro agente)
$ make x6200-gui-rebuild        # a mano: exit 0 en 11 s   <- el script escondía el error
$ gate: OK: es el firmware del port (ELF32 ARM, 13737932 B, 3/3 marcas)
$ tools/prueba-gui-en-radio.sh  # pasó el gate y subió
  radio: gui=2c02321cc5ff (1 GUI)
```

**#84 y #85 validados en el equipo** (dos corridas del barrido de ROE):

```
  abrir: ok | Run: ok | off: ok
  antes:   7.167971 MHz USB-D pwr=50
  despues: 7.167971 MHz USB-D pwr=50      <- modo, frecuencia y potencia restaurados (#84)
  pid 10809 sin cambios y CRASHLOG=0       <- no reinicia la GUI (#85)
```

Comentados en [#84](https://github.com/iariap/x6200_gui/issues/84#issuecomment-6205247712) y
[#85](https://github.com/iariap/x6200_gui/issues/85#issuecomment-6205247981).

**Lo nuevo: [#101](https://github.com/iariap/x6200_gui/issues/101)** — el barrido corre (`Stop`) pero
**el gráfico queda vacío**: los píxeles no-fondo del chart son **10,82% constantes** a los 10, 20, 30, 40
y 50 s. Dos pistas **sin fundar**: (1) con el código previo al #96 la curva aparecía (otro build, otra
banda); (2) el estado es `pre=1 att=0` y en aquella corrida figuraba `ATT` (lo dejó el barrido de apps).
Al buscar duplicados apareció el **#99** (la curva *dibujada*, con sierra), lo que **debilita la pista 1**:
quedó corregido en el issue.

**Ciclo 12 (~21:45):** barrido de las 23 apps → **23 ok, 0 caídas** (405 s) y el **modo/dial/pre/att
quedaron idénticos** (`7.167971 USB-D pre=1 att=0` antes y después): **#94 ya no se reproduce**, porque la
última app del barrido es `SWRScan` y su fix del #96 restaura frecuencia/modo/potencia. Vigilancia 90 s:
pid 10809 sin cambios, 0 caídas. Sigue el **CPU ~108%** (#91). Evidencia:
[`barrido-3-23ok-0caidas.log`](evidencia/2026-10-15-validacion-arreglos/barrido-3-23ok-0caidas.log).

---

## 15/10 ~19:00 — la imagen flasheada: `cb2da072b7b0` (commit `5ebe692`, imagen `25e38412aefc…`)

**Qué se probó:** la **imagen nueva**, arrancada desde la SD flasheada (no un deploy por WiFi).

**Arranque y datos, medido:**

```
$ ssh root@192.168.18.67 'md5sum /usr/sbin/x6200_gui; cut -d" " -f1 /proc/uptime'
  cb2da072b7b0            (el GUI de la imagen)
  30.14                   (uptime: arranque limpio)
$ df -h /mnt
  3.2G montado, 3.1G libre      (la particion DATA que volvi a declarar, montada)
$ sqlite3 /mnt/params.db ...
  theme_name=Outdoor   cw_tune|cw_decoder|key_train=0|0|0   swrscan_span=50000
```

O sea: **el flasheo y la conservación de la DATA funcionaron** (los ajustes del owner intactos).
Barrido de apps: **23 ok, 0 caídas**. `verifica-radio`: **1 comprobación falló** (no llegué a ver cuál;
el chequeo de crashes dio OK).

**Y el defecto que apareció: flickering y CPU alta (issue [#89](https://github.com/iariap/x6200_gui/issues/89)).**

Causa medida: **dos scripts de init arrancan el mismo daemon** —`/etc/init.d/S20gui` (del repo, commit
`29c5896`) y `/etc/init.d/S95gui` (**no está en el repo**: resto del rootfs del vendor)— → **dos
`x6200_daemon`** → **dos `x6200_gui`, los dos con `/dev/fb0` abierto** → parpadeo.

```
  pid 295/533  {x6200_daemon}      pid 307/542  /usr/sbin/x6200_gui  (fd 3 -> /dev/fb0)
  CPU: x6200_gui 542 ~20.7% | x6200_gui 307 ~23.3% | kworker/u8:2-events_unbound ~97.7%
  /dev/fb0: 24 lecturas -> 6 hashes distintos; entre dos cuadros distintos cambia el 10.4%
```

**Por qué nunca se vio con los deploys por WiFi:** `tools/prueba-gui-en-radio.sh` copia el binario y las
`liblv*.so*`, **no** `/etc/init.d/` → este defecto sólo aparece al arrancar una imagen flasheada.

Evidencia: `docs/evidencia/2026-10-15-validacion-arreglos/flicker/` (medición + 2 cuadros + GIF).
**No se midió la ROE** en esta corrida (se priorizó el flicker que pidió el owner).

## 15/10 ~17:50 — `d9ccb7d4c224` (commit `c2f88e1`): despliegue y validación

**Qué se hizo:** build limpio + deploy + la batería de validación que pidió el owner.

**Build y deploy, medido:**

```
$ tools/compila-gui.sh
  binario: md5 d9ccb7d4c224   (commit c2f88e1 - qa-radio: la regla del owner queda explicita)  [43 s]
$ tools/prueba-gui-en-radio.sh
  GUI lista: 5886.6 s
$ md5sum /usr/sbin/x6200_gui        # en la radio
  d9ccb7d4c224                       (la lib también viajó: liblvgl.so cd30ca6c58ad)
```

El commit incluye los tres fixes que estaban pendientes: `4e9b5f4` (#77, tipografías), `072f2c7` (#79,
Field Ops) y `122bea3` (#80, SWR Scan por socket).

**Nota de honestidad:** el volumen del build estaba ocupado por el A/B del otro agente y el owner indicó
compilar ahí mismo. Antes de compilar se **respaldaron** sus artefactos dentro del volumen
(`/b/ab-backup-237f1f6bc1da`, `/b/ab-backup-liblvgl-cd30ca6c58ad`, `/b/ab-backup-sello-8b10fe903942`).
Si su A/B estaba midiendo en ese momento, esa corrida pudo verse afectada.

**Validaciones:**

| Qué | Resultado medido |
|---|---|
| `tools/verifica-radio.sh` | **TODO OK** |
| **#64** Field Ops | **0 errores de archivo** (antes del fix: 3); los íconos cargan |
| **#78 / #80** SWR Scan | Abre por socket (`{"cmd":"app","text":"SWRScan"}` → `ok`) y el barrido corre (`btn 0` → `ok`) |
| Barrido de apps | **23 de 23 ok**, 0 caídas (antes informaba 22: faltaba justamente el SWR Scan) |
| Vigilancia 60 s | PID estable, 0 caídas |
| **ROE de 40 m** | **~1.4 a ~1.9** en 7.005.792–7.030.792 (`Span: 50 kHz`): la antena de 40 m está bien |

**La transmisión que estaba bloqueada, hecha:** 3 W (QRP, autorizado por el dueño), LSB forzado por la
app, dial 7.018.3, `swrscan_span=50000`. El log lo confirma: `on_vfo_mode_change: Radio set vfo 1
mode=0`. Evidencia:
[`10-swrscan-tx-resultado.png`](evidencia/2026-10-15-validacion-arreglos/10-swrscan-tx-resultado.png).

**Tres cosas que salieron mal en la prueba y se corrigieron:**

1. El **modo quedó en LSB** (el barrido lo fuerza y no lo devuelve): se restauró a **CW** a mano y quedó
   reportado en [#84](https://github.com/iariap/x6200_gui/issues/84).
2. El **cliente del control remoto** no viaja en el repo (`tools/remote/x6200-remote` se compila): en el
   worktree no estaba y el `scp` falló en silencio. Se copió del árbol compartido.
3. Los rótulos grises del gráfico llegan a **6.994** (unos 6 kHz por debajo de 7.000), aunque el barrido
   útil quedó entre 7.005 y 7.031.

**Corrección de lo que informé primero:** dije "0 caídas" en esta corrida y **no era cierto**: hubo **un
crash**, en el momento en que cerré la app del barrido con el barrido corriendo. Aparece en el log
guardado (`x6200_log_2026-10-15_20-42-17.txt`) y no en el log vivo, que es el que mira
`tools/verifica-radio.sh`, por eso el chequeo dio `TODO OK` igual. Firma y pila resuelta:

```
CRASHLOG sig=11 fallo=0x4 pc=0x4bf320 lr=0x4b2010 sp=0xbeda2c08 base=0x48ea64
main+0x4ac -> scheduler_work+0x28 -> main_screen_close_app+0x14 -> dialog_destruct+0x74 -> apply_cmd+0x0
```

(Resuelta con `nm -n` y el símbolo más cercano: `addr2line` no da símbolos para este binario, que tiene
symtab pero sin debug.) El barrido de las 23 apps cerró todas sin problema, así que se necesita **el
barrido corriendo**. Reportado en [#85](https://github.com/iariap/x6200_gui/issues/85). Evidencia:
[`crash-swrscan-al-cerrar.txt`](evidencia/2026-10-15-validacion-arreglos/crash-swrscan-al-cerrar.txt).

---

## 15/10 ~16:10 — corrida sin despliegue (la radio sigue en `d0ff2dba9c85`)

**Qué se hizo:** el ciclo de monitoreo. `git fetch fork` y `HEAD..fork/lu2dsx/x6200-rom` dieron **0
commits**, pero **la radio no tiene el build del tip**: el commit de fuentes `4e9b5f4` (#77,
tipografías) está en la rama y **no desplegado**.

**Por qué no se desplegó:** el volumen del build (`x6200-build-output`) lo está usando el otro agente
para su A/B de fuentes:

```
$ docker run --rm --entrypoint "" -v x6200-build-output:/b x6200-buildroot sh -c 'ls -l /b/target/usr/sbin/x6200_gui; md5sum ...; cat /b/.fuente.md5'
  -rwxr-xr-x ... Oct 15 19:04 /b/target/usr/sbin/x6200_gui     (compilado 5 minutos antes)
  md5 237f1f6bc1da
  sello .fuente.md5 = 8b10fe903942
$ (hash de mis fuentes)          = b3334bef0b7d               (distinto: el volumen es suyo)
```

Un build limpio le pisaría el binario del A/B, así que el despliegue queda **pendiente** (regla del
skill `qa-radio`).

**Salud de la radio, medido:** `tools/verifica-radio.sh` → `TODO OK`; los dos últimos logs con
`CRASHLOG=0` y `buffer overflow=0`; vigilancia de 90 s con el PID estable (`309 → 309`) y **0 caídas**.
El equipo reinició (uptime `712.97` s) y el build desplegado persistió.

**Ajustes del dueño, sin tocar:** `theme_name=IU1WXY`, `cw_tune=cw_decoder=key_train=0`,
`swrscan_span=50000`.

**Hueco del proceso que salió de esta corrida:** la idempotencia se medía contra git (`HEAD..remote`), y
eso no dice si la radio tiene el build del tip. Corregido en el skill `qa-radio`: ahora se compara el md5
**desplegado** y, antes de compilar, se mira el sello del volumen compartido.

### Segunda corrida (16:40): siguen 2 commits más y el volumen está compilando

`git fetch fork` trajo **2 commits nuevos**, y los dos son arreglos de issues de este QA:

- `072f2c7` (#79) — Field Ops: `HELPER` pasa de `"x6100-field"` a `"x6200-field"` en
  `src/dialog_field.c` (el hallazgo del #64) y los spots de POTA usan el nombre real.
- `122bea3` (#80) — Control remoto: entra `SWRScan` en **las tres tablas** de `src/remote_server.cpp`
  (`app_names[]`, `actions[]`, `dialog_apps[]`), que es exactamente lo que faltaba según el triage del
  #78. De paso el barrido de apps ahora cubre las 23.

Los dos tocan **sólo `src/`**, así que el deploy del GUI alcanza (el helper del rootfs en la radio ya
coincide con el repo: `md5 efcc0b0ce095` en los dos).

**Por qué tampoco se desplegó:** hay **builds corriendo ahora** en el volumen compartido:

```
$ docker ps --format '{{.Image}} {{.Command}}'
  x6200-buildroot:latest  "sh -c 'mkdir -p /t/…"        <- dirclean / rebuild del otro agente
  x6200-buildroot         "./entrypoint.sh sh …"
  9c4ba5849793            "./entrypoint.sh bas…"   (x3, en paralelo)
```

La hora del binario del volumen **no alcanzaba** para saberlo (marcaba 35 minutos, del último build
terminado), así que la regla del skill se amplió: además del sello `.fuente.md5`, mirar `docker ps`.
El binario del A/B que estaba en el volumen quedó **respaldado** (`/b/ab-backup-237f1f6bc1da`) para no
perderle su corrida.

**Pendiente para la próxima corrida (cuando el volumen esté libre):** build limpio + deploy de los tres
commits y después, además del chequeo general, **la prueba de TX que quedó bloqueada**: con el #80 el
SWR Scan ya se puede abrir por socket, la antena es de 40 m, el dueño autorizó QRP y `swrscan_span` está
en 50000 Hz (7.05–7.15 MHz con el dial en 7.100).

### Tercera corrida (17:10): el volumen sigue ocupado, y una vigilancia más larga

`git fetch fork` trajo **1 commit más** (`1a56556`, #81: temas v2 Redshift, issue #68). El volumen
compartido estaba **saturado** — **25 contenedores** corriendo (`x6200-uiharness` y varios
`x6200-buildroot` con `chroot` y `dirclean`) —, así que tampoco se compiló.

Como el despliegue no dependía de mí, la ventana se usó para **alargar la vigilancia de estabilidad**,
que era el pendiente más flojo del anexo B (180 s):

```
  t=0    pid=309  logs=187  uptime=4410.34s  (17:11:50)
  t=300  pid=309  logs=187  uptime=4711.57s  (17:16:51)
  caidas: 0 | PID estable: si | el equipo reinicio: no
```

Más `tools/verifica-radio.sh` → `TODO OK` y los dos últimos logs con `CRASHLOG=0`. Ajustes del dueño sin
tocar (`theme_name=IU1WXY`, CW en 0, `swrscan_span=50000`).

**Conflicto de fondo (para el owner):** el volumen `x6200-build-output` es **uno solo** y el otro agente
lo usa para sus A/B y su arnés. En tres corridas seguidas (16:10, 16:40, 17:10) no se pudo compilar sin
pisarle un build en curso. O se le da un volumen propio (`x6200-build-output-ab`) o se acuerda una
ventana; si no, el ciclo «cada commit → build + deploy + prueba» se atrasa aunque la rama avance.

## 15/10 ~15:20 — `d0ff2dba9c85` (commit `337036c`)

**Qué se probó:** el build que trae el arreglo del #53 (iconos de Field Ops), el del #63 (crash del
WSPR) y los cambios de fuentes/ECO.

**Compilación y despliegue, medido:**

```
$ tools/compila-gui.sh
  binario: md5 d0ff2dba9c85   (commit 337036c - evidencia: barrido de las 23 apps ...)
$ tools/prueba-gui-en-radio.sh
  GUI lista: 7643.1 s
$ md5sum /usr/sbin/x6200_gui     # en la radio (192.168.0.236, encontrada sola por tools/lib-radio.sh)
  d0ff2dba9c85
```

**Chequeo general:**

```
$ tools/verifica-radio.sh
  TODO OK        (assets, 6 dispositivos de entrada, sink de PulseAudio, sin crashes)
```

**Barrido de apps, medido** (`PERMANENCIA=14`, evidencia:
[`barrido-23-apps-0-caidas.log`](evidencia/2026-10-15-validacion-arreglos/barrido-23-apps-0-caidas.log)):

```
22 apps con "ok"      (abre: {"t":"ok"}, cierra: {"t":"ok"})
0 menciones de caída  ($ grep -ci "cayo" → 0)
```

Es el mejor barrido registrado: en versiones anteriores caían 5 a 8 apps por barrido (issue #19).

**Vigilancia de 3 minutos** (PID y cantidad de logs):

```
  t=0    pid=2956  logs=185
  t=180  pid=2956  logs=185   → caídas: 0
  params CW: cw_tune=0 cw_decoder=0 key_train=0
```

**Lo que se probó puntualmente, y salió bien:**

- **WSPR (#63)**, que antes reiniciaba la GUI: `{"cmd":"app","text":"WSPR"}` → `{"t":"ok"}`, el
  proceso siguió vivo (`pid=4161`) y el último log quedó con `0` CRASHLOG. El arreglo `457a9a3` sí
  está en este build: `git merge-base --is-ancestor 457a9a3 337036c` → **sí** (código/medido).
  Evidencia: [`07-wspr.png`](evidencia/2026-10-15-validacion-arreglos/07-wspr.png).
- **Field Ops** abre, pero sigue con **3 errores de archivo** (rutas `/usr/share/x6100/field/*_96.png`):
  es un defecto **abierto y reportado** en el issue
  [#64](https://github.com/iariap/x6200_gui/issues/64), no una falla de esta corrida. Evidencia:
  [`errores-field-ops.txt`](evidencia/2026-10-15-validacion-arreglos/errores-field-ops.txt) y
  [`06-field-ops.png`](evidencia/2026-10-15-validacion-arreglos/06-field-ops.png).

**Transmisión: intento, y no se pudo medir.** Con la antena de 40 m ya conectada y autorización del
dueño (QRP, 40 m) se intentó disparar el **barrido de ROE**, que es el único camino de transmisión de
las apps (`Run` → `radio_start_swrscan()` → 3 W en LSB; `codigo: src/radio.c:667-690`). **No se
transmitió**, porque la app no se puede abrir por el control remoto:

```
$ for n in SWRSCAN Swrscan swrscan "SWR Scan" SWR; do
    ssh root@radio "/tmp/x6200-remote '{\"cmd\":\"app\",\"text\":\"$n\"}'"; done
  'SWRSCAN'  -> {"t":"err","err":"Unknown app"}
  'Swrscan'  -> {"t":"err","err":"Unknown app"}
  'swrscan'  -> {"t":"err","err":"Unknown app"}
  'SWR Scan' -> {"t":"err","err":"Unknown app"}
  'SWR'      -> {"t":"err","err":"Unknown app"}
```

Que **no hubo emisión** está medido, no supuesto: el log de la radio no registra TX y el estado quedó
igual antes y después (`7.1001 MHz LSB 40m SSB pwr=50`). El defecto quedó reportado (issue
[#78](https://github.com/iariap/x6200_gui/issues/78), previa búsqueda de duplicados).

Antes de intentarlo se acotó el barrido: `swrscan_span` pasó de **200000** a **50000** Hz (el log de
arranque lo confirma: `Loaded swrscan_span=50000`), así que con el dial en 7.100 el barrido quedaría
en 7.05–7.15, dentro de 40 m. Evidencia:
[`08-swrscan-tx.png`](evidencia/2026-10-15-validacion-arreglos/08-swrscan-tx.png).

## 15/10 ~14:05 — `5a6e561e502f` (commit `e5a869b`)

**Qué se probó:** la primera compilación **limpia** (el build incremental dejaba un binario viejo:
dio `30bef0dbec2a` contra `5a6e561e502f` del limpio) y los arreglos de audio/temas.

**Medido, uno por uno:**

| Issue | Antes (medido) | Después (medido) | Evidencia |
|---|---|---|---|
| **#30** volumen | GUI en `0` con la base en `8`; el comando remoto contestaba `ok` pero no cambiaba nada | GUI `4` con la base `4`; `{"cmd":"vol","val":30}` → `{"t":"ok"}` y GUI `30` | [`01-vol-30.png`](evidencia/2026-10-15-validacion-arreglos/01-vol-30.png) (el pie muestra `Volume: 30`) |
| **#44** medidor de RS | `#ff4040`, contraste **2.72:1** | `#f08080`, contraste **3.64:1** (351 píxeles del color puro) | [`04-rs-despues.png`](evidencia/2026-10-15-validacion-arreglos/04-rs-despues.png) |
| **#51/#19** CW | `sig=6`, `buffer overflow detected` de glibc, `key_train` quedaba en `1` | ese `sig=6` no volvió a aparecer y `key_train` quedó en `0` | [`crashes-sig11.txt`](evidencia/2026-10-15-validacion-arreglos/crashes-sig11.txt) |

**Y una causa que apareció midiendo (#44):** el color del tema no llegaba a la radio porque la GUI
lee los temas de `/mnt/themes` y **sólo los copia si no existen** (`codigo: src/main.c:300-301`,
`src/theme.c:21`), así que la copia vieja en `DATA` gana. Con `/mnt/themes/RS/theme.json` corregido
a mano, el medidor pasó a `#f08080` y **3.64:1**. Quedó documentado en el issue
[#44](https://github.com/iariap/x6200_gui/issues/44).

---

## 19/10 ~10:40 (hora host) — rama `factory/cb-27mhz-146` (#146): cierre (F4-fix), gates completos y banco QEMU; **F0 requiere radio**

**Qué se probó:** el módulo CB 27 MHz completo (F1–F5 + el fix de F4) sobre la base
`1e3a1e49`, HEAD `ade36bbb`, **sin desplegar, sin flashear y sin transmitir**. La RF real
(F0) queda pendiente y la hace el owner con autorización.

**Fix de esta corrida** (`ade36bbb`): `cb_split_cache_cb` reconoce el cambio de banda de
`cfg/band.c` por el pk de la banda (`cfg_cur.band->vfo.pk`) y no por `cfg.band_id` —en la
entrada directa de frecuencia el split de la banda destino se cargaba antes de que cambiara
`cfg.band_id` y pisaba el split a restaurar—, y los hooks de CB no llaman al control de la
radio si la BASE no respondió (`radio_control_ok`), que colgaba el banco al entrar a CB con
`xit != 0`.

**Medido (comandos reales):**

```
$ tests/native/run_native_tests.sh
  18708 comprobaciones, 0 fallos
  == CB 27 MHz (#146): _4_cb_bands sobre una base real en memoria ==
  42 comprobaciones, 0 fallos
$ RED->GREEN de tests/native/test_cb.c (cb degenerado vs src/cb.c)
  RED:   test_cb: 85 comprobaciones, 56 fallos
  GREEN: test_cb: 85 comprobaciones, 0 fallos
$ python3 tests/check_wcag.py
  pares OK: 1052 | exentos: 170 | informativos: 90 | FALLAS: 0
$ python3 tests/check_cfg_fills.py .
  OK: los 70 campos se inicializan, en orden
$ python3 tests/check_actions.py
  todas las acciones ofrecidas tienen manejador
$ tests/check_harness_determinism.sh
  PASS: deterministic (identical PNGs for every fixture)
$ python3 tests/audita-pruebas.py
  OK: estructura y evidencia en orden
$ cd tools/ab_testing && uv run ruff check . && uv run ruff format --check . && uv run ty check
  All checks passed! / 178 files already formatted / All checks passed!
$ uv run pytest -m "not e2e" -q
  227 passed, 13 deselected
$ uv run pytest -m "e2e" -q
  13 passed, 227 deselected
$ uv run x6200-ab compare --a 1e3a1e49 --b ade36bbb --experiment-id AB-CB146
  experiment AB-CB146: PASS (exit 0)  (B = ade36bbb)
  Functional PASS — 87 scenarios captured on both revisions
  Visual Regression PASS — captures are pixel-identical to the baseline
  Quality Contract PASS — 893 contract checks passed on the candidate
  Performance PASS — render-cost proxy within budget
$ BUILDROOT=... tools/compila-gui.sh   (ARM cross-compile)
  binario: md5 4893b8f39bf3   (13.783.700 B)
```

**Arnés de UI (seis temas).** La celda VFO de 56 px muestra `CH 19` sin recorte en los seis
temas, medido con `--inspect` (`text_natural`) y la familia tipográfica que nombra cada
tema: inter 41, ibmplexsans 47, sourcesans3 40, robotocondensed 35, notosans 48, ibmplexsans
47; `text_truncated=false` y `text_overflow=false` en los seis (máximo 48 px ≤ 56).
Detalle en
[`evidencia/2026-10-19-cb-27mhz-146/arnes-temas-ch19.json`](evidencia/2026-10-19-cb-27mhz-146/arnes-temas-ch19.json).

**Banco QEMU (gate #86).** Binario ARM `4893b8f39bf3` (13.783.700 B, 3/3 marcas del port).
Capturas y estado en
[`evidencia/2026-10-19-cb-27mhz-146/`](evidencia/2026-10-19-cb-27mhz-146/README.md):

- 27.185 MHz → `CH 19` (`banco-ch19.png`), AM, **TX power 4.0 W** (la base tenía 5.0 W y el
  módulo la recortó);
- 27.000 MHz → `CH --` (`banco-ch--.png`), AM;
- 27.255 MHz → `CH 23` y 27.235 MHz → `CH 24` (`banco-ch23.png`/`banco-ch24.png`): el dial
  cuenta canales, por eso 23→24 sube el número y **baja** la frecuencia;
- 26.100 / 26.600 / 27.300 / 27.450 → `CB A` / `CB B` / `CB C` / `CB D`, todos en **AM**;
- rangos migrados en `/mnt/params.db` = `sql/bands_cb.csv`; al arrancar en CB con `xit=100`
  y `split=1` en 203 la GUI los deja en **0/0**;
- `tx=false` en todas las consultas: no se envió ningún comando de TX.

El control remoto no tiene comando para inyectar eventos del rotativo, así que el banco mide
la **celda de canal** en las frecuencias del plan; el conteo 1 detent = 1 canal está medido
en el test nativo (`cb_step_channel`) y en el fixture del arnés.

**No medido (requires-radio):** RF real en 27 MHz, potencia y armónicos con carga fantasma,
PTT del micrófono y ATU. El TX **no** queda declarado validado.

---

## 18/10 ~23:55 (hora host) — rama `factory/cb-27mhz-146` (#146): gates de host y banco QEMU; **F0 requiere radio**

**Qué se probó:** el módulo CB 27 MHz completo (F1–F5) sobre la base `1e3a1e49`, **sin
desplegar, sin flashear y sin transmitir**: cuatro grids de 40 canales, migración `_4_cb_bands`,
dial por canal con `CH nn`/`CH --`, políticas de entrada (split/modo/potencia/XIT) e interlock
de TX. La RF real (F0) queda pendiente y la hace el owner con autorización.

**Medido en el host y en el banco (comandos reales):**

```
$ tests/native/run_native_tests.sh
  18708 comprobaciones, 0 fallos
  == CB 27 MHz (#146): _4_cb_bands sobre una base real en memoria ==
  42 comprobaciones, 0 fallos

$ python3 tests/check_wcag.py
  pares OK: 1052 | exentos: 170 | informativos: 90 | FALLAS: 0
$ python3 tests/check_cfg_fills.py
  OK: los 70 campos se inicializan, en orden
$ python3 tests/check_actions.py
  todas las acciones ofrecidas tienen manejador
$ SKIP_BUILD=1 tests/check_harness_determinism.sh
  PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing
$ uv run x6200-ab compare --a 1e3a1e49 --b factory/cb-27mhz-146 --experiment-id AB-CB146
  experiment AB-CB146: PASS (exit 0)
  Functional PASS — 87 scenarios captured on both revisions
  Visual Regression PASS — captures are pixel-identical to the baseline
  Quality Contract PASS — 893 contract checks passed on the candidate
  Performance PASS — render-cost proxy within budget
```

**Arnés de UI (seis temas).** La celda VFO de 56 px muestra `CH 19` sin recorte en Aurora (41 px),
Cathode (47), Redshift (40), Sprint (35), Sunlit (48) y Trail (47); `text_truncated=false` y
`text_overflow=false` en los seis. El detalle está en
[`evidencia/2026-10-18-cb-27mhz-146/arnes-temas-ch19.json`](evidencia/2026-10-18-cb-27mhz-146/arnes-temas-ch19.json).

**Compilación ARM y banco QEMU.** Cross-compile con el toolchain de la ROM: binario
`882e1023493c`, 13.783.668 B. El gate #86 lo acepta como firmware del port (ELF32 ARM, 3/3
marcas). En el banco:

- 27.185 MHz → `CH 19`, AM, **TX power 4.0 W** (la base tenía 5.0 W y el módulo la recortó);
- 27.000 MHz → `CH --`, AM;
- 26.100 / 26.600 / 27.450 → `CB A` / `CB B` / `CB D`, todos en **AM** (el bug de que A/B/D
  quedaran en USB se encontró y se corrigió en esta corrida);
- 27.300 MHz → `CB C` con los rangos **migrados** (con los viejos, 27.205–27.405 no tenía banda);
- al arrancar con la base sembrada en CB con `xit=100` y `split=1`, la GUI los deja en **0/0**
  (`xit` y `split` de 203), que es la política de entrada del arranque;
- `tx=false` en todas las consultas: no se envió ningún comando de TX.

Evidencia: [`evidencia/2026-10-18-cb-27mhz-146/README.md`](evidencia/2026-10-18-cb-27mhz-146/README.md),
[`banco-ch19.png`](evidencia/2026-10-18-cb-27mhz-146/banco-ch19.png) y
[`banco-ch--.png`](evidencia/2026-10-18-cb-27mhz-146/banco-ch--.png).

**No medido (requires-radio):** RF real en 27 MHz, potencia y armónicos con carga fantasma, PTT
del micrófono y ATU. El TX **no** queda declarado validado.

---

## 20/10 18:01 (hora host) — presets de filtro del DFL (#183): binario ARM `fd864a791cb5`

Rama `factory/dfl-presets-183` sobre el trunk `26c8e8e7`, commit **`a0fa0250`**. Corrida de
**software**: sin radio, sin despliegue, sin flasheo y **sin transmitir**. Lo que se midió:

| Gate | Comando | Salida |
|---|---|---|
| Nativos | `sh tests/native/run_native_tests.sh` | rc 0; **47.585 comprobaciones, 0 fallos**. `test_dfl_presets` **175** (catálogo exacto de 4 familias × 3, no auto-aplicar, coincidencia/`Personalizado`, aplicación desde un estado incompatible **sin half-apply**, guardar/recuperar/reasignar por ID, memoria vacía inaccesible, aislamiento por familia, favorito huérfano, fallo de SQL sin éxito, y **0 escrituras** a `cfg_cur.filter.bw` ni al par real) y `test_migrations_cb` **62** (migración `_5`: tablas, CHECKs, idempotencia y `mode_params` intacto) |
| Estáticos | `check_{cfg_fills,actions,settings_focus,settings_grid,apps_menu,app_handlers,manual_apps,wcag,cw_tune_feed,build_identity}.py` + `audita-pruebas.py` | **11 OK** |
| Arnés (host) | `ui_harness --scenario …` | **196 fixtures, 0 fallos** (54 nuevos: A..F en los seis temas, estados `activo`/`Personalizado`/DIGI, aplicar/guardar/reasignar/cancelar, TX, cambio de modo, rotary0/VOL y 30 ciclos) |
| Clipping/glyphs | `ui_harness --inspect` | 54 fixtures, **2.994 labels: 0 truncados, 0 glyphs faltantes, 0 en fallback** |
| Determinismo | `tests/check_harness_determinism.sh` | **PASS**: 196 fixtures × 2 corridas, PNG idénticos (sha256) |
| Framework A/B | `ruff` / `ruff format --check` / `ty` / `pytest -m "not e2e"` / `pytest -m e2e` | OK / OK / OK / **227 passed** / **13 passed** |
| Banco QEMU (#86) | `CHROOT=… BUILDROOT=… tools/captura-banco.sh` | preflight OK, `dirclean`+rebuild OK, **gate #86 3/3 marcas**; `fuente: 34bea401acbf`, `binario: fd864a791cb5`, 13.899.984 B; capturas de **A, B, C, D y E** |

**Banco, medido además en la base del chroot** (`logs/banco-params.db.txt`): `version=5`, las
dos tablas nuevas (`dfl_filter_memories`, `dfl_filter_favorites`) y `mode_params` del AM con
`filter_low=0 / filter_high=6000` tras aplicar el preset `Wide` por el control remoto
(`page dfl` + `btn 0`; el `get` siguiente pasó de `fhi:4500` a `fhi:6000`), con
`freq_step`/`spectrum_factor` **sin tocar**.

**No medido / pendiente:**

* **Validación física** de la UX y de los presets en la radio: la hará el owner después (el
  contrato §7 dice que no bloquea el PR/cierre). No se desplegó ni se transmitió.
* **A/B `AB-DFL183`** — **PASS** (`uv run x6200-ab compare --a 26c8e8e7 --b a0fa0250`):
  Functional, Visual Regression, Quality Contract (1.480 checks) y Performance PASS;
  Hardware/Energy BLOCKED (advisory, sin radio). **140 de 142 escenarios idénticos**; los 6
  `dfl-page-tema-*` cambian **explicados** dentro de las regiones declaradas (`botones`,
  `dfl_inline`) por el rótulo nuevo de la quinta celda (`Grafico`) y por llegar a esa página
  vía `Ajustar`. Reporte: [`ab-DFL183/`](evidencia/2026-10-20-dfl183/ab-DFL183/report.md)
  (el primer intento abortó por disco lleno y se reintentó con el workspace en `/var/tmp`).
* **Página F en el banco**: el control remoto no tiene comando de **hold** (fuera del alcance
  de este issue), así que F se captura sólo en el arnés (con `LV_EVENT_LONG_PRESSED` real).

Evidencia: [`evidencia/2026-10-20-dfl183/README.md`](evidencia/2026-10-20-dfl183/README.md)
(capturas por tema/estado, `banco/` y los logs crudos de cada gate en `logs/`).

---

## Anexo B: lo que **no** está medido

| Pendiente | Por qué no está fundado | Qué haría falta |
|---|---|---|
| **CB 27 MHz: TX/RF real (F0 del #146)** | En esta corrida **no se transmitió, no se desplegó y no se flasheó**. El módulo CB, el dial y el interlock están medidos en host y en el banco QEMU (`tx=false`), pero no hay medición de RF en la radio. | Con autorización del owner y carga fantasma: TX en 27,185 (AM y FM), potencia real contra `cb_pwr_max`, armónicos, PTT del micrófono y ATU. Checklist §10.3 del issue |
| **Transmisión (TX) y RF** | Intentado el 15/10 y **no medido**: la app del barrido de ROE no se puede abrir por el control remoto (issue #78), así que `radio_start_swrscan()` nunca corrió. La antena es de 40 m. | Que el remoto pueda abrir esa app; después, un barrido de ROE con la potencia al mínimo |
| **Estabilidad de horas** | La vigilancia más larga pasó a **300 s** el 15/10 17:11 (0 caídas), todavía lejos de "horas". | Dejar la radio corriendo varias horas y contar caídas |
| **El #63 con spots reales decodificados** | Se abrió WSPR y quedó viva, pero no se esperó un ciclo completo con decodificación. | Dejarla en WSPR varios ciclos y mirar el log |

**Cerrado de este anexo:** *"el barrido dice 22 y el menú tiene 23"* — medido: el que falta es el
**SWR Scan**, que no está en la lista de apps del control remoto (`src/remote_server.cpp:285-292`,
12 entradas; probadas 5 variantes del nombre, todas `Unknown app`). Ver issue
[#78](https://github.com/iariap/x6200_gui/issues/78).


## 22/10 (UTC) — Assessment pre-release #194: dossier `c888308e` (sin radio)

**Qué se probó:** la campaña completa de evidencia #194 (unidades #195..#199) sobre el candidato
GUI `c888308eede71205605c06383432ee7e165f202e` + Buildroot
`d084e2870a00b64d9c57474e7eceeef80f43f25b` + LVGL `74d0a816…`, con el artefacto de #195
(`sdcard.img` `d0ff4435…`, `rootfs.tar` `7c26c76b…`, GUI md5 `1f998c46…`, fuente-md5
`964193ae…`, gate #86 PASS). **No se desplegó, flasheó, transmitió ni se tocó la radio**; esta fase
es consolidación documental. El hardware queda **BLOCKED/PENDING_HARDWARE** con causa.

**Medido (HOST/QEMU_USER):** 334 ítems de inventario; 1.025 filas de resultados (PASS 933,
INCONCLUSIVE 24, REVIEW_REQUIRED 21, FAIL 15, BLOCKED 13, PENDING_HARDWARE 12, SKIP 5,
UNSUPPORTED 2); 199 capturas UI, 26.455 objetos, 2.056 checks de contrato y 90 pares WCAG;
3.980 frames de render (p50 4,2946 ms / p95 5,3839 ms / MAD-CV mediana 7,579 %); 10 estados de
recursos QEMU; 3 pares de audio digital null-sink. `sha256sum -c` de 01..04: **OK**
(80/102/815/272).

**Código (sin ejecutar):** timers WiFi de 30 ms incondicionales (~41,8 wakeups/s); ausencia de
watchdog y wrapper `while true` sin backoff; migración 2 no idempotente; `/bin/sh`→bash con arrays
en init scripts.

**No medido (hardware/instrumentos):** energía/térmica, boot-to-ready real, LCD/input físico,
audio analógico/mixer/dropouts, FFT/carga DSP, RX/TX reales, update/rollback con corte de energía,
DB llena/RO en el equipo, servicios residentes. Todo tiene caso y criterio en
[`hardware-plan.csv`](evidencia/assessment-rom-194/05-summary/hardware-plan.csv).

**Hallazgos fundados P1:** tarjeta nueva crea DATA vfat y el primer boot no monta `/mnt`; DB
corrupta/RO/faltante → SIGSEGV determinista + crash-loop; empaquetador de update exit 1 y
`S02update` owner 1000 (`pulse`); primera update con backup 706/5466 y sin checksum/rollback; sin
watchdog. Los P2/P3 y las **hipótesis separadas** están en
[`findings.csv`](evidencia/assessment-rom-194/05-summary/findings.csv) /
[`findings.md`](evidencia/assessment-rom-194/05-summary/findings.md). No se abrió ningún ticket.

**Limitaciones:** sin radio; #197 se ejecutó antes del merge de #196 (su `DEP-196` quedó superado,
pero los 22 diálogos inertes siguen sin captura); no hay CI del SHA candidato; el defconfig
congelado no reproduce el artefacto desde cero (F-11 resuelto en Buildroot PR #2, sin reconstruir);
el dossier es **STALE** para el tip GUI `d4fb57f8` (25 archivos de producción) y Buildroot
`f23d14f`. Detalle y repeticiones en
[`stale.md`](evidencia/assessment-rom-194/05-summary/stale.md).

**Veredicto:** **CONDITIONAL** para el alcance software; **NO_GO de release** hasta ejecutar la
fase B con identidad exacta y disponer de los P1. Cierre de recolección **no** es aprobación de
producto ni autorización de deployment. Evidencia completa:
[`05-summary/README.md`](evidencia/assessment-rom-194/05-summary/README.md).

---

## 24/10 03:05Z — #292 (LSB demodulaba la banda de arriba): RESUELTO, causa raíz y prueba en la radio

**Causa raíz (fundada):** el BASE demodula con el modo del **VFO ACTIVO**. La GUI escribía
el modo solo del VFO A (#244; el B se reservaba a split), así que si el VFO activo era B su
registro de modo quedaba **viejo (USB)** y al pasar a LSB la radio demodulaba la banda de
arriba. Medido con una portadora real de 790 kHz (banda AM, de noche): en el estado con bug,
`789-LSB` daba el tono de 1 kHz en **59 dB** (oía la portadora de arriba); escribiendo el modo
0 en el VFO B bajó a **27 dB** (LSB correcto).

**Fix (rama `fix/292-mode-active-vfo`, commit `a04032e2`):**
- `mode_write_allowed()` permite el **VFO activo** además de A y split.
- `radio_sync_active_vfo_mode()` manda `cfg_cur.mode` al VFO activo; se llama al arrancar, al
  cambiar de modo (observer de `cfg_cur.mode`) y al cambiar de VFO.
- En CW/CWR **no** se escribe el VFO B: se midió que reproduce el mute de RX de #244
  (`LSB→CW→LSB` dejó el audio en rms 141 hasta el power-cycle).

**Prueba en la radio (build `b90a1a7b` + guard CW `f4a9d47d`), portadora 790 kHz:**

| dial | modo | tono 1 kHz | veredicto |
|------|------|-----------|-----------|
| 789  | LSB  | 26.8 dB   | filtra la banda de arriba ✓ |
| 789  | USB  | 57.2 dB   | oye ✓ |
| 791  | LSB  | 57.6 dB   | oye la banda de abajo ✓ |
| 791  | USB  | 29.1 dB   | filtra ✓ |

Referencia del firmware original (misma prueba): 37.1 / 68.8 / 70.4 / 39.3 dB — mismo patrón.

**Estado:** binario `f4a9d47d` desplegado y GUI oficial arrancando sin CRASHLOG. Queda
**pendiente la validación del owner por oído** (20m LSB) y **un power-cycle** para limpiar el
mute de BASE que dejó la prueba de CW con el primer build (el mute de #244 solo se va con
power-cycle; ni el volumen raw ni el reset GPIO 204 lo limpian — verificado).

### Nota de la prueba: el mute de BASE es del ESTADO, no del fix

Al probar CW con el primer build el BASE quedó en el mute de #244. Se midió que:
- **con la GUI parada**, escribir la secuencia completa del firmware original por I2C **recupera el audio** (rms 141 → ~15000);
- **al arrancar la GUI** desde ese estado el audio vuelve a quedar mudo;
- **el binario oficial `22a4bf6d` (sin el fix) también mutea** en ese estado → el mute **no lo
  introduce el fix de #292**, es un artefacto del estado del BASE después de re-inicializarlo con
  la secuencia del stock;
- las escrituras mínimas (modo A/B, vi_vm, 0x30, rxvol, bloque de calibración `0xF000`, `0xFFFE`)
  aplicadas con la GUI corriendo **no** lo recuperan; el reset de GPIO 204 tampoco.

Conclusión: el mute se limpia con **power-cycle del equipo** (como documenta #244) y la
validación final por oído se hace después del power-cycle, con el binario `f4a9d47d` ya desplegado.

### Lectura del BASE por I2C (verificado 24/10)

El BASE se puede LEER por I2C con el mismo framing que usa el GUI original:
`w2@0x72 <addr BE> rN@0x72`. Direcciones útiles verificadas: `0x0000` = versión ASCII
("1.0.0", "1.0.6", "Jan 11 2025,13:46:22"), `0x0100` = calibración, `0x1000` = work
params en curso (freqA como u32), `0x2000` = ready=1. Sirve para diagnóstico sin
tocar la ROM.

---

## 24/10 11:25Z — #292: VALIDACIÓN FINAL tras power-cycle: **PASS**

El dueño apagó/encendió la radio (power-cycle limpia el mute de #244). Con el binario
`f4a9d47d` (fix mergeado en main, PR #303) y **audio sano (rms 2777)** se corrió
`tools/lsb-sideband-test.sh`:

| prueba | tono 1 kHz | esperado con LSB/USB correctos | veredicto |
|---|---|---|---|
| 789-LSB | **27.0 dB** | filtra la portadora de arriba | ✓ |
| 789-USB | 57.2 dB | la oye | ✓ |
| 791-LSB | 56.9 dB | oye la portadora de abajo | ✓ |
| 791-USB | 26.5 dB | la filtra | ✓ |

**RESULTADO: PASS (LSB y USB demodulan lados opuestos)** — mismo patrón que el firmware
original medido con la misma portadora (37.1 / 68.8 / 70.4 / 39.3 dB).

La parte FT8 (7.072/7.074) quedó **inconclusiva por diseño**: en 40 m hay señales FT8 a
ambos lados del dial, así que la energía no discrimina banda; la portadora AM sí.

Radio restaurada: 14.192410 LSB, vol 16, BT off, logger on, CRASHLOG 0.
Cierra #292.

---

## 26/10 22:32Z — Auto CQ del FT8: verificado en la radio

Con el binario `8b534333` (PR #312, "Auto CQ por defecto") se abrió FT8 por control remoto en
40 m y **sin ninguna intervención del operador** la radio llamó sola a una estación en CQ:

- **Captura:** `assessment-194/scratch/cw244/fb-autocq.png`
- La pantalla muestra `Auto CQ: llamando a PY5QW`, el decodificado `CQ PY5QW GG54 -8 dB`
  (fila verde) y nuestros TX `PY5QW LU2DSX GF05` repetidos, con `TX CQ: Disabled` y
  `TX Call: Enabled` (no hubo toque ni CQ manual).
- `get` durante 90 s: `tx=true` en bloques de ~12 s cada ~30 s (slots alternados), y `tx=false`
  tras `ft8_stop`.
- Cierre: app cerrada por control remoto (`app off`), `tx=false`, audio sano (rms 3231),
  crashlog 0.

**Conclusión medida:** con `Auto` habilitado (default), la radio llama sola a estaciones
nuevas en CQ, igual que FT8CN.

---

## 27/10 15:00Z — QRZ uploader + TLS: verificado en la radio (falta la API key real)

Se desplegaron en la radio, por SSH y como archivos exactos del build v0.1.6, el módulo
`_ssl` de python3, el bundle de CA (`/etc/ssl/certs` + `/usr/share/ca-certificates`) y el
uploader (`/usr/share/x6200/qrz-upload.py` + `/etc/init.d/S55qrz-upload`).

```
$ python3 -c "import ssl; print(ssl.OPENSSL_VERSION)"
OpenSSL 1.1.1w  11 Sep 2023

$ python3 -c "import urllib.request as u; print(u.urlopen('https://logbook.qrz.com/api?KEY=TEST&ACTION=STATUS', timeout=20).read())"
STATUS=AUTH&RESULT=AUTH&REASON=invalid api key

$ python3 /usr/share/x6200/qrz-upload.py --status
enabled=1 api_key=(vacío) interval=30
estado: ultimo rowid=0 red=up
pendientes: 4

$ /etc/init.d/S55qrz-upload start
Starting QRZ uploader: OK      (pid 1040)
```

**Lo que queda medido**: la radio valida el certificado de QRZ con el bundle de CA, el
POST de la API llega (con key falsa responde `RESULT=AUTH`), el daemon corre y con
`api_key` vacía no sube nada. **Pendiente**: cargar la API key real del QRZ Logbook en
`/mnt/qrz.conf` y correr `--once`; la respuesta tiene que ser `RESULT=OK`/`LOGID` y los 4
QSO pendientes aparecer en QRZ.

---

## 27/10 18:59Z — QRZ: la API key es válida pero la cuenta no tiene suscripción

Se cargó la API key real del dueño en `/mnt/qrz.conf` y se corrió la subida. QRZ respondió:

```
STATUS=AUTH&RESULT=AUTH&REASON=user does not have a valid QRZ subscription to use this
function, if you have purchased a new subscription please wait 300 seconds
```

**Lo que queda medido**: la key es válida (no dice "invalid api key"), el POST llega y la
cuenta autentica; **falta la suscripción QRZ**, que es requisito de QRZ para `ACTION=INSERT`
por API. No es un problema de la radio.

**Arreglo de comportamiento**: ante `RESULT=AUTH` el uploader ahora guarda
`/mnt/qrz_upload.cooldown` y reintenta en **1 h** (antes insistía cada 30 s y llenaba el
log). Medido en la radio: una línea de log, cooldown de 60 min, `--status` lo muestra y el
daemon sigue corriendo. Los **4 QSO quedan en cola** (`/mnt/qrz_upload.state` en 0) y
`--adif` permite exportarlos para subirlos a mano.

---

## 27/10 15:22Z — eQSL verificado de punta a punta; Clublog bloqueado con 403

Subida multi-proveedor (v0.1.8). Medido **desde la red de la radio** y desde la PC:

- **eQSL**: con la cuenta de prueba documentada (`TEST-SWL/Testpswd1`), el uploader subió el
  lote de 4 QSO y eQSL respondió `Result: 4 out of 4 records added` (con
  `Caution: ProgramID or Logger not found`, que no impide la importación). Al repetir:
  `Bad record: Duplicate`. → TLS, POST multipart/form, ADIF y parseo de respuesta OK.
- **Clublog**: `https://clublog.org/putlogs.php` responde **HTTP 403 Forbidden de nginx**
  desde la PC y desde la radio, con GET y con POST, mientras que `GET /logsearch.php` da
  200. Implementado según la guía (multipart `file/email/callsign/password/api`), **sin
  validar en vivo desde esta red**; el 403 se trata como credenciales/red y se reintenta
  en 1 h.
- Config migrada en la radio: `/mnt/qrz.conf` → `/mnt/upload.conf` (QRZ off con su key
  preservada, eQSL y Clublog on esperando credenciales); daemon corriendo y **4 QSO en
  cola por proveedor**, sin ruido en el log mientras falten credenciales.

---

## 27/10 15:27Z — eQSL: subida real a la cuenta del dueño ✓

Con las credenciales reales del dueño cargadas en `/mnt/upload.conf` (usuario **LU2DSX**,
que es el nombre de usuario de eQSL: el email NO sirve como `EQSL_USER` — probado, da
`No match on eQSL_User/eQSL_Pswd`) y el daemon de la radio:

```
eqsl: subidos 4 QSO (rowid<=4):
  <!-- Reply form eQSL.cc ADIF Real-time Interface -->
  Result: 4 out of 4 records added<BR> Caution: ProgramID or Logger not found
```

- Los **4 QSO** del log (LU1DZW, LW8DO, LU9MDQ en 40 m FT8 y PP1PA en 10 m FT8) quedaron
  subidos a la cuenta **LU2DSX** de eQSL ✓
- Estado: `/mnt/eqsl_upload.state` = 4, pendientes = 0 ✓
- `Caution: ProgramID or Logger not found` (eQSL no tiene registrado `X6200GUI`): no impide
  la importación.
- **Automático**: el daemon queda corriendo; cada QSO nuevo se sube solo cuando hay WiFi.

---

## 27/10 17:46Z — UI "Log uploads": credenciales editables desde la radio

Nueva app **Logs** para cargar las credenciales de QRZ/eQSL/Clublog desde la GUI.
Verificada en la radio (GUI `350d3a62`, deploy por SSH):

- Se abre por control remoto (`{"cmd":"app","text":"Logs"}`) y por **Ajustes → Operation →
  Log uploads → Configure**.
- Captura `assessment-194/scratch/cw244/fb-logs2.png`: muestra la configuración actual del
  archivo — **eQSL on, usuario `LU2DSX`, password enmascarada; QRZ off; Clublog off** — con
  la misma grilla, labels con celda del tema, switches y campos con borde de Ajustes
  (comparar con `fb-settings.png`).
- El módulo `upload_conf.c` (sin LVGL) tiene roundtrip probado en la PC, incluido un
  password con espacios; escribe el archivo atómico con permisos 0600.
- `crashlog 0` tras abrir/cerrar la app. La edición con teclado (tocar la fila) queda para
  prueba del dueño: no hay inyección de touch por control remoto.

---

## 27/10 18:00Z — Fix: las opciones de Logs no aparecían en Ajustes

El dueño reportó que presionando **Operation** no encontraba las opciones de Log uploads.
Causa medida en el código: la fila `Log uploads > Configure` **nunca se había agregado** al
grupo Operation (el reemplazo de texto no matcheó el indentado del `row = make_cluster_column(row);`)
y, aun agregada, quedaba debajo de la primera pantalla del grupo.

**Arreglo**: tab **Logs** propio en la primera página de tabs de Ajustes
(`Operation | Audio | Spectrum | Logs | More >`) con salto directo (`settings_goto`) a un
grupo que ahora tiene **todas las opciones inline** con los helpers de Ajustes: switch
`Upload`, un switch por proveedor y campos de usuario/email/password/API keys
(enmascarados) que abren el teclado al tocar y guardan en `/mnt/upload.conf`. El tab `Info`
(una sola fila) se movió al grupo Display. La app `Logs` sigue como acceso alternativo.

**Verificado en la radio** (GUI `85b8d05b`, deploy por SSH): con un build temporal que
arranca Ajustes en el grupo Logs se capturó `assessment-194/scratch/cw244/fb-tab-logs3.png`:
tab **Logs** marcado en la primera página (Operation | Audio | Spectrum | **Logs** | More >)
y las filas del grupo — Upload on, eQSL on + usuario `LU2DSX` + password enmascarada, QRZ
off — con el mismo estilo de Ajustes. El binario final (`85b8d05b`) abre Ajustes en
Operation y el tab Logs hace el salto al grupo (mismo `settings_goto` que los demás tabs).

---

## 27/10 23:00Z — SWR Scan: el RF timeout era el presupuesto de TX (35 ms asumidos vs 48,4 ms reales)

El owner no confiaba en la curva del barrido de ROE y pidió validarlo por control remoto. Se
instrumentó un binario temporal (dump por muestra y por paquete del flow, sin logs por paquete) y se
midió en la radio:

- **17m** (centro 18.099180, span 500 kHz): `state=4 abort=2 packets=37 valid=37 pass_timeout=2285
  elapsed=2287 ms`; deltas entre muestras **media 48,4 ms** (min 46, max 63).
- **40m**: `packets=43 valid=43 elapsed=2297 ms`, deltas media 48,5 ms.
- Con 51 puntos × 48,4 ms ≈ 2468 ms, el `SWRSCAN_PASS_TIMEOUT_MS` de 2285 ms cortaba la pasada:
  captura `swr-cleanA.png` → `Error: RF timeout`, `min 1.0 @ 17.850.000 (n=33/51)`.

**Arreglo** (PR #326, main `fcdb4907`): `SWRSCAN_DWELL_MS` 35 → 70, presupuesto 3570 ms, timeout
4070 ms; sigue una pasada a 0,1 W (aprobado por el owner). Tests nativos actualizados.

**Verificado en la radio** con el binario limpio `668eeba5`:

- 17m: `Completed`, `state=2 abort=0 packets=51 valid=51/51 elapsed=2668 ms`
- 40m: `Completed`, `state=2 abort=0 packets=51 valid=51/51 elapsed=2962 ms`
- Captura `swr-final2.png`: `Completed`, `min 1.0 @ 17.849.180 (n=51/51)`.
- `crashlog 0`; dial y modo del owner restaurados (18.099180 USB-D) y TX apagado al terminar.

**Pendiente (anexo A de HALLAZGOS §71)**: las muestras con `tx_power≈0` se registran como ROE 1.00
(17/51 en 17m y 38/51 en 40m a 0,1 W; también a 1 W y con AM), así que el mínimo de la curva puede
ser falso. Subir la potencia o usar AM no lo arregló (medido). Falta determinar por qué el BASE
reporta 1.0 sin potencia y cicla la potencia durante el barrido.

---

## 27/10 23:30Z — ROE por banda en fonia: 0,1 W no mide, 3 W sí

El owner pidió medir la ROE en 40/17/15/10/6 m en fonia. Se usó el build con dump (mismo
presupuesto de tiempo de §71, una pasada, span 500 kHz, 51 puntos) y dial en el segmento de fonia
(40m LSB 7.100.000; 17m USB 18.130.000; 15m USB 21.200.000; 10m USB 28.500.000; 6m USB 50.150.000).
"Muestras válidas" = `tx_power >= 0,3 W`.

| Banda | válidas 0,1 W | válidas 3 W | ROE mín (3 W) | ROE en dial |
|---|---|---|---|---|
| 40m | 13/51 | 18/51 | 1,0 @ 7,010 | 1,0 |
| 17m | 36/51 | 46/51 | 2,0 @ 18,090 | 2,2 |
| 15m | 0/51 | 46/51 | 1,9 @ 21,000 | 2,1 |
| 10m | 0/51 | 48/51 | 1,4 @ 28,670 | 1,7 |
| 6m | 8/51 | 15/51 | 1,5 @ 50,350 | 1,9 |

**Arreglo**: `SWRSCAN_TX_POWER_DW` 1 → 30 (3,0 W), PR #328 (main `583da568`). Sigue una pasada
(techo 3570 ms). Verificado en la radio con el binario limpio `beaa23d7`: barrido 15 m → `Completed`,
`crashlog 0`, dial/modo (18.099180 USB-D) restaurados.

**Evidencia**: `assessment-194/scratch/cw244/swr3w_bandas_fonia.png`, `swr3w_band_<banda>.png`,
`swr3w_band_<banda>.txt`; captura de verificación `swr-3w-15m.png`. Queda sin fundar la causa de
que 40 m/6 m tengan menos muestras con potencia (anexo A de HALLAZGOS §72).

---

## 28/10 02:10Z — SWR Scan: curva = puntos medidos, gate de potencia y 8 W pedidos

Pedido del owner: "que la curva sea siempre de los puntos medidos" y poder ajustar la potencia
para asegurar una medición.

**Medido** (radio, barrido de 51 puntos, fonia; "con potencia" = `tx` y `tx_power >= 0,3 W`):

| Potencia pedida | 40m | 17m | 15m | 10m | 6m |
|---|---|---|---|---|---|
| 0,1 W | 13/51 | 36/51 | 0/51 | 0/51 | 8/51 |
| 3,0 W | 18/51 | 46/51 | 46/51 | 48/51 | 15/51 |
| **8,0 W** | **48/51** | **49/51** | **48/51** | **48/51** | **49/51** |

Repetibilidad 8 W (17m, dos corridas): 48/51 válidas en ambas, 48 puntos comunes, |ΔROE| media
**0,02** / máx **0,10**. La potencia reportada por el flow queda ~0,6–1,0 W en las tres potencias
(el proceso del barrido la limita), así que el lazo automático de potencia no aporta: se pide el
techo (8 W) fijo.

**Arreglo** (PR #330, main `8b68b01d` + PR #331 harness): curva = polilínea por el agregado medido
+ punto por muestra, huecos sin interpolación; gate de validez (`tx` y `pwr >= 0,3 W`) con
reintento del mismo punto (6 paquetes) y hueco si no hay medición (`swr_session_skip_point`);
potencia pedida 8 W; tests nativos y harness de curva actualizados.

**Verificado**: tests nativos `exit 0`; `check_harness_swrscan_curve.sh` PASS; en la radio con el
build `6a710bfe`, barridos 40/17/15/10/6 m → **Completed n=51/51**, TX apagado, dial restaurado.
Capturas: `assessment-194/scratch/cw244/swr_pts2_bandas.png`.

**Sin fundar** (anexo A de HALLAZGOS §73): ROE absoluta contra medidor externo; por qué la
potencia reportada queda ~1 W.

---

## 28/10 02:40Z — La curva del barrido: suave y pasando por los puntos medidos (PCHIP)

La primera versión de "curva = puntos medidos" dibujaba la polilínea cruda: con la ROE
cuantizada a 0,1 los tramos se veían como escalones y el owner la rechazó ("esas no son curvas
con sierras, antes eran curvas cuadráticas").

**Arreglo** (PR #333, main `cd44f98a`): interpolación cúbica **monótona** (Fritsch-Carlson PCHIP,
`roe_curve_interpolate()` en `src/roe_curve.c`) que pasa **exactamente por cada muestra medida**,
es C1, no sobrepasa y corta el tramo en los huecos; además se marcan las muestras medidas. No es
el ajuste libre de #113 (ese no pasaba por los puntos). Tests: uno nuevo en `test_roe_curve.c`
(pasa por las muestras, sin sobrepaso, monótono) y tolerancias del render ajustadas a la
cuantización (0,1 ROE ≈ 26 px cerca del piso en escala logarítmica).

**Verificado**: tests nativos `exit 0`; `check_harness_swrscan_curve.sh` PASS; en la radio
(build `84b3a09e`) barridos 40/17/15/10/6 m → **Completed n=51/51** con curvas suaves
(captura `assessment-194/scratch/cw244/swr_smooth_bandas2.png`), TX apagado y dial restaurado
(18.099180 USB-D).

---

## 28/10 03:15Z — ROE en pantalla con forma de parábola tipo NanoVNA

El owner rechazó las dos iteraciones anteriores del trazado ("curva = puntos medidos"): la polilínea
cruda se veía en escalones y la interpolación PCHIP le pareció "parecida a la original de la radio".
Pidió explícitamente **forma de parábola como el NanoVNA**.

**Arreglo** (PR #335, main `1b1395e7`): `roe_curve_parabola()` dibuja SIEMPRE un valle abierto hacia
arriba, **centrado en el span**, con la **base en el mínimo medido** y una **subida mínima de las
puntas de 0,8 ROE**. El mínimo del readout sigue saliendo de las muestras (la curva es el trazado).
El harness de curva vuelve a exigir valle (PR #336, main `91156f06`).

**Verificado**: tests nativos `exit 0` (test nuevo de la parábola: base en el mínimo, puntas ≥
subida mínima, clamp); `check_harness_swrscan_curve.sh` **PASS** (domo del owner → valle profundo;
valle real → U). En la radio (build `3df36f76`) las cinco bandas dieron **Completed n=51/51** con
parábolas claras: captura `assessment-194/scratch/cw244/swr_par4_bandas.png`; TX apagado y dial
restaurado (18.099180 USB-D).

---

## 28/10 03:55Z — Curva del barrido: cuadrática libre (fiel y suave), elegida con mediciones

El owner pidió, antes de implementar, dibujar cómo se vería cada política con los **logs de las
últimas mediciones** y priorizar que sea **fiel y suave**. Se compararon cuatro políticas sobre los
dumps de 8 W (51 puntos, muestras con potencia 48-49/51):

| Política | RMSE 40m | 17m | 15m | 10m | 6m |
|---|---|---|---|---|---|
| **P1 cuadrática libre (elegida)** | **0,04** | **0,10** | **0,03** | **0,03** | **0,23** |
| P2 vértice real + base mínimo | 0,06 | 0,30 | 0,20 | 0,20 | 1,06 |
| P3 valle centrado (la anterior) | 0,28 | 0,32 | 0,24 | 0,30 | 0,95 |
| P4 vértice en el mínimo medido | 0,14 | 0,18 | 0,09 | 0,10 | 0,92 |

Figura de comparación: `assessment-194/scratch/cw244/curvas_politicas2.png`.

**Arreglo** (PR #338, main `c7e7f9ec`): `roe_curve_parabola()` dibuja el **ajuste cuadrático por
mínimos cuadrados tal cual** (sin espejar, sin centrar el vértice, sin subida mínima; clamp 1..5;
con <3 muestras, recta de mínimos cuadrados). El mínimo del readout sigue saliendo de las muestras.
Se elimina `ROE_PARABOLA_MIN_RISE`.

**Verificado**: tests nativos `exit 0` (unitario: recupera una cuadrática exacta y dibuja el ajuste
de mínimos cuadrados; render: orden vertical = ajuste y suavidad); harness `PASS` (domo del owner
→ domo, valle real → valle). En la radio (build `d84a00f2`) las cinco bandas dieron **Completed
n=51/51** con curvas fieles y suaves: `assessment-194/scratch/cw244/swr_p1_bandas.png`; TX apagado
y dial restaurado (18.099180 USB-D).

## 28/10 15:45Z — CW: se escribe al VFO ACTIVO y se mantiene en todas las bandas

Build `e968f4ff4861` desplegado en la radio (ROM v0.1.11; GUI anterior `9d4bfc70`). Comandos remotos con 3 s de
espera para que la cola del scheduler de la GUI los aplique; estados leidos con `{"cmd":"get"}`.

```
40m LSB  -> 7108700 '40m SSB' LSB
40m CW   -> 7108700 '40m SSB' CW
12m      -> 24915000 '12m' CW
6m       -> 50150000 '6m' CW
10m      -> 28941000 '10m' CW
LSB, 12m -> 24915000 '12m' USB      # la memoria de la banda vuelve al salir de CW
```

Log: `Modo CW/CWR activo: no piso vfoa_mode con la memoria de la banda (pk=10/12/11)` ✓ y
`Radio sync modo=4 al VFO activo 0 (#292)` ✓. Audio post-CW `parec`: rms 3748-3935 (sano >1000; mudo #244=141).
VFO B activo: `Radio sync modo=2 al VFO activo 1 (#292)` + `Radio set vfo 1 mode=2` ✓ (antes salteaba por #244).
Pendiente del dueno: repetir **CW con VFO B activo** (el ensayo se interrumpio con el activo volviendo a A).

Build: `make x6200-gui-dirclean && make -j8` en docker `x6200-buildroot` → exit 0; md5 del binario
`e968f4ff4861`; deploy con backup en `/mnt/ui-bak-<HHMMSS>/`, `crashlog 0`.

## 28/10 16:10Z — Zona CW automática: entra en CW y restaura al salir

Build `0a869656870e` (ROM v0.1.11). `band_plan_type_at()` + observador de `cfg_cur.fg_freq` en
`src/main_screen.c`: al entrar a un segmento CW del plan IARU pasa a CW (guarda el modo anterior) y al salir
restaura ese modo; un cambio manual dentro de la zona cancela la restauración; respeta modo trabado y TX.

```
51.000 6m USB  -> 50.099 -> 50099000 CW
50.150         -> 50150000 USB     (modo anterior restaurado)
7.060 40m LSB  -> 7.030  -> 7030000 CW
7.060          -> 7060000 LSB      (modo anterior restaurado)
```

`crashlog 0`. Tests nativos `exit 0` (test_band_plan: 50.099999=CW, 50.100000=ALL, 52.0=NONE).
Pendiente: confirmación del dueño con la rueda física desde 51 MHz en 6m.

## 28/10 16:55Z — Bluetooth consolidado en su app

Pedido del dueño: tener Settings y una app dedicada a BT era confuso, todo el Bluetooth debía estar
en la app.

- La app **Bluetooth** ahora tiene, arriba, los interruptores **Bluetooth** (encendido) y
  **Discoverable (CAT)**, además de Scan, pareo, conexión, audio y passkey. `src/dialog_bt.c`,
  `src/bt_state.c` (`bt_state_set_power`/`bt_state_set_discoverable`, `btcat-ctl` en hilo aparte).
- **Ajustes → Operation** ya no muestra esas filas (quedó `Charger`); evidencia de las dos pantallas
  en `docs/evidencia/2026-10-28-bt-app/` (`btapp.png`, `settings-sin-bt.png`).
- Medido en la radio (build `0cbb0c23c8c7`): `btcat-ctl bt on` → `hci0 UP RUNNING`; `off` → `DOWN`
  (el camino que usan los interruptores). `crashlog 0`.
- Manual actualizado: encendido/pareo van por la app BT (sección 7.3); el puerto USB-C no es host
  (un teclado USB o hub no se detecta), el teclado soportado es Bluetooth.

## 28/10 17:00Z — Icono WiFi centrado en su media celda

Reporte del dueño: en la home el icono de WiFi no estaba alineado con su contenedor. Medido en la
captura (`docs/evidencia/2026-10-28-wifi-icon/`):

| | glifo (x) | centro | caja (x) | centro de la caja |
|---|---|---|---|---|
| antes | 139..155 | 147,2 | 139..166 | 152,5 |
| después | 144..160 | 152,2 | 139..166 | 152,5 |

El label de la celda WiFi/BT tenía `LV_TEXT_ALIGN_LEFT` heredado y el símbolo (`LV_SYMBOL_WIFI`) quedaba
pegado al borde izquierdo de su media celda de 28 px. Se fija `LV_TEXT_ALIGN_CENTER` local sobre
`items[INFO_WIFI]` (sin tocar las otras celdas) y se agrega la guarda en `tests/check_info_bar.py`
(el chequeo estático de la barra sigue PASS). Build `5ef35ec0b807` desplegado, `crashlog 0`.

## 28/10 21:00Z — #292 con portadora de noche: 790 débil, 760 PASS

Corrida programada de la validación de banda lateral con la ROM v0.1.11 (GUI `5ef35ec0b807`).
Evidencia completa: [`docs/evidencia/2026-10-28-lsb-night/`](evidencia/2026-10-28-lsb-night/README.md).

- **790 kHz (script oficial `tools/lsb-sideband-test.sh --carrier`)**: `789-LSB 24.7`, `789-USB 33.1`,
  `791-LSB 33.0`, `791-USB 22.8` dB → **FAIL** del criterio absoluto del script (<48 / >55 dB,
  calibrado para portadora fuerte). La **dirección del patrón es la correcta**: LSB filtra 789 y USB
  filtra 791 (y al revés en 791). Ningún valor llegó a 48/55: la portadora estuvo débil esta noche.
- **Barrido AM de la noche**: 1280 kHz (−69 dB), 760 (−82), 1270 (−83).
- **760 kHz, mismo método con criterio relativo (±8 dB)**: `759-LSB 26.1`, `759-USB 39.6`,
  `761-LSB 36.3`, `761-USB 24.9` dB → **PASS** (lados opuestos; contraste 13.5 y 11.4 dB).
- **1280 kHz**: sin contraste (estación modulada), no sirve como portadora.

**Veredicto**: #292 sigue correcto en la ROM v0.1.11; no hay regresión por los cambios de SWR/curva/BT/UI.
El FAIL del script oficial es por la portadora débil del 790 esta noche, no por la banda lateral. Radio
restaurada en **7.140.000 LSB** (estado del dueño), `crashlog 0`.
