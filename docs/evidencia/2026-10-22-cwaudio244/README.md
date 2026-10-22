# #244 — Ir a CW y volver deja el audio de RX mudo: análisis del camino, descarte del #232 y procedimiento para la radio

- **Fecha**: 2026-10-22 (corrida del factory; worktree `state/worktrees/x6200/cw-audio-mute-244`, rama `factory/cw-audio-mute-244`).
- **Base**: `35918ec6` (= tip de `origin/lu2dsx/x6200-rom` al arrancar la corrida).
- **Issue**: [iariap/x6200_gui#244](https://github.com/iariap/x6200_gui/issues/244).
- **PR que ya arregla la clase de silencio**: [#247](https://github.com/iariap/x6200_gui/pull/247) (`lu2dsx/244-cw-mute`, commit `16976863`, abierto 2026-10-22T21:31:40Z, base `lu2dsx/x6200-rom`).
- **Estado de esta corrida**: el **disparo del mute del dueño no quedó identificado** (es lo que el propio #247 declara como no resuelto). Esta corrida **no duplica el arreglo**: lo **verifica** con la matriz de gates, **descarta el #232** con evidencia de código, acota los caminos que pueden dejar el audio en 0 y deja la **medición exacta que falta** para la radio. Registro: `docs/HALLAZGOS.md` §65.

## 0. Por qué esta rama no repite el arreglo del #247

El PR #247 ya trae, sobre la misma base:

| Pieza | Dónde |
|---|---|
| `radio_vol_value()`/`radio_vol_apply()`: el único lugar que decide y escribe el registro 14 | `src/radio.c` (PR #247) |
| El mute del usuario y el de auriculares A2DP compuestos por estado (no por toggle) | `src/radio.c`, `src/btaudio.c` (PR #247) |
| La perilla re-manda el registro cuando el nivel no se movió (tope 0/100) | `src/radio.c` (PR #247) |
| **Mitigación** del #244: re-mandar el nivel al salir de CW/CWR | `on_cur_mode_change_vol()`, `src/radio.c` (PR #247) |
| 16 comprobaciones nuevas sobre el `radio.c` real | `tests/native/test_radio_thread.c` (PR #247) |
| El hallazgo con las mediciones de la radio (`vol=0` en `params.db`, 3 de 4 corridas de aceptación en 0, log de la sesión muda) | `docs/HALLAZGOS.md` **§64** (PR #247) |

Repetir eso en `factory/cw-audio-mute-244` daría **dos secciones §64** y dos versiones del mismo
`src/radio.c` sobre la misma base: un conflicto seguro y ninguna información nueva. Lo que falta —y
es lo que trae esta rama— es: (a) el **descarte del #232** con evidencia de código, (b) la **lectura
del camino completo** con archivo:línea y la corrección de una lectura del issue, (c) la
**verificación independiente** de los gates del arreglo en vuelo, y (d) el **procedimiento de radio**
con la medición que discrimina el disparo.

> Nota de convivencia: si el #247 entra a `lu2dsx/x6200-rom` antes que esta rama, el rebase toca el
> final de `docs/HALLAZGOS.md` y el índice (los dos PR agregan una sección al final). La resolución
> es textual: quedan §64 (PR #247) y §65 (esta rama), en ese orden.

## 1. El camino del audio de RX (código, base `35918ec6`)

### 1.1 Qué le manda la GUI al BASE al entrar o salir de CW

El BASE se controla por I2C (`/dev/i2c-0`, dirección `0x72`; registro = número de comando × 4:
`.x6200_ctl/src/low/control.c`, `x6200_control_cmd()`). Un cambio de modo escribe esto y nada más:

| Qué | Registro | Dónde (esta base) |
|---|---|---|
| modo del VFO | `x6200_vfoa_mode` = 4 | `src/radio.c:385` (`on_vfo_mode_change`) → `src/radio.c:388` |
| ancho del filtro RX + el filtro del modo | `x6200_rxfilter` = 30, `x6200_filter_cw` = 47 | `src/radio.c:419`/`src/radio.c:439` → `set_rx_filters()` de la lib (`.x6200_ctl/src/control.c:99`) |
| squelch (nivel + enable) | `x6200_micsel_…_sqlthr` = 21, bits 8-15 y 24 | `src/cfg/cfg.c:441` (`update_sql_level`) → `src/radio.c:465` → `src/sql_policy.c:15` |
| (además, del lado DSP) modo vigente + perfil de enhancement | — | `src/dsp.cpp:292` (`on_cur_mode_change`) |

Ninguno de esos registros es un volumen ni un mute: el nivel de audio de recepción es el registro
**14** (`x6200_rxvol`), que en esta base **sólo** escriben el camino del volumen
(`src/radio.c:535` → `x6200_control_rxvol_set`, y `src/radio.c:684` dentro de `radio_change_mute()`).

### 1.2 Ningún camino de software de la GUI lleva el audio de RX al parlante

Esto es lo que acota el problema a "registro del BASE" y descarta todo lo que viva en el DSP:

- La captura (`alsa_input.platform-sound.stereo-fallback`) entra por `read_callback()`
  (`src/audio.c:57-69`) y sale por tres destinos: `audio_enh_process()` (en el lugar),
  `screen_rec_put_audio_samples()` (el AVI) y `dsp_put_audio_samples()` (decodificador/recorder/
  diálogos). **Nunca** se escribe al parlante.
- El stream de reproducción (`play_stm`, `alsa_output.platform-sound.stereo-fallback`,
  `src/audio.c:37`) lo escribe **una sola** función, `audio_play()` (`src/audio.c:281`), y sus
  únicos llamadores son las apps de audio digital (FT8/JS8), los mensajes de voz, el recorder y
  `tx_player` — o sea, audio que la GUI **le manda a la radio**, no el audio de recepción.
- El propio `mixer_setup()` lo dice: `// Play level from app to radio (for FT8)` (`src/audio.c:75`).
- El registro 14 lo documenta el SDK como **"Speaker volume"**
  (`aether_radio/x6200_control/control.h:43` en `.x6200_ctl/include`).

Consecuencia: el audio de RX que el dueño escucha lo produce el **BASE board**, y la única palanca
de la GUI sobre él son sus registros.

### 1.3 El tono de prueba y la voz salen por el mismo sink: el dato no aísla al códec

El issue (y `assessment-194/scratch/audio-failure-2210.md`) usan "los tonos de prueba por PulseAudio
no fueron audibles" como prueba de que la salida estaba muerta. El dato **no aísla** una causa:

- El tono de prueba fue al mismo sink que usa la GUI para lo que genera (`play_device`,
  `src/audio.c:37`, único consumidor `audio_play()`), y lo que la GUI genera **sí** se escucha en el
  parlante de la radio: es el síntoma de §1 de este registro (el TTS no sonaba porque `pulse` no
  estaba en el grupo `audio`; arreglado, el dueño lo escucha) y el camino documentado en
  `mixer_setup()` como "app to radio (for FT8)" (`src/audio.c:75`).
- O sea: por el parlante de la radio pasan **dos** audios — el de recepción (camino del BASE, §1.2) y
  el que genera la GUI (códec del SoC) — y en el estado mudo los dos estaban en silencio.
- Un único registro del SDK gobierna ese bloque compartido: `x6200_rxvol` ("**Speaker volume**",
  `aether_radio/x6200_control/control.h:43`). Con el registro 14 en 0 se explica el silencio en las
  dos direcciones, con PulseAudio y el códec sanos.

**Corrección de una lectura del registro de evidencia del 22/10**: "la salida no suena (ni tonos)"
se leyó como una segunda falla de la salida. Con el escenario compartido, ese dato es consistente
con **una** causa (el nivel en 0) y no es prueba de una falla de hardware. La prueba que sí distingue
está en §5 (captura con el nivel conocido, y la secuencia de reinicio del GUI).

### 1.4 El reenvío que pide el issue ya ocurre cada segundo

`radio_thread()` llama a `x6200_control_idle()` cada `IDLE_TIMEOUT` = 1000 ms (`src/radio.c:334`) y
esa función reescribe **los 54 registros** en una transacción
(`.x6200_ctl/src/low/control.c:480`, `send_regs(&all_cmd, sizeof(all_cmd))`), registro 14 incluido,
con el valor que la GUI tenga en su caché. O sea: si el mute fuera el **valor** del registro 14, la
GUI ya lo re-escribe una vez por segundo. Reenviar `rxvol` al salir de CW (lo que hace el #247 como
mitigación) **no agrega un reenvío**: agrega un reenvío *inmediato después del cambio de modo*. Que
el BASE acepte y latchee ese bloque de 218 bytes **no está medido** (anexo A).

## 2. El preset del PR #232: DESCARTADO como disparo

El PR #232 (mergeado 2026-10-22T17:49:42Z) agregó `audio_enh_apply_mode_preset()`
(`src/audio_enhancement.c:331`) y la llamó desde el cambio de modo (`src/dsp.cpp:293`), desde el
arranque (`src/main.c:602`) y desde Ajustes (`src/dialog_settings.c:398`). Las dos reproducciones
del dueño son posteriores (20:55Z y 21:15Z) con la ROM `0f4f4bae`, que incluye el #232. **No es la
causa**, por tres hechos de código:

1. **No toca el BASE ni el audio del sistema**: `src/audio_enhancement.c` no tiene **ninguna**
   llamada a `x6200_control_*` (0 ocurrencias), ni a ALSA/PulseAudio (`pa_*`, `snd_*`, `amixer`: 0
   ocurrencias; sus únicos includes son LVGL, cfg/subjects, radio, params, math/stdlib/string y
   liquid). Lo único que hace `audio_enh_preset_load()` (`src/audio_enhancement.c:165`) es fijar
   `g_audio_enh.enabled`, `nr.enabled`, `af_leveler.enabled` y sus parámetros.
2. **Su efecto cae sobre la copia que la GUI procesa, no sobre el parlante**: `audio_enh_process()`
   se llama sólo desde `read_callback()` (`src/audio.c:62`), sobre el buffer de **captura**, antes
   del screen-recorder y del DSP (§1.2). El parlante no pasa por ahí.
3. **No puede producir silencio**: el único camino a la ganancia es `apply_af_leveler()`, con el
   rango acotado a ±30 dB (`src/audio_enhancement.c:246-292`), y los notch son de banda angosta. Un
   perfil distinto cambia el nivel/limpieza del audio que ve el decodificador y el AVI, no lo apaga.

Corolario útil: si el #232 afecta algo audible en CW es la **calidad** del tono que procesa la GUI
(el motivo del PR), no la presencia del audio de RX.

## 3. Las otras candidatas del issue, cada una con su razón

| Candidata | Qué dice el código | Veredicto |
|---|---|---|
| `radio.c: on_vfo_mode_change()` → `x6200_control_vfo_mode_set()` (`src/radio.c:385-389`) | Escribe **sólo** el registro de modo del VFO (`x6200_vfoa_mode` = 4; la lib no toca otro registro en ese camino). No es un mute ni un volumen | La GUI no mutea por acá; que el modo cambie el audio **dentro** del BASE no se puede afirmar ni descartar desde el código |
| "re-sync de volumen" (`x6200_control_rxvol_set`) | El registro 14 = "Speaker volume" (SDK) y su valor sale de `cfg.vol.val` / `radio_change_mute()`. Ya se reescribe cada 1 s por `x6200_control_idle()` (§1.4) | **Necesario para el estado, redundante como mitigación** mientras el valor esté bien: converge con el #247 (que lo implementa) y con su propio marco de "mitigación, no arreglo" |
| `radio_change_mute()` (`src/radio.c:682-685`) | `mute = !mute; x6200_control_rxvol_set(mute ? 0 : vol)`: un **toggle**, y `radio_change_vol()` (`src/radio.c:655-676`) limpia `mute` **sin** reenviar el registro cuando el nivel no se puede mover (`if (new_val != vol)`), así que la GUI puede quedar diciendo "no estoy muda" con el equipo en 0 | **Defecto de código fundado** (es la clase de "silencio sin causa visible"); el #247 lo arregla por estado. Es el único camino de la GUI que deja el equipo en **silencio total con todo lo demás sano**, que es exactamente el síntoma del issue |
| La ventana/app de CW (`src/cw.cpp`, `src/cw_tune_ui.c`, `src/dialog_cw_trainer.c`) | 0 llamadas a `x6200_control_*` en los tres archivos. Sus palancas son el modo (vía `cfg_cur.mode`) y las teclas de volumen de esos diálogos (`radio_change_vol(∓1)`, `src/dialog_cw_trainer.c:1065`, `src/dialog_cw_kbd.c:355`, `src/dialog_msg_cw.c:224`) | No mutea por sí misma; **puede** bajar el nivel por el mismo camino que la perilla (el `vol_mode` del encoder es global, `src/vol.c:19`, y su default es `VOL_VOL`) |
| El squelch | Se re-aplica en cada cambio de modo (`cfg.c:441` → `sql_policy.c:15`) y también en el arranque (`src/radio.c:626`, **después** de los setters del registro 21) | Descartado **por medición ajena** (PR #247): en `params.db` el `sql` estaba en **0** con la radio muda, y el enable se deriva del nivel (`sql_policy.c:31`) |
| `spmode` (Speaker/Phone, bit 5 del registro 21) y `key_train` | `spmode` sólo se escribe en el arranque y desde el switch de Ajustes (`src/dialog_settings.c:1786-1800`); `key_train` no toca audio de RX | Descartados **por medición ajena** (PR #247): `spmode=0`, `key_train=0` en el `params.db` de la radio muda |

## 4. Lo que está medido (por la sesión del PR #247) y qué falta

**Medido por esa sesión** (no por esta corrida; los comandos y su salida están en su
`docs/HALLAZGOS.md` §64 y en el cuerpo del #247):

- `params.db` de la radio el 22/10 21:25Z: `vol|0` con `sql|0`, `spmode|0`, `key_train|0`, `moni|13`.
  Con `vol=0` la GUI manda 0 al registro 14: el mismo valor con el que `radio_change_mute()` mutea.
- `get` del control remoto en 4 corridas de aceptación del 22/10: `vol` = 0, 0, 0 y 14.
- El log de la sesión que quedó muda: la ida y vuelta a CW está (`mode=4` → `mode=0` en 0,6 s) y **no
  hay una sola línea** de ALSA/PulseAudio, mixer ni "no acepta datos".
- El audio de recepción sale por el camino del BASE (no por el códec Linux) y la captura del códec
  no sirve como discriminante por sí sola (el propio registro tiene una lectura de RMS 141,2 con el
  audio funcionando).

**Lo que falta, y es exactamente lo que esta corrida deja medido como procedimiento** (§5):

1. **El disparo**: qué camino dejó `cfg.vol.val` (o el mute) en 0. En esta base los únicos escritores
   del nivel son `radio_change_vol()` (perilla/teclas de volumen/remoto, `src/radio.c:667`), el
   comando CAT (`src/cat.cpp:574`), `settings_manager` (Ajustes/remoto) y `radio_change_mute()`
   (toggle). **Un cambio de modo no escribe el nivel**: no hay ningún camino que ligue el modo a
   `cfg.vol.val` (barrido de writers en §3). O sea: si el mute del dueño fue el nivel, algo **externo
   al cambio de modo** lo puso en 0 ese mismo rato (la sesión de pruebas automáticas contra la radio
   viva es la candidata con medición: 3 de 4 corridas quedaron en 0).
2. **La contradicción del power-cycle**: con `vol=0` persistido, un arranque vuelve a mandar 0
   (`src/radio.c:535` → registro 14), así que el arranque **no** explica la recuperación que el
   dueño reporta. Ese es el dato que separa "fue el nivel" de "fue un estado del BASE", y es
   barato de medir: reiniciar **sólo la GUI** (no la radio) durante el mute.

## 5. Procedimiento de verificación en radio (PENDING: validación del owner)

> Restricciones respetadas por esta corrida: **sin SSH, sin deploy, sin flash, sin TX, sin sudo** y
> sin tocar la configuración de BT/audio. Todo lo de acá lo corre el dueño, cuando la radio esté
> libre, con el binario del arreglo (#247).

### 5.1 Antes de tocar nada (estado sano, lectura)

Los helpers son los que ya usa el QA (`tools/lib-radio.sh`: `ssh_radio()`, `radio_ip()`), y el
`x6200-remote` tiene que estar en la radio (`/tmp/x6200-remote`; un boot lo borra, se repone con
`tools/remote/compila.sh` + `scp`, ver `docs/PRUEBAS.md`).

```sh
. tools/lib-radio.sh
IP=$(radio_ip); r() { ssh_radio "$IP" "$@"; }
rget() { r "/tmp/x6200-remote '{\"cmd\":\"get\"}'"; }

rget                       # -> ..."vol":N,"sql":0...
r "sqlite3 -readonly /mnt/params.db \"select name,val from params where name in ('vol','sql','spmode','key_train');\""
r 'parec --format=s16le --rate=44100 --channels=1 --device=alsa_input.platform-sound.stereo-fallback >/tmp/a.raw 2>/dev/null & sleep 5; kill $! 2>/dev/null'
r "od -An -td2 -v /tmp/a.raw | awk '{for(i=1;i<=NF;i++){v=\$i<0?-\$i:\$i; s+=v; if(v>m)m=v; n++}} END {printf \"media %.1f pico %.0f muestras %.0f\", (n?s/n:0), m, n}'"
```

**Sano esperado**: `vol` ≠ 0 (el nivel del dueño), `sql` 0, `spmode` 0; la captura con pico alto y
media alta, como el estado sano que midió el issue (RMS 2450,7 / pico 12192); en el estado mudo que
el issue midió, el pico cae a 144 y la media queda clavada en 141,2. Ojo: la captura **no** es un
discriminante por sí sola (§4).

### 5.2 Reproducir el mute

1. Con el nivel audible (`vol` ≥ 1), cambiar a **CW** y volver a **LSB** (por la pantalla o por el
   remoto: `/tmp/x6200-remote '{"cmd":"mode","val":"CW"}'` y después `"LSB"`).
2. Si el audio se va:
   - leer **`vol`** y el `params.db` **sin tocar nada** (los dos comandos de §5.1), y anotarlos;
   - reiniciar **sólo la GUI** (no la radio) y volver a mirar `vol` + el audio:
     `r "kill \$(pidof x6200_gui)"` — el daemon (`x6200_daemon`) la vuelve a levantar solo
     (es lo mismo que hace `tools/prueba-gui-en-radio.sh:65` y `deploy-ui-ssh.sh:45`);
   - recién después, el power-cycle.

**Qué distingue cada resultado**:

| Resultado | Lectura |
|---|---|
| Al salir de CW el audio sigue bien | La mitigación del #247 alcanzó (el registro 14 se re-manda en el cambio de modo). Anotarlo con el md5 del binario |
| Mute, `vol` = 0 | El silencio **es el nivel**: el disparo es quien puso el nivel en 0 (perilla/teclas de volumen en el contexto de CW, remoto o CAT). Si además el `vol` del `params.db` ya era 0 **antes** de la reproducción, el "mute" estaba puesto desde antes y el viaje a CW no lo causó |
| Mute, `vol` > 0 y **la GUI reiniciada lo destraba** | El estado vive en lo que la GUI re-aplica en el arranque (registro 14 y/o 21): se puede acotar leyendo `vol` antes y después del reinicio |
| Mute, `vol` > 0 y **sólo el power-cycle lo destraba** | El estado está **dentro del BASE** (firmware). La GUI sólo puede mitigar: es el caso para escalar al fabricante con este registro, no para inventar un workaround nuevo |
| Con `vol` ≥ 1 la captura sigue constante (RMS 141,2) y el parlante mudo | El mute no es el nivel: la medición que falta es el registro 14/21 **del BASE** (no hay readback de registros en el SDK: sólo se pueden mirar los flags del flow) |

### 5.3 Medición que falta y que cerraría el caso

- **Readback del BASE**: el SDK no expone lectura de registros (`x6200_control_get()` devuelve la
  caché de la GUI, `.x6200_ctl/src/low/control.c:466`), así que la única ventana al estado del BASE
  es el **flag `sql_mute`/`sql_fm_mute` del paquete de flow** (ya parseado y hoy descartado:
  `src/radio.c:224` es un `printf` comentado). Encender ese log (2 líneas, por cambio de flag) es la
  medición que diría si el propio BASE está muteando por squelch. No se implementó acá para no
  pisar la reescritura de `radio.c` del #247.
- **Loggear el nivel aplicado**: con el #247, `radio_vol_apply()` es el único lugar que decide el
  registro 14; una línea de log ahí (valor + si venía de mute/BT/perilla) convierte la próxima
  reproducción en "quién puso el nivel en 0", sin adivinar. Tampoco se implementó acá por la misma
  razón.

## 6. Verificación de host corrida por esta corrida (gates)

Comandos y salidas reales: [`gates.txt`](gates.txt).

| Gate | Base `35918ec6` | Fix `16976863` (PR #247) |
|---|---|---|
| `sh tests/native/run_native_tests.sh` | `EXIT=0` (stage de `radio.c`: **36 comprobaciones, 0 fallos**) | `EXIT=0` (stage de `radio.c`: **57 comprobaciones, 0 fallos**) |
| `sh tests/native/run_native_tests.sh --ci-parity` | — | `EXIT=0` · `== ci-parity: OK (build host sin lvgl/) ==` |
| `python3 tests/audita-pruebas.py` | — | `EXIT=0` · `OK: estructura y evidencia en orden` (237 builds, 14 secciones, 59 enlaces) |
| `sh tests/run_all_tests.sh --quick` | — | `EXIT=0` · **20 OK, 0 fallos** (`host` y `host_ci_parity` incluidos) |
| `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | — | `EXIT=0` · 16 checks `ok` (nativos, 12 estáticos, `x6200-ab themes` y `x6200-ab contracts` del sha) |
| `ASan/UBSan` | `== sanitizers: NO disponibles en este toolchain (se omite) ==` | ídem (SKIP del runner, no un verde) |
| `banco QEMU` (preflight) | — | `BLOCKED: falta entorno` con el detalle (falta el volumen del build, el chroot, `tools/remote/x6200-remote`); corre en el host del owner |

**Observación del compilador**: el stage de `radio.c` emite
`warning: no return statement in function returning non-void [-Wreturn-type]`
(`radio_src/radio.c:339`). Está **también en la base** (y hay otro igual en `src/params/params.c:617`
por el mismo motivo del stage con `--gc-sections`): el #247 **no agrega** ninguna observación nueva.
No se silenció nada.

**SKIP con motivo** (no son pases): `check_wcag` de temas, `ruff`/`pytest` del arnés A/B, `bash -n`,
`harness determinism` (todos por paths: el #247 no toca `rootfs/`, `tools/ab_testing/` ni
`tools/ui_harness/`); `qemu_apps`, `wspr`, `sstv`, `wefax` (falta entorno/headers en la imagen `ci`).

## 7. Resumen del estado

- **El #232 no es la causa** (evidencia de código, §2): no toca el BASE, ni ALSA, ni PulseAudio, y su
  efecto cae sobre la copia que la GUI procesa, nunca sobre el altavoz.
- **El camino del issue (re-sync de volumen al salir de CW) converge con el #247** y, con el
  `x6200_control_idle()` de cada 1000 ms, ya estaba cubierto como *valor*: su valor real es el
  reenvío inmediato después del cambio de modo (§1.4).
- **El disparo del mute del dueño queda NO identificado**, con una contradicción medida que lo acota:
  `vol=0` explica el síntoma completo pero no explica la recuperación por power-cycle (§4). La
  medición que lo cierra está en §5.3 (reiniciar sólo la GUI durante el mute, y el log del flag
  `sql_mute` del flow).
