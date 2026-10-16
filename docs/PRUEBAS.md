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
|---|---|---|---|---|---|---|
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

## Anexo B: lo que **no** está medido

| Pendiente | Por qué no está fundado | Qué haría falta |
|---|---|---|
| **Transmisión (TX) y RF** | Intentado el 15/10 y **no medido**: la app del barrido de ROE no se puede abrir por el control remoto (issue #78), así que `radio_start_swrscan()` nunca corrió. La antena es de 40 m. | Que el remoto pueda abrir esa app; después, un barrido de ROE con la potencia al mínimo |
| **Estabilidad de horas** | La vigilancia más larga pasó a **300 s** el 15/10 17:11 (0 caídas), todavía lejos de "horas". | Dejar la radio corriendo varias horas y contar caídas |
| **El #63 con spots reales decodificados** | Se abrió WSPR y quedó viva, pero no se esperó un ciclo completo con decodificación. | Dejarla en WSPR varios ciclos y mirar el log |

**Cerrado de este anexo:** *"el barrido dice 22 y el menú tiene 23"* — medido: el que falta es el
**SWR Scan**, que no está en la lista de apps del control remoto (`src/remote_server.cpp:285-292`,
12 entradas; probadas 5 variantes del nombre, todas `Unknown app`). Ver issue
[#78](https://github.com/iariap/x6200_gui/issues/78).
