# Mapa de señal de RX: quién produce, quién consume y qué es audible

- **Issue**: [iariap/x6200_gui#345](https://github.com/iariap/x6200_gui/issues/345) — tarea **A** (auditoría de paths, read-only).
- **Base**: `a1c667eee6d4c138416e5ec459dd86de93cb630f` (= `origin/main` al arrancar; `git rev-parse HEAD` == `git rev-parse origin/main`).
- **Worktree**: `state/worktrees/x6200/rx-audio-feasibility`, rama `factory/rx-audio-feasibility`.
- **Qué es este documento**: lectura de código con `archivo:línea`, sin ejecutar la GUI en la radio y sin tocar
  `src/`, `rootfs/`, `tests/`, PulseAudio, ALSA, BT ni registros del BASE. **No implementa, no propone routing
  ni DSP, no propone UI.**
- **Qué NO es**: no es una medición de hardware. Todo lo que no está en el código de este repo queda marcado
  `UNVERIFIED`, `HYPOTHESIS` o `BLOCKED` con la medición que falta.

## Clases de evidencia usadas

| Clase | Significado en este documento |
|---|---|
| `CODE` | Leído en el árbol de esta base (`ruta:línea`, función). Verificable con `grep`/`sed`. |
| `MEASURED-HOST` | Medido en host/banco, no en la radio. Acá no se agregó ninguna medición nueva. |
| `MEASURED-HARDWARE` | Medición registrada en la radio **por corridas anteriores** (se cita el registro; no se repitió). |
| `HYPOTHESIS` | Inferencia a partir de `CODE` o de semántica de una API, **sin** medición que la cierre. |
| `BLOCKED` | Se quiso comprobar y no se pudo en este entorno; se dice por qué. |
| `UNVERIFIED` | No hay evidencia de ninguna clase todavía. |

Los caminos **externos** al proceso de la GUI (trayecto eléctrico BASE→parlante, sink A2DP, hardware) usan sólo
`HYPOTHESIS` / `UNVERIFIED`, nunca `CODE`.

## 0. Resumen en cinco líneas

1. El RX que procesa la GUI entra por **un** stream de captura de PulseAudio
   (`alsa_input.platform-sound.stereo-fallback`, mono S16NE 44100) y `audio_enh_process()` lo modifica
   **in situ**, antes de sus dos consumidores (`screen_rec_put_audio_samples()` y `dsp_put_audio_samples()`).
2. Ese buffer **no** va al parlante: el stream de reproducción (`play_stm`) lo escribe sólo `audio_play()`, y sus
   llamadores son audio que la GUI **genera** (TTS, FT8/JS8, mensajes de voz, recorder, `tx_player`).
3. La única ruta de RX hacia **otra** salida es el `module-loopback` de PulseAudio que carga `btaudio-ctl`
   (RX → sink A2DP). Es un cliente **distinto** del mismo *source*: no ve el procesamiento de la GUI.
4. Ninguna ruta de este árbol lleva el RX capturado al parlante (`CODE`); que el parlante lo alimente el DSP del
   BASE es `HYPOTHESIS` sin esquema ni medición (`docs/HALLAZGOS.md:937`).
5. Por eso hoy **no hay loopback** de RX procesado al parlante: no está escrito, y los dos caminos que existen
   (reproducción del SoC y A2DP) no llevan la señal procesada.

---

## 1. El camino real de `read_callback()`

`read_callback()` (`src/audio.c:57-70`) es el **único** punto de entrada del audio de recepción al proceso:

```
57  static void read_callback(pa_stream *s, size_t nbytes, void *udata) {
58      int16_t *buf = NULL;
60      pa_stream_peek(s, (const void**) &buf, &nbytes);
62      // Apply audio enhancement processing
63      audio_enh_process(buf, nbytes / 2);
65      // Screen recorder: keeps the RX audio next to the video frames
66      screen_rec_put_audio_samples(nbytes / 2, buf);
68      dsp_put_audio_samples(nbytes / 2, buf);
69      pa_stream_drop(s);
70  }
```

- `CODE` **Qué stream es**: `capture_stm`, creado con `pa_stream_new()` y conectado con
  `pa_stream_connect_record(capture_stm, capture_device, ...)` (`src/audio.c:228-232`).
  `capture_device = "alsa_input.platform-sound.stereo-fallback"` (`src/audio.c:40`).
- `CODE` **Formato**: `PA_SAMPLE_S16NE`, 1 canal (`src/audio.c:204-207`), `spec.rate = AUDIO_CAPTURE_RATE =
  44100` (`src/audio.c:225`, `src/audio.h:15`); `attr.fragsize = attr.tlength =
  pa_usec_to_bytes(AUDIO_RATE_MS=100 ms, &spec)` (`src/audio.c:29`, `226`) ⇒ nominal **4410 muestras / 8820 B**
  por callback (aritmética sobre constantes de `CODE`).
- `CODE` **Quién lo entrega**: el hilo del *threaded mainloop* de PulseAudio, arrancado en
  `pa_threaded_mainloop_start(mloop)` (`src/audio.c:170`); el callback se registra en
  `pa_stream_set_read_callback()` (`src/audio.c:231`). Corroboración de que ese hilo es "el hilo de audio":
  `src/cw_tune_ui.c:127` y `:252` lo dicen explícitamente al justificar sus accesos.
- `CODE` **Qué se procesa in situ**: `audio_enh_process(buf, nbytes/2)` (`src/audio.c:63`) recibe el buffer que
  devolvió `pa_stream_peek()` y lo reescribe. Los dos consumidores que siguen reciben **el mismo buffer ya
  procesado** (mismo puntero, `src/audio.c:66-68`).
- `CODE` **Quién más recibe el buffer**: sólo esos dos. No hay una tercera llamada dentro de `read_callback()`.
- `CODE` **Transferencia del RX al stream de reproducción**: **no existe** en este callback. La única escritura al stream de
  reproducción es `pa_stream_write(play_stm, ...)` en `audio_play()` (`src/audio.c:281`), y `play_stm` es el
  stream de `alsa_output.platform-sound.stereo-fallback` (`src/audio.c:36-37`, `217-221`).
- `CODE` **Un detalle del callback que no se cierra acá**: no se comprueba el retorno de `pa_stream_peek()`
  (`src/audio.c:60`). `audio_enh_process()` tolera `samples == NULL` (`src/audio_enhancement.c:349`) pero
  `screen_rec_put_audio_samples()` copiaría desde `samples` sin chequear (`src/screen_rec.c:261-273`, sólo mira
  `get_state()`). Si PulseAudio puede devolver un "hole" con `buf == NULL` en este modo es
  `HYPOTHESIS` (semántica documentada de `pa_stream_peek` para grabación), **no** medido: `BLOCKED` en host
  (no hay libpulse ni daemon en este entorno) y no se prueba en la radio en esta tarea.

### 1.1 Los tres clientes del mismo *source*

| Cliente | Quién lo abre | Qué pide | Evidencia |
|---|---|---|---|
| Captura de la GUI | la GUI | `connect_record`, S16 mono 44100, fragsize 100 ms | `CODE src/audio.c:228-232` |
| Medidor de pico | la GUI | `PA_STREAM_PEAK_DETECT` sobre el mismo `capture_device` | `CODE src/audio.c:492-512` (`record_monitor_setup()`), `src/audio.c:506` (`spec.rate = 30`, ignorado por *peak detect*) |
| Loopback A2DP | el demonio PulseAudio | `module-loopback source=alsa_input.platform-sound.stereo-fallback sink=<bluez_sink>` | `CODE rootfs/usr/sbin/btaudio-ctl:23` (`SOURCE=...`), `:108-109` (`pactl load-module module-loopback ... latency_msec=10 adjust_time=1`) |

Consecuencia (`CODE` + `HYPOTHESIS`): son tres *streams* distintos sobre el mismo `source`; lo que la GUI
reescribe es **su** copia. Por eso el loopback A2DP, si está cargado, entrega RX **sin** el procesamiento de
`audio_enh_process()`. Es `HYPOTHESIS` en cuanto al resultado audible (falta la comparación de grabaciones, §7).

---

## 2. Tabla por edge de señal

Convención de la columna **audible BASE**: `no` = no hay ruta hacia el parlante/auriculares del operador;
`sí (auriculares)` = sale por el sink A2DP; `UNVERIFIED` = dependería de trayecto no documentado.
La columna **captura/apps**: `sí` = llega a grabación o decodificadores.

| # | productor | consumidor | tasa/formato | hilo | efecto observable | evidencia (archivo:línea, función) | audible BASE | captura/apps |
|---|---|---|---|---|---|---|---|---|
| E1 | *source* `alsa_input.platform-sound.stereo-fallback` (lo alimenta el ADC del códec: E15) | `capture_stm` → `read_callback()` | 44100 Hz, mono, S16NE, bloques nominales de 100 ms (4410 muestras) | hilo del *threaded mainloop* de PulseAudio | RX demodulado disponible para la GUI | `CODE src/audio.c:40,204-207,225-226,228-232`; `AUDIO_CAPTURE_RATE` `src/audio.h:15` | no | sí (entrada de todo lo demás) |
| E2 | `read_callback()` | `audio_enh_process()` (in situ sobre el buffer *peeked*) | 44100 Hz reales; la cadena interna asume `AUDIO_ENH_SAMPLE_RATE=48000` y frames de 256 | hilo del mainloop PA | notch (si hubiera) + AF leveler + Speex NR sobre **la copia de la GUI** | `CODE src/audio.c:63`; `src/audio_enhancement.c:347-380` (`audio_enh_process`), `:22-40` (`audio_enh_init`), `src/audio_enhancement.h:21-22` | no | sí (condiciona lo que ven E3/E5) |
| E3 | `read_callback()` | `screen_rec_put_audio_samples()` | 44100 Hz mono S16, anillo de `1<<17` muestras (≈3 s) | hilo del mainloop PA (productor); escritor del AVI consume | track de audio del `.avi` de la grabadora de pantalla | `CODE src/audio.c:66`; `src/screen_rec.c:261-273` (`screen_rec_put_audio_samples`), `:57` (`RING`), `:77` (`ring[]`) | no | sí (sólo con `state == RUN`) |
| E4 | anillo de `screen_rec` | `write_audio()` → `avi_write_audio()` | PCM s16 mono, 44100 (`avi_open(..., AUDIO_CAPTURE_RATE)`) | `writer_thread` (`nice +10`) | archivo `/mnt/screenrec/screen-*.avi` | `CODE src/screen_rec.c:151-165` (`write_audio`), `:167-177` (`writer_thread`, `avi_open`), `:242-247`; `src/avi_writer.c:177-181` (`avi_write_audio`), `:83-85` (header `auds` mono 16 bit) | no | sí (archivo) |
| E5 | `read_callback()` | `dsp_put_audio_samples()` | entra 44100 Hz s16; se convierte a complejo con `firhilbf_r2c_execute()` | hilo del mainloop PA | alimenta decodificadores y grabadores de la GUI | `CODE src/audio.c:68`; `src/dsp.cpp:331-354` (`dsp_put_audio_samples`), `:346` (Hilbert), `:208` (`audio = malloc(AUDIO_CAPTURE_RATE * sizeof(cfloat))`) | no | sí |
| E6 | `dsp_put_audio_samples()` | `recorder_put_audio_samples()` → `sf_write_short()` | MP3 VBR (calidad 0.25), mono, 44100 | hilo del mainloop PA (escribe el archivo) | `/mnt/rec/REC_*.mp3` con el audio **ya procesado** por E2 | `CODE src/dsp.cpp:336-348`; `src/recorder.c:23-56` (`create_file`), `:78-80` (`recorder_put_audio_samples`) | no | sí (archivo) |
| E7 | `dsp_put_audio_samples()` | `rtty_put_audio_samples()` / `cw_put_audio_samples()` / `dialog_audio_samples()` | complejo a 44100 Hz (baseband) | hilo del mainloop PA | decodificador RTTY, decodificador CW, `audio_cb` de la app abierta | `CODE src/dsp.cpp:348-354`; `src/rtty.c:236`; `src/cw.cpp:429`; `src/dialog.c:179-183` | no | sí (texto/decodificación) |
| E8 | `dsp_put_audio_samples()` | `dialog_msg_voice_put_audio_samples()` | 44100 Hz s16 | hilo del mainloop PA | grabación de un mensaje de voz; **corta** el camino a recorder/decoders (`return`) | `CODE src/dsp.cpp:336-339`; `src/dialog_msg_voice.c:671` | no | sí (buffer del mensaje) |
| E9 | *source* (mismo `capture_device`) | `monitor_cb()` → `peak_db` → `audio_get_peak_db()` | `PA_STREAM_PEAK_DETECT` (el `rate` no aplica) | hilo del mainloop PA | barra de nivel del diálogo Recorder | `CODE src/audio.c:475-489` (`monitor_cb`), `:492-512` (`record_monitor_setup`), `:471-473` (`audio_get_peak_db`); único consumidor `src/dialog_recorder.c:440` (grep: 1) | no | no |
| E10a | `voice.c` (TTS) | `audio_play()` → `play_stm` | 44100 Hz mono S16 (la voz llega a 22050 y se remuestrea), trozos ≤2048 | hilo de voz (`voice.c`) | muestras de la GUI hacia el códec (AIF1 DA0) | `CODE src/voice.c:175-200`; `src/audio.c:238-289` | UNVERIFIED (ver E12) | no |
| E10b | `dialog_ft8.c` / `dialog_js8.c` | `audio_play()` | 44100 Hz mono S16, trozos ≤2048 | hilos de TX de la app | audio de TX digital hacia el códec | `CODE src/dialog_ft8.c:1272`; `src/dialog_js8.c:4821-4829` | UNVERIFIED | no |
| E10c | `dialog_msg_voice.c` / `dialog_recorder.c` / `tx_player.c` | `audio_play()` | 44100 Hz mono S16 | hilos `play_thread` / TX | reproducción de mensajes, WAV del recorder, TX de RTTY/WeFax | `CODE src/dialog_msg_voice.c:299-313`; `src/dialog_recorder.c:190-200`; `src/tx_player.c:139` | UNVERIFIED | no |
| E10d | `audio_play()` | `play_stm` (`alsa_output.platform-sound.stereo-fallback`) | S16NE mono 44100, `fragsize` 100 ms, `tlength` = 8× fragsize (800 ms) | el que llama (E10a-c) | bytes aceptados por el sink del SoC | `CODE src/audio.c:36-37,213-221,280-282`; `pa_stream_write` es la única escritura (grep) | UNVERIFIED | no |
| E11 | `audio_play_en(true/false)` | registros del BASE (`hmic`, `imic`, `record`) | — (I2C) | el que llama (hilos `play_thread`) | cambia el ruteo **del equipo** mientras se reproduce: `record_set(true)` + micrófonos a 0; al salir restaura | `CODE src/audio.c:370-389`; SDK `tools/ui_harness/third_party/aether_radio/x6200_control/control.h:44` (`record_set`: "Toggle audio stream mic/speaker or RX/TX audio"), `:91-93` | UNVERIFIED (efecto audible real) | no |
| E12 | *source* RX | `module-loopback` → `bluez_sink.<MAC>.a2dp_sink` | loopback PA (S16, tasa del sink A2DP), `latency_msec=10`, `adjust_time=1` | demonio PulseAudio | RX **crudo** en auriculares A2DP, con el altavoz del equipo en 0 si `BTAUDIO_MUTE=yes` | `CODE rootfs/usr/sbin/btaudio-ctl:10,23,80-116,290`; `rootfs/etc/pulse/system.pa.d/btaudio.pa:1-18`; mute del altavoz `src/btaudio.c:108-131` + `src/radio.c:792-802` (`radio_vol_apply`) | sí (auriculares) | no |
| E13 | `btaudio_speaker_mute()` / conexión de auriculares | `radio_vol_apply()` → `x6200_control_rxvol_set(0)` | registro 14 del BASE | LVGL thread (`btaudio_follow_state`) / hilo que mutea | parlante del equipo a 0 mientras hay auriculares | `CODE src/btaudio.c:104-141`; `src/radio.c:792-802` (`radio_vol_value`/`radio_vol_apply`); `src/radio.c:669-690` (registro de observers) | sí (silencio del parlante) | no |
| E14 | demodulador del BASE | parlante / jack del equipo | analógico (desconocido) | — | voz de RX que el operador escucha hoy | `HYPOTHESIS` — no hay esquema; `docs/HALLAZGOS.md:937` lo lista como hipótesis sin fundar ("sin medición ni esquema") | sí (es la ruta que hoy suena; el trayecto es `HYPOTHESIS`) | no |
| E15 | códec del SoC (`AIF1 DA0`) | BASE (¿mezcla a parlante? ¿sólo TX?) | analógico/digital (desconocido) | — | explicaría que el audio generado por la GUI se escuche (o no) | `HYPOTHESIS`, con evidencia **contradictoria**: `docs/HALLAZGOS.md:150-168` midió la cadena del SoC configurada y **sin sonido** (12/10); `docs/evidencia/2026-10-22-cwaudio244/README.md:80-86` interpreta que lo que genera la GUI **sí** se escucha (reporte del dueño, sin log) | UNVERIFIED | no |

Filas: **17** (E1–E15, con E10 desdoblada en E10a–E10d).

## 3. Diagrama

```
                        BASE / placa del equipo
   ┌──────────────────────────────────────────────────────────────────────────────┐
   │  demodulador (modo del VFO activo)                                           │
   │        │                                                                     │
   │        ├──────────────► parlante / jack del operador   [E14 UNVERIFIED]       │
   │        │                 (HYPOTHESIS: alimentado por el BASE, no por el SoC)  │
   │        │                                                                     │
   │        └──────────────► ADC del códec del SoC                                │
   └──────────────────────────────┬───────────────────────────────────────────────┘
                                  │  [E1 CODE] 44100 mono S16NE, 100 ms
                    source PulseAudio "alsa_input.platform-sound.stereo-fallback"
                     │                        │                        │
      [E1 CODE]      │        [E12 CODE]      │        [E9 CODE]       │
                     ▼        module-loopback ▼        PA_STREAM_PEAK  ▼
        capture_stm (GUI)      a bluez_sink.*.a2dp_sink       monitor_stm (GUI)
                     │                    │                        │
                     ▼                    ▼                        ▼
      read_callback()  [E1/E2]     auriculares A2DP          peak_db → barra del Recorder
        │  audio_enh_process() in situ (E2)   (RX SIN procesar: HYPOTHESIS)
        │        │
        │        ├──► screen_rec_put_audio_samples  [E3] → ring → writer_thread
        │        │                                              [E4] → .avi
        │        │
        │        └──► dsp_put_audio_samples         [E5] → hilbert r2c
        │                         ├── [E8] dialog_msg_voice (grabación; corta)
        │                         ├── [E6] recorder → REC_*.mp3
        │                         └── [E7] rtty / cw / dialog_audio_samples
        │
        └── (NO hay escritura al stream de reproducción desde acá)

   audio_play()  ◄── voice.c (TTS) / dialog_ft8 / dialog_js8 / tx_player /
        │            dialog_msg_voice / dialog_recorder          [E10a-c CODE]
        ▼
   play_stm "alsa_output.platform-sound.stereo-fallback"  [E10d CODE]
        │
        ▼
   códec del SoC (AIF1 DA0) ──► BASE  [E15 UNVERIFIED]
        ▲
        └── audio_play_en() escribe hmic/imic/record  [E11 CODE]
```

Leyenda: cada edge lleva su id de la tabla §2. Los que no tienen evidencia cerrada están marcados
`UNVERIFIED`/`HYPOTHESIS` en el propio diagrama.

## 4. Ciclo de vida y propiedad de `SpeexPreprocessState`

`nr_state_t` guarda el puntero (`src/audio_enhancement.h:53-58`):

```
52  /* Noise reduction state */
53  typedef struct {
54      bool enabled;
55      uint8_t level;  // 0-100
56      SpeexPreprocessState *speex_state;
57      float noise_gate_thresh;
58  } nr_state_t;
```

| Operación sobre el estado | Quién y dónde | Cuándo (arranque / runtime) | Clase |
|---|---|---|---|
| **crea** | `audio_enh_init()` → `speex_preprocess_state_init(256, 48000)` (`src/audio_enhancement.c:26`), dentro de `audio_init_sync()` (`src/audio.c:168`) | una vez, en el hilo de inicialización del audio (`audio_init()` → `audio_init_thread`, `src/audio.c:107-123,155-170`), **antes** de conectar el stream de captura (`src/audio.c:228-232`) | `CODE` |
| **configura** | `speex_preprocess_ctl(..., SPEEX_PREPROCESS_SET_DENOISE, 1)` (`src/audio_enhancement.c:28-29`) | arranque | `CODE` |
| **escribe el nivel de supresión** | `audio_enh_nr_set_level()` → `speex_preprocess_ctl(..., SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress)` (`src/audio_enhancement.c:151-158`) | runtime: cada `audio_enh_preset_load()` (`:165-214`) | `CODE` |
| **procesa** | `speex_preprocess_run()` (`src/audio_enhancement.c:312`) desde `apply_noise_reduction()` (`:293-318`), llamada por `audio_enh_process()` (`:365`) | por cada callback de captura (hilo PA) | `CODE` |
| **destruye** | `audio_enh_deinit()` (`src/audio_enhancement.c:42-55`) | **no tiene llamador en `src/`** (grep: sólo la definición y los tests) ⇒ en producción vive lo que dura el proceso | `CODE` |
| **notches (mismo `g_audio_enh`)** | `firfilt_rrrf_create_notch()` (`:77`), destruidos en `:46`/`:96` | **sin llamador de producción** para `audio_enh_notch_add()` (grep en `src/`: sólo declaración y tests) ⇒ hoy el camino de notch no se ejercita en la radio | `CODE` |

### 4.1 Qué campos cruzan hilos (*posible* acceso concurrente, **no** carrera probada)

Quien escribe el estado global desde fuera del hilo de audio:

- `audio_enh_apply_mode_preset()` (`src/audio_enhancement.c:331-345`) lee `params.audio_enh_preset` y
  `cfg_cur.mode`, y llama a `audio_enh_preset_load()` si cambió el perfil efectivo. Se invoca desde
  `src/dsp.cpp:293` (`on_cur_mode_change`, observer de `cfg_cur.mode`) y `src/main.c:602` (arranque).
- `audio_enh_preset_load()` desde el desplegable de Ajustes: `src/dialog_settings.c:426-432`
  (`audio_enh_update_cb`), hilo LVGL.
- Ambas rutas terminan en `audio_enh_set_enabled()`, `audio_enh_nr_set_enabled()`,
  `audio_enh_nr_set_level()` y `audio_enh_af_leveler_*()` (`src/audio_enhancement.c:147-159,165-214`), que
  escriben campos de `g_audio_enh` y, en el caso de `nr_set_level`, llaman a `speex_preprocess_ctl()` sobre
  el **mismo** `speex_state` que el hilo de audio está usando en `speex_preprocess_run()`
  (`src/audio_enhancement.c:312`).

Lectura del mismo estado desde el hilo de audio: `audio_enh_process()` (`:347-349` lee `enabled`),
`apply_af_leveler()` (`:246` lee `af_leveler.enabled` y escribe `current_gain_db`/`peak_gain_db`),
`apply_noise_reduction()` (`:294` lee `nr.enabled`).

- `CODE`: no hay mutex, atómico ni sección crítica en `src/audio_enhancement.c` (0 ocurrencias de
  `pthread_mutex`/`_Atomic`/`atomic_` en el archivo).
- `HYPOTHESIS` (por inspección, **no** probada): (a) lectura/escritura concurrente de `enabled`/`level`/
  `af_leveler.*` entre hilo LVGL y hilo PA; (b) `speex_preprocess_ctl()` concurrente con
  `speex_preprocess_run()` sobre el mismo `SpeexPreprocessState`. La documentación oficial de SpeexDSP que
  cita el issue advierte que el estado no es seguro para uso simultáneo sin sincronización; **acá no se
  ejecutó ningún detector** (ni TSan ni helgrind) ni se reprodujo en la radio ⇒ no se afirma carrera.
- `BLOCKED` para cerrarlo: TSan/helgrind sobre el binario real está fuera de esta tarea y el runner nativo
  compila con stubs (`tests/native/run_native_tests.sh:4`: "LVGL/liquid/Speex are stubbed"), así que correrlo
  no probaría nada sobre el estado real de Speex.

### 4.2 Otro dato de propiedad que importa para la viabilidad

`g_audio_enh.float_buffer[AUDIO_ENH_FRAME_SIZE]` (`src/audio_enhancement.h:80`) es el buffer de trabajo de
`audio_enh_process()` (`:357-360`): es **compartido y no reentrante**. Hoy sólo lo toca el hilo de audio
porque el único llamador de producción es `read_callback()` (`src/audio.c:63`); con dos llamadores
concurrentes se pisaría. `CODE` (estructura) + `HYPOTHESIS` (consecuencia).

## 5. Por qué hoy **no** hay loopback de RX procesado hacia el parlante

Tres hechos independientes, sin interpretación de arquitectura:

1. **No está escrito.** Ninguna función de `src/` transfiere muestras de `read_callback()` al stream de
   reproducción. `play_stm` tiene un único escritor (`pa_stream_write`, `src/audio.c:281`) y sus llamadores
   son audio **generado** (E10a-c). `CODE` (grep de `pa_stream_write` sobre `src/`: una sola ocurrencia).
2. **El procesamiento es local al proceso.** `audio_enh_process()` reescribe el buffer que devolvió
   `pa_stream_peek()` (`src/audio.c:60-63`). Ese buffer es de la copia del *stream* `capture_stm`; el
   `module-loopback` A2DP tiene su propio *stream* sobre el mismo *source* (`rootfs/usr/sbin/btaudio-ctl:108`).
   Que por eso el A2DP entregue RX sin procesar es `HYPOTHESIS` (semántica de clientes de PulseAudio); la
   medición que lo cierra está en §7.
3. **La ruta del SoC hacia el parlante no está documentada.** Lo que la GUI escribe en `play_stm` sale por
   `AIF1 DA0` (`src/audio.c:73-77`, `391-430`) y su destino físico depende del ruteo del BASE, que activa o no
   `x6200_control_record_set()` (`src/audio.c:370-389`). Hay dos registros de evidencia **contradictorios**:
   uno mide la cadena del SoC configurada y **sin sonido** (`docs/HALLAZGOS.md:150-168`) y otro interpreta que
   el audio generado por la GUI se escucha (reporte del dueño, sin log:
   `docs/evidencia/2026-10-22-cwaudio244/README.md:80-86`). ⇒ `UNVERIFIED`.

Además, una restricción de código que cualquier uso futuro tendría que resolver **antes** de escribir diseño
(se registra como restricción, no como propuesta): `read_callback()` corre en el hilo del *threaded mainloop*
(E1) y `audio_play()` toma ese mismo lock (`pa_threaded_mainloop_lock`, `src/audio.c:256-267,280-282`).
Llamar a `audio_play()` desde `read_callback()` sería re-entrar el lock desde el propio hilo del mainloop.
`HYPOTHESIS`: deadlock o aserción de libpulse (el patrón soportado es el inverso: los hilos externos toman el
lock, como en `src/audio.c:135-143`). `BLOCKED` para verificarlo acá: sin `libpulse` en el host
(`pkg-config --exists libpulse` → ausente) y sin poder tocar la radio en esta tarea.

## 6. Lo externo al proceso: qué queda sin evidencia y qué medición lo cerraría

| Edge / afirmación | Clase | Qué haría falta para fundarla |
|---|---|---|
| Trayecto eléctrico demodulador del BASE → parlante/jack (E14) | `HYPOTHESIS` (`docs/HALLAZGOS.md:937`: "sin medición ni esquema") | esquema del equipo, o medir la señal en la salida del DSP del BASE y en el parlante (osciloscopio/sonda); alternativa: firmware original (`docs/HALLAZGOS.md:4289`, §67) para ver qué registros mueve |
| El códec del SoC está cableado al parlante (E15) | `UNVERIFIED` (evidencia contradictoria, §5 punto 3) | reproducir una señal conocida por `play_stm` con el volumen ≠ 0 y medir en el parlante; y la misma señal por el jack. Con la GUI andando, `speaker-test` no se puede (placa tomada: `docs/HALLAZGOS.md:164`) |
| El `module-loopback` A2DP entrega RX **sin** el procesamiento de la GUI (E12) | `HYPOTHESIS` | grabar a la vez el monitor del `bluez_sink` y el audio del `.avi` de la grabadora de pantalla, con un preset de NR fuerte y una señal estable; comparar. Requiere auriculares reales (medido antes sólo con Pixel Buds el 13/10: `rootfs/usr/sbin/btaudio-ctl:136`) |
| Nivel real de audio del parlante / si el registro 14 tiene efecto continuo | `UNVERIFIED` | `parec` de 2–3 s + RMS con dos valores de `vol` distintos (`docs/evidencia/2026-10-22-cwaudio244/README.md:185-191`), sin cambiar el mute del usuario |
| Tamaño real de bloque que entrega PulseAudio (¿siempre 4410?) | `UNVERIFIED` para la GUI; `MEASURED-HARDWARE` sólo como tasa global, citado en `src/audio_enhancement.c:325` (`parec` 441000 B en 5 s, medido en la radio; no reproducido acá) | `parec --format=s16le --rate=44100 --channels=1 --device=alsa_input.platform-sound.stereo-fallback` y contar bytes por lectura; para el callback real hace falta instrumentar (fuera de esta tarea) |
| `buf == NULL` en `pa_stream_peek()` durante grabación (§1) | `HYPOTHESIS` | instrumentar el retorno de `peek` en una corrida de la radio (cambio de código: fuera de esta tarea) |

## 7. Qué NO demuestra este documento

- **No** mide audio: no hay WAV, ni RMS, ni latencia, ni CPU de esta corrida.
- **No** afirma que el parlante esté alimentado por el BASE ni que el códec del SoC no llegue al parlante: son
  `HYPOTHESIS`/`UNVERIFIED` (§6).
- **No** afirma una carrera en `audio_enhancement.c`: describe accesos entre hilos **posibles** por inspección
  (§4.1) y dice qué falta para probarlo.
- **No** juzga si el procesamiento actual mejora o empeora la inteligibilidad, y no propone routing, DSP ni UI.

## 8. Reproducción de la lectura (read-only)

Todos los datos de arriba salen de lectura estática sobre `a1c667ee`; los comandos usados fueron consultas, no
ejecuciones de la app:

```sh
git rev-parse HEAD; git rev-parse origin/main          # a1c667ee...  (== origin/main)
grep -rn "pa_stream_write\|pa_stream_peek\|pa_stream_connect_" src/
grep -rn "audio_play(\|audio_play_en" src/
grep -rn "dsp_put_audio_samples\|screen_rec_put_audio_samples" src/
grep -rn "audio_enh_init\|audio_enh_deinit\|audio_enh_process\|audio_enh_apply_mode_preset" src/
grep -rn "module-loopback\|btaudio" rootfs/usr/sbin/btaudio-ctl
grep -c "pthread_mutex\|atomic" src/audio_enhancement.c   # 0
```
