# Spec: banco-audio — conectar el audio del banco (decisión del owner: "podés conectar el audio con el host")

> **Nota (corrida `factory/banco-audio`)**: este spec llegó con fragmentos ilegibles (ruido del
> orquestador); el wiring y la evidencia con números están en [`docs/banco-audio.md`](banco-audio.md),
> que manda sobre lo que se lea acá.

**Fecha**: 2026-10-15 · **Corrida**: `factory/banco-audio` · **Base**: trunk `02c1311`
**Contexto**: el owner habilitó conectar el audio de QEMU con el audio del host, para
medir los remanentes del **#30** (sink en silencio) y cualquier app que genere sonido.

## El plan (en orden de costo, el primero es el objetivo de esta corrida)

### Parte A — captura determinista por ALSA file-plugin (SIN root, la prioridad)
1. El chroot del banco (`/home/pablo/desa/x6200/.vfix3`) no tiene `/dev/snd`; la GUI
   hojea al ALSA device default y falla silenciosamente. El plugin de ALSA **`type file`**
   (pcm slave hacia `null` + `file /tmp/.../capture.wav`) escribe en un WAV exactamente
   lo que la app escribiría al sink — deterministic, CI-able, sin root ni módulos.
2. Wiring limpio: el banco NO toca el chroot del owner a mano: la config vive en el
   script (un `ALSA_CONFIG_PATH` apuntando a un `.conf` generado por `captura-banco.sh`
   o variante equivalente de las métricas del arnés/banco; NO edites binarios del chroot).
   Si el app no respeta ALSA_CONFIG_PATH (usa `-D hw:...` hardcodeado al dispositivo),
   probablemente el paths del asound.conf/bisento de una device alias — decidís con
   la lectura del audio_out code (mirá `src/audio.c`, mixer_setup, cómo define el
   device default).
3. **Fuente de señal para no capturar silencio**: en el banco no hay dsp real (sin
   hardware). Buscá un generador de señal por parte del GUI:
   - SSTV genera su modulación (el `{"cmd":"app","text":"SSTV"}` en el banco)?
   - CW keyer/tone (la `key_tone`, 700 Hz — `controls` o el trainer)?
   - voz espeak si el rootfs la tiene?
   El que funcione: activarlo por socket y capturar con vol alto y vol 0 para que el
   VALOR del vol SE VEA en el RMS (el punto del #30 es que el vol se aplicara).
4. **Evidencia**: captura WAV con RMS medido (tone vs silencio, y vol=N vs M con
   proporción esperada — con `audio_set_play_vol` lineal en dB, la proporción
   en RMS es la que ens expres: documentá la cuenta) — qué mende el mixer-path medible
   sin hardware.
5. Integrá el modo al banco del arnés/compartir scripts (un flag de captura-banco.sh:
   e.g. `CAP_AUDIO=<out.wav>`), reproducible en CI (el banco puede las ci con
   SEVG debug del zero-fill y el diseño del plugin).

### Parte B — loopback al host audio REAL (sólo si puedo, si no PENDIENTE)
- El camino "host OS audio" real exige `sudo modprobe snd-aloop` + montar `/dev/snd`
  en el chroot (no hay root acá). Si la Parte A queda lista y ese circuito Nace
  disponible, preparado el wiring documentado (un guard en captura-banco.sh que
  monta /dev/snd si existe y aponta la GUI a `hw:Loopback,0` con `ALSA_CARD`),
  y **PENDIENTE** con las 3 líneas para que el owner lo ejecute (sudo).
- No rompe la Parte A (el plugin file no necesita dispositivos).

### Criterios de aceptación
- captura-banco.sh con audio: WAV con contenido NO uniforme (RMS > 0) al tone y
  nivel DIFERENTE entre vol=0 y vol=N (proporción en dB en forma del mixer-documento),
  o BLOCKED con análisis del porqué (si el GUI no permite inyectar señal).
- El re-basación de contrates/straps no cambia (solo script + docs; sin cambios de
  código del ROM: NO tocar `src/`).

## Fuera de alcance
- `src/` (el ROM NO se toca); el host queda como está salvo producir el runbook del sudo; el RF/TX.
- `lvgl/` pinned. El defecto del #30 (cuerpo del codec) sigue siendo hardware/sino
  alimitación del banco (lo que sí mediremos es el mixer path + el output digital, pero
  el sink real sigue siendo hardware).

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>` — **Sin push/PR** (publica el orquestador).
- PENDIENTE fundado > supuesto. Nada se infla a "medido": los RMS son números con
  tu cuenta, no afirmaciones.

## Reporte final
1. Parte A: el wiring y la evidencia (RMS por fase del vol/tone, etapas del mixer).
2. Parte B: el wiring documentado para el listo del sudo o PENDIENTE con análisis.
3. El test del día de que la base podabili así que si sirve al coder o feda, al salir
4. Gates relevantes (determinismo/contracts si toca el arnés; el compila qemu banko interactivo).
5. PENDIENTES honestos.
