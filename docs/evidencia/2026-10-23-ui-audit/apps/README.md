# UI AUDIT C — Apps: evidencia visual real en QEMU (issue #272, épica #269)

Auditoría de **recolección de evidencia**: inventario de las apps de la ROM con
**capturas reales ARM/QEMU_USER 800×480** y clasificación explícita
`REAL`/`PARTIAL`/`SYNTHETIC`/`UNSUPPORTED` por app. **No** se modifica código, temas,
tests, settings, radio, RF ni firmware. No se propone rediseño.

- **SHA base (congelado)**: `d0ff5cd196b43ba0190d4ccad03f559a2954daf6` — es también
  `origin/main` y el HEAD del worktree al momento de la corrida (`Merge pull request
  #268 from iariap/feat/ui-258-259`).
- **Rama/worktree**: rama `factory/ui-audit-apps`, worktree
  `state/worktrees/x6200/ui-audit-apps`. El issue pedía la rama
  `audit/ui-apps-20261023` y el worktree `../x6200-ui-audit-apps`; se usó el nombre
  que fijó el orquestador (mismo contenido, **sin push**).
- **Fecha de la corrida**: 2026-10-23.
- **Evidencia**: `docs/evidencia/2026-10-23-ui-audit/apps/`.

## 1. Resultado en una línea

El binario ARM real de las **25 apps** de `src/remote_server.cpp::app_names[]`
(sin `off`) se abrió por su socket de control en un chroot privado, con framebuffer
falso; se capturaron **75 PNG reales 800×480** (25 apps × 3 rondas) y se probó la
vuelta a la home, la estabilidad de PID/`boot_id`/`logrev`/freq/mode/band y la
ausencia de crash/SIGSEGV/TX. **PASS 12, PARTIAL 6, FAIL 7, BLOCKED 0** (el peor
resultado de las 3 rondas manda; una corrida fallida no se descarta ni se repite).

> **El runner documentado no puede arrancar el banco con el binario congelado**
> (defecto **P1**, §5): `tests/qemu/run_apps_qemu.sh` sale con exit 1 sin producir ni
> una captura. Las 75 capturas se obtuvieron con una **copia derivada mínima** del
> runner que agrega el montaje de `/proc` y de una DATA real (la receta canónica del
> QA de `#213`). Todo lo demás (binario, `libfakefb`, `x6200-remote`, socket,
> `barrido_qemu.py`, contrato de 3 rondas) es idéntico al runner del repo.

## 2. Entorno y procedencia (todo reproducible)

| Item | Valor |
| --- | --- |
| Capa | `QEMU_USER` (userland armv7 por `binfmt_misc/qemu-arm`, kernel del host). Sin kernel/init real, sin radio, sin RF, sin SD del owner |
| SHA base | `d0ff5cd196b43ba0190d4ccad03f559a2954daf6` |
| `fuente_md5` (src + lvgl/src) | `4cdc9ae8a7bb756798e0a3c4016e5ab4` — coincide con `boot_identity: fuente: 4cdc9ae8…` del log de la GUI (prueba de que corrió la fuente congelada) |
| Binario medido | `/usr/sbin/x6200_gui`, md5 `609554a9729d3be17e3d5d290ac4261d` (13.432.744 B) |
| Build del binario | `rom-build/clone-buildroot` (r3): `gui_commit=d0ff5cd…`, `gui_md5_en_rootfs=609554a9729d3be17e3d5d290ac4261d` (verificado contra `last-rom.txt`) |
| Guard #86 | `tools/verifica-firmware-port.py` OK: ELF32 ARM, 3/3 marcas del port |
| Imagen de build | `x6200-buildroot` `sha256:b3e7601313d64284645a5224c60fb40f39ea58e0bfc68f42fd9bb0dad13a1189` |
| Contenedor | `x6200-c272-qemu` (privado; empezaba con `x6200-`, nunca `banco-apps`) |
| Volumen | `x6200-c272-build` (privado; sembrado desde el build del SHA congelado: `/b/target` + `/b/host`) |
| LAB | `/tmp/x6200-c272-qemu` (chroot, capturas, logs; fuera de los worktrees) |
| `QEMU_CPU` | `cortex-a7` (sin esto `qemu-arm-static` muere con SIGBUS) |
| Rondas / dwell | `RONDAS=3 DWELL=2` |
| Tema | **Aurora** (default de la ROM; la DATA sembrada no trae `theme_name`, así que `theme_init()` cae en `THEME_DEFAULT`, `src/theme.h:211`) |
| Cliente de control | `tools/remote/x6200-remote` compilado con el toolchain del volumen |

## 3. Comandos exactos

Preflight del runner del repo (no toca nada):

```bash
# sin chroot: 1 requisito faltante -> exit 77
LAB=/tmp/x6200-c272-nolyqemu CONT=x6200-c272-qemu VOL=x6200-c272-build \
  tests/qemu/run_apps_qemu.sh --check                     # -> SKIP, exit 77
# con el chroot privado ya armado -> exit 0
LAB=/tmp/x6200-c272-qemu CONT=x6200-c272-qemu VOL=x6200-c272-build \
  tests/qemu/run_apps_qemu.sh --check                     # -> "todo listo", exit 0
```

Corrida completa **del runner del repo** (falla, §5): `runner_original_exit1.txt`.

Barrido que produjo la evidencia (driver derivado, §4):

```bash
WT=$PWD
env WT="$WT" DATOS="$WT/tests/qemu/barrido_qemu.py" \
    LAB=/tmp/x6200-c272-qemu CONT=x6200-c272-qemu VOL=x6200-c272-build \
    RONDAS=3 DWELL=2 \
    sh docs/evidencia/2026-10-23-ui-audit/apps/tools/run_apps_qemu_audit.sh
# -> 75 PNG en $LAB/out/capturas, barrido.json / results.csv / index.txt
```

Comando por app y por ronda (el que registra `manifest.csv`):

```bash
docker exec x6200-c272-qemu chroot /t /tmp/x6200-remote '{"cmd":"app","text":"<app>"}'   # exit 0 (ninguno superó los 5 s)
docker exec x6200-c272-qemu chroot /t /tmp/x6200-remote '{"cmd":"app","text":"off"}'     # vuelve a la home
```

## 4. Driver derivado (por qué) y su diff

`tests/qemu/run_apps_qemu.sh` (issues #222/#216) es la herramienta pedida, y **se corrió**.
Su preflight pasa (exit 0) pero **la corrida completa sale exit 1**: el contenedor del
banco **no monta `/proc` ni una DATA real**, y el `x6200_gui` endurecido (`#211-#215`,
r3) aborta con `EXIT_PERSISTENCE_UNAVAILABLE` (**78**) cuando `/mnt` no es un montaje real
rw distinto de la raíz (`src/params/db.c::data_mount_ok()` lee `/proc/self/mountinfo`).
Evidencia: `logs/runner_original_exit1.txt` y `logs/out-banco.txt`.

La causa raíz está fuera del contrato del barrido (no es una app rota): es el **entorno
del banco**. Para no declarar BLOCKED teniendo QEMU real disponible, se usó una copia
mínima del propio runner que agrega exactamente lo que falta —el montaje real, la receta
canónica ya usada por el QA de `#213` (`run-arm.sh`/`arm-inner.sh`: `--cap-add SYS_ADMIN`
+ `mount --bind /proc /t/proc` + `mount -t tmpfs /t/mnt` con la plantilla de la ROM)—.
El diff completo está en `tools/run_apps_qemu_audit.diff` y son **2 cambios**:

1. `DATOS=${DATOS:-$AQUI/barrido_qemu.py}` (para poder correrlo desde otra carpeta).
2. En `arrancar()`: `--cap-add SYS_ADMIN` y, antes de la GUI,
   `mount --bind /proc /t/proc && mount -t tmpfs -o size=64M tmpfs /t/mnt &&
   cp /t/usr/share/x6200/params.default.db /t/mnt/params.db`.

Además, el `--check` del runner **exige que el chroot exista de antemano** aunque su
mensaje diga "se arma solo" desde `$VOL/target` (el chroot se sembró a mano desde el
target del volumen antes de correr). Todo esto queda como hallazgo P1 (§8).

## 5. Inventario de apps y cobertura (obligatorio de la épica)

`inventario_apps.csv` tiene la tabla completa (una fila por app + las 3 pantallas
`UNSUPPORTED`). Resumen de **cobertura por app** — el render sale **siempre** del
binario ARM real; `PARTIAL` cuando la *función* necesita un periférico/servicio que el
chroot no tiene:

| App | Fuente | Cobertura | Barrido (peor de 3) | Se verifica / no se verifica |
| --- | --- | --- | --- | --- |
| FT8 | dialog_ft8.c | **REAL** | FAIL | render + navegación OK; FAIL por contrato: recuerda su dial |
| FT4 | dialog_ft8.c | **REAL** | FAIL | alias de FT8 (protocolo FT4); mismo cambio de dial |
| JS8 | dialog_js8.c | **REAL** | FAIL | render OK; header muestra el label viejo "FT4 30m" |
| WSPR | dialog_wspr.c | **PARTIAL** | FAIL | render OK; no se valida la subida a WSPRnet (red) |
| SSTV | dialog_sstv.c | **REAL** | PASS | render OK; canvas idle blanco |
| WeFax | dialog_wefax.c | **REAL** | PASS | render OK; canvas idle blanco |
| NavTex | dialog_navtex.c | **REAL** | PASS | render OK; sin RX no hay mensajes |
| RTTY | rtty.c | **REAL** | PASS | ventana sobre la home (no es diálogo) |
| CW | dialog_cw_kbd.c | **REAL** | FAIL | render OK; FAIL porque `open_app()` fuerza modo CW (por diseño) |
| Field | dialog_field.c | **PARTIAL** | FAIL(render) | render REAL completo; no se valida QRZ/POTA (red); header dice "X6100" |
| GPS | dialog_gps.c | **PARTIAL** | PARTIAL | mensaje controlado "None: this radio has no GPS" (no hay GNSS en el X6200) |
| Bands | dialog_bands.c | **REAL** | PASS | render OK; sin red la actividad queda 0/0 |
| Settings | dialog_settings.c | **REAL** | PASS | render OK (página Operation) |
| BT | dialog_bt.c | **PARTIAL** | PARTIAL | ventana/estado controlado OK; no se valida el pairing |
| Cluster | dialog_cluster.c | **PARTIAL** | PARTIAL | render OK; no se validan spots (red) |
| CwTrainer | dialog_cw_trainer.c | **REAL** | FAIL | render OK; FAIL por el modo CW forzado (por diseño) |
| Eq | dialog_eq.cpp | **REAL** | PASS | 5 sliders, render OK |
| Status | dialog_status.c | **REAL** | PASS | render OK |
| QTH | dialog_qth.c | **REAL** | PASS | ventana de texto + teclado |
| Callsign | dialog_callsign.c | **REAL** | PASS | ventana de texto + teclado |
| Recorder | dialog_recorder.c | **REAL** | PASS | render OK; header/lista vacíos |
| WiFi | dialog_wifi.c | **PARTIAL** | PARTIAL | render OK; no se valida conexión/APs |
| SWRScan | dialog_swrscan.c | **REAL** | PASS | ventana OK; sin sesión de barrido no hay curva |
| Propagation | dialog_propagation.c | **PARTIAL** | PARTIAL | render OK; no se validan beacons (spots/NTP) |
| DFLEditor | dfl_inline.c | **REAL** | FAIL(artefacto) | panel inline REAL capturado; el estado `app` queda vacío (no es diálogo) |
| Frequency entry | dialog_freq.c | **UNSUPPORTED** | UNSUPPORTED | se abre por `HKEY_FINP` (tecla larga); el protocolo no inyecta teclas |
| CW messages | dialog_msg_cw.c | **UNSUPPORTED** | UNSUPPORTED | pulsación larga del botón de modo en CW; el protocolo no inyecta teclas |
| Voice messages | dialog_msg_voice.c | **UNSUPPORTED** | UNSUPPORTED | pulsación larga del botón de modo en SSB/AM/NFM; idem |

**Totales**: 18 apps `REAL`, 7 apps `PARTIAL`, 0 `SYNTHETIC`, 3 pantallas
`UNSUPPORTED` (no alcanzables por el barrido de las 25 apps; razón exacta arriba y en
`tests/qemu/dialog-coverage.csv`). Ninguna captura es `SYNTHETIC`: todas salen del
binario ARM real. **Nunca un PASS falso**: las apps que dependen de red/periférico son
`PARTIAL` con la foto del mensaje controlado.

## 6. Capturas y manifiesto

- **75 PNG 800×480 sin escalar** en `capturas/<app>-r<ronda>.png` (25 × 3), RGB, sin
  JPEG ni resize (export del framebuffer 480×800×32 rotado 270° como la radio).
- `manifest.csv` (columnas del issue): `screen,state,theme,source_coverage,sha,command,
  exit_code,png,width,height,issue_id,severity,source_path,observation`. `exit_code` es
  el del paso `app <nombre>` (0 = sin timeout; ninguno lo superó, 124 no aparece).
  `observation` lleva el `resultado`/`motivo` del reportero + las medidas de la captura.
- `logs/barrido.json`, `logs/results.csv`, `logs/index.txt`, `logs/intentos.jsonl`: la
  salida íntegra del reportero (`barrido_qemu.py`), sin descartar rondas.
- `logs/por-app/<app>-r<ronda>.txt`: el log de la GUI durante esa app.

## 7. Qué se mide y qué queda UNKNOWN

`medidas/medidas.csv` (PIL) trae, por PNG: `width=800`, `height=480`, `mode=RGB`,
cantidad de colores, color de fondo, color de "primer plano", **contraste WCAG** entre
esos dos, fracción de píxeles distintos del fondo y el **bbox del contenido**
(`bbox_x0..y1`). Ejemplo (BT-r1): 210 colores, fondo `#1b1529`.

- **Bounds de cada widget/fuente**: `UNKNOWN` con razón — el protocolo de control del
  binario ARM **no expone el árbol de objetos LVGL** (el `--inspect` existe solo en el
  arnés de host `tools/ui_harness`, que es sintético). No se inventan.
- **Contraste "fondo vs primer plano"**: es un proxy objetivo del par de colores más
  frecuentes, **no** el contraste real de cada texto (el fondo puede ser translúcido
  sobre la home). Se reporta lo medido, no una inferencia.
- **Legibilidad física en el LCD de 4"**: `UNKNOWN` — una captura ampliada **no** prueba
  la legibilidad en el panel físico. Requiere radio (fuera de alcance).

## 8. Defectos (medido vs hipótesis), con evidencia

**Medido** = se ve en un PNG y/o se lee en el código en el SHA congelado.
**Hipótesis** = interpretación, marcada como tal. **No** hay P0.

### P1 (bloquea la operación/documentada) — 1

- **P1-1 · `tests/qemu/run_apps_qemu.sh` no arranca el banco con el binario congelado.**
  `--check` sale 77 sin chroot y exige el chroot de antemano (aunque su mensaje diga que
  lo arma solo desde `$VOL/target`); con el chroot presente `--check` da 0 pero la
  corrida completa sale **exit 1** y **cero capturas**: el contenedor no monta `/proc` ni
  una DATA real → `x6200_gui` sale **78** `Persistence unavailable`
  (`src/params/db.c::data_mount_ok`), y el barrido aborta con "el banco privado no
  arrancó". Evidencia: `logs/runner_original_exit1.txt`, `logs/out-banco.txt`,
  `logs/preflight_original_*.txt`. *(Medido.)* Arreglo posible: la receta del QA de #213
  (§4). **No se aplicó** (fuera de alcance: solo evidencia).

### P2 (degradación visible) — 2

- **P2-1 · Los paneles de varias apps son translúcidos y el VFO de la home se ve a
  través, tapando texto de la app.** Medido: en `GPS`, el "10.100.000" de la home cruza
  la fila `Age:`; en `Status`, el "9.908" cruza `Kernel …`; en `WiFi`, cruza
  `IP address:`; en `Settings`, `Accel threshold` y `detected configs` quedan debajo de
  "9.908"/"10.292". Es **sistemático**: los diálogos se construyen con
  `dialog_init()` (`src/dialog.c:138`, `lv_obj_remove_style_all` + `dialog_style`) y
  `dialog_style` (`src/styles.c:224`) **no** fija `bg_color`/`bg_img_src` (solo
  `bg_img_opa`), así que el fondo queda transparente; solo `app_frame_body()`
  (`src/app_frame.c:46`) pinta un panel opaco, y lo usan únicamente Bands/NavTex/SSTV/
  WeFax/WSPR. *(Medido en captura + código.)* **Contexto/honestidad**: el mismo aspecto
  aparece en evidencia **previa y aceptada** (`docs/evidencia/2026-10-20-settings-alineacion/settings-01.png`),
  o sea es un rasgo **preexistente** del port, no una regresión de esta corrida; el
  dueño decide si la translucidez es intencional.
- **P2-2 · El header de Field Ops dice "X6100" en una X6200.** Medido en `Field-r1.png`
  (header "X6100  FIELD OPERATIONS") y en el código: `src/dialog_field.c:1842`
  `lv_label_set_text(x, "X6100")`. (Además `src/field/flog.c:88` escribe
  `PROGRAMID … X6100-IU1WXY` en el ADIF del log — dato, no UI.) *(Medido.)*

### P3 (pulido/observaciones) — varias

- **JS8 muestra "FT4 30m" en su ventana** mientras su estado dice "Listening for JS8…":
  el label digital es compartido y queda viejo (`cfg_digital_label_get()`,
  `src/cfg/digital_modes.c:100`, lo setea el último `cfg_digital_load` exitoso). Medido
  en `JS8-r1.png`. *(Medido; impacto a criterio del dueño.)*
- **SSTV y WeFax: canvas idle en blanco puro `#ffffff`** ocupando ~40–50% de la
  pantalla (`SSTV-r1.png`, `WeFax-r1.png`; píxeles medidos = (255,255,255)). Objetos
  `lv_canvas` en `src/dialog_sstv.c:794` y `src/dialog_wefax.c:1065`. *(Medido.)*
  **Hipótesis** (no afirmada): puede ser glare nocturno en el panel físico; requiere
  radio.
- **Recorder**: header y lista vacíos, sin título/estado visible (`Recorder-r1.png`).
  *(Medido.)*
- **Idioma mezclado en la UI de app**: `SWRScan` ("sin datos medidos",
  `src/dialog_swrscan.c:381`) y toda la ventana de `Propagation`
  (`src/dialog_propagation.c:181/601/1337/1449`: "Banda", "Paso", "Repetir",
  "Escuchar", "Cerrar") están en español, mientras el resto usa "Close"/"Clear". Medido
  en `SWRScan-r1.png`/`Propagation-r1.png`. *(Medido; consistencia de i18n.)*
- **FAILs del reportero que NO son defectos visuales** (contrato del barrido vs diseño):
  FT8/FT4/JS8/WSPR cambian freq/mode al abrirse (la app recuerda su dial) y
  CW/CwTrainer fuerzan el modo CW — esto último es **por diseño**
  (`src/remote_server.cpp:370-394`, `open_app()`). *(Medido.)*
- **DFLEditor** sale FAIL por un **artefacto**: es un panel inline, no un diálogo, así
  que el campo `app` del protocolo queda `""`; la captura prueba que el editor DFL sí
  renderiza. *(Medido.)*
- **Artefacto del banco, no de la ROM**: `Status` reporta "/mnt 63 MB free of 64 MB"
  porque la DATA del banco es un tmpfs de 64M (§4). *(Medido.)*

## 9. Determinismo

Las 3 rondas dan **la misma clasificación** para cada app. Comparando los PNG entre
rondas (`medidas/diffs_bbox.csv`), en **la mayoría de las apps la única diferencia está
en el reloj** (bbox del diff ≈ (645,12)–(745,30): el reloj de la barra superior); o sea
**el cuerpo de la app es idéntico píxel a píxel** entre rondas. En apps con contenido
vivo cambia además el área viva: FT8/FT4/WSPR (strip de waterfall), DFLEditor
(animación del esquemático), GPS (antigüedad/fecha) y Cluster (timestamp de spots).
`CwTrainer`, `Field` y `Status` son idénticos salvo el reloj
(`medidas/determinismo.csv`).

## 10. Límites (lo que esta evidencia NO prueba)

- **No** prueba legibilidad en el LCD físico de 4": una captura ampliada no lo mide.
- **No** prueba RF, audio, táctil, térmica ni consumo: no hay radio.
- GPS/WiFi/BT/Cluster/Field/Propagation/WSPR quedan **PARTIAL**: se ve el render real y
  el mensaje controlado, **no** la función (red/periférico ausentes).
- SWRScan/`Eq`/`Settings` se capturan en su estado inicial (el barrido abre, permanece 2 s
  y cierra): **no** se recorren todas las subpáginas ni sesiones de barrido.
- No se tocó `src/`, tests, temas, settings persistidos, radio, RF ni firmware.

## 11. Contenido del directorio

```
apps/
  README.md                     este documento
  manifest.csv                  75 filas (25 apps × 3 rondas), formato obligatorio
  inventario_apps.csv           tabla de cobertura por app (+3 UNSUPPORTED)
  capturas/*.png                75 PNG 800×480 reales
  medidas/medidas.csv           colores/contraste/bbox por PNG (PIL)
  medidas/determinismo.csv      sha por ronda (reloj enmascarado)
  medidas/diffs_bbox.csv        dónde difieren las rondas (bbox del diff)
  logs/                         preflight, corrida original (exit 1), barrido.json,
                                results.csv, index.txt, intentos.jsonl, out-banco.txt
  logs/por-app/*.txt            log de la GUI por app y ronda
  tools/run_apps_qemu_audit.sh  driver derivado (ver §4)
  tools/run_apps_qemu_audit.diff diff exacto contra tests/qemu/run_apps_qemu.sh
```
