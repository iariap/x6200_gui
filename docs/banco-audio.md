# Audio del banco — captura de lo que la GUI escribe a su sink

**Corrida**: `factory/banco-audio` · **Fecha**: 2026-10-15 · **Base**: trunk `02c1311`
**Decisión del owner**: "podés conectar el audio de qemu con el audio del host".
**Qué cambió**: `tools/captura-banco.sh` (modo `CAP_AUDIO`, sólo script) y esta doc. **Nada de `src/`**,
nada de `lvgl/`, nada del chroot del owner a mano.

El objetivo era medir el remanente del **#30** ("el sink queda en silencio digital"): hoy se puede
**capturar y medir el audio que la GUI le manda al sink** (Parte A, sin root, determinista) y quedó
**documentado con runbook** el paso que falta para medir además el **volumen del mixer** (Parte B,
necesita `sudo` del owner).

## Resumen en una tabla

| Qué | Estado | Evidencia |
|---|---|---|
| Captura del audio que la GUI escribe al sink | **hecho, medido** | `docs/evidencia/2026-10-15-banco-audio/captura-voz.txt` (RMS/pico/muestras) |
| Control sin voz = silencio digital | **hecho, medido** | `control-sin-voz.txt`: 440.094 muestras, todas 0 |
| El volumen del mixer (elemento `AIF1 DA0`) dentro de la captura | **BLOCKED en el banco** (sin tarjeta) → Parte B | `alsa-evidence.txt` §3 y §4 + el log del GUI |
| Loopback al audio real del host | **PENDIENTE del `sudo` del owner**, guard listo en el script | `parte-b-pendiente.txt` + runbook abajo |

## El wiring de la Parte A (y por qué es así)

La GUI **no escribe por ALSA**: manda su audio a **PulseAudio**
(`src/audio.c`: `pa_stream_connect_playback(play_stm, "alsa_output.platform-sound.stereo-fallback", ...)`,
nombre fijo). El chroot del banco no tiene `/dev/snd` (no hay placa), así que sin root quedan sólo dos
caminos, y uno se descartó **con medición**:

1. **El plugin de ALSA `type file` (el plan original) no sirve como captura en el banco.** Sin un
   device que consuma a ritmo real (el slave es `null`) escribe a velocidad de CPU: **3.588.764.428
   bytes en 6 s (~573 MB/s)**; con un `module-loopback` de PulseAudio delante, **2.590.433.280 bytes
   en 5 s más (~518 MB/s)**. No hay primitiva de ALSA sin hardware que ponga el reloj (`null`, `plug`,
   `rate`, `dmix`, `softvol` se probaron). Sí se validó que el plugin es **transparente** con un tono
   conocido (`alsa-evidence.txt` §2: RMS medido 8666,7 = el esperado 9192,4/√1,125 exacto, o sea las
   muestras pasan tal cual). *Queda documentado por si otro banco sí tiene reloj (ver Parte B).*
2. **Lo que sí funciona**: correr **PulseAudio adentro del chroot** con un **sink nulo que lleva el
   nombre exacto que la GUI busca** y grabar **su monitor** con `parec`. El sink nulo de PulseAudio es
   el reloj que falta (consume a ritmo real: 176 KB/s en estéreo 44100 — medido, la ventana de captura
   sale de la duración pedida). Lo capturado es, literalmente, lo que la GUI manda al sink.

La señal de "no silencio" es la **prueba de voz de la propia GUI** (`X6200_VOICE_TEST`, ver
`docs/HALLAZGOS.md` §2): espeak sintetiza, la GUI remuestrea y escribe por **el mismo `audio_play()`
que usan la voz, FT8, JS8 y SSTV**, y la GUI termina sola al terminar la frase. No depende de la radio.
espeak **no está en el chroot del banco**: el modo audio lo copia del volumen del build (binario, lib
y `espeak-data`), igual que el script ya hace con el binario de la GUI. Son 3 archivos y **quedan en el
chroot** (si molesta: `rm -f $CHROOT/usr/bin/espeak $CHROOT/usr/lib/libespeak.so*`).

## Cómo se usa

```bash
CHROOT=/home/pablo/desa/x6200/.vfix3 tools/captura-banco.sh --check    # informa pulse/parec del chroot
CHROOT=... CAP_AUDIO=/tmp/audio.wav tools/captura-banco.sh             # dice la frase y mide (8 s)
CHROOT=... CAP_AUDIO=/tmp/vacio.wav CAP_VOZ= tools/captura-banco.sh    # control: sin voz
CHROOT=... CAP_AUDIO=/tmp/vol.wav CAP_LOOPBACK=si tools/captura-banco.sh  # Parte B (sudo, ver abajo)
```

- `CAP_AUDIO=<out.wav>`: modo audio; copia la captura ahí y **imprime la cuenta** (muestras, RMS dBFS,
  pico, cuántas muestras son distintas de 0, canal por canal) + la versión y el md5 del binario.
- `CAP_VOZ="texto"` (vacío = control, sin voz). `CAP_SEGUNDOS` (ventana, 8 s por defecto),
  `CAP_LOOPBACK=si` (Parte B).
- Deja todo lo generado (config de PulseAudio, arranque interno, logs) en `$CHROOT/tmp/banco-audio/`.
- Corredor interno: 1 contenedor `banco-audio` (se borra al final), no toca `banco-apps`.

## Evidencia medida (números, con el binario `67b41837e0f4`, commit `02c1311`, QEMU `cortex-a7`)

Captura = monitor del sink, estéreo 44100 (el stream de la GUI es mono: los dos canales salen
idénticos — verificado, `canales iguales en todas las muestras: True`). El sink está en
**`0.00 dB` (100%)**: la captura no viene atenuada por PulseAudio.

| Corrida | muestras/canal | duración | RMS | pico | muestras ≠ 0 |
|---|---|---|---|---|---|
| `captura-voz.txt` (la de evidencia) | 205.778 | 4,67 s | **−25,23 dBFS** | **−3,70 dBFS** | 67.974 (33,0 %) |
| repetición 1 | 205.967 | 4,67 s | −26,33 dBFS | −3,70 dBFS | 53.503 (26,0 %) |
| repetición 2 | 205.223 | 4,65 s | −25,22 dBFS | −3,70 dBFS | 67.974 (33,1 %) |
| repetición 3 | 205.424 | 4,66 s | −25,23 dBFS | −3,70 dBFS | 67.974 (33,1 %) |
| **control `CAP_VOZ=`** | 440.094 | 9,98 s | **−inf** | −inf | **0 de 440.094 (0,0 %)** |

Lectura: con voz, la captura **no es uniforme y no es silencio** (pico −3,70 dBFS en 5 de 5 corridas);
sin voz, la captura es **silencio digital exacto** (todo 0), así que el nivel de la corrida con voz
viene de la GUI y no del camino de captura.

**Lo que todavía no está explicado** (PENDIENTE fundado, no supuesto): la captura con voz **no es
bit-reproducible**. Se ven dos poblaciones (67.974 muestras ≠ 0 con RMS ≈ −25,2 dBFS, y ~53.500 con
RMS ≈ −26,3 dBFS) y la diferencia está **en el arranque de la voz** (el final del audio cae siempre en
el mismo lugar). Lo que sí está probado:

- **espeak es determinista**: dos síntesis de la misma frase dan el mismo md5
  (`146c956371bbc6d8872b1443803a25d5`, 87.268 bytes).
- **La GUI manda todo**: el log dice `al audio 87224 de 87224` en las 5 corridas.
- **La grabación no pierde por su lado**: dos `parec` en paralelo sobre el mismo monitor dieron
  **capturas bit-idénticas** (188.877 muestras cada una, 67.974 ≠ 0, mismo tramo).
- Queda como causa candidata (no medida) un *race* del arranque del sink de PulseAudio bajo QEMU.
  Métrica estable hasta acá: el **pico** (−3,70 dBFS, 5/5) y el RMS dentro de ±1,1 dB.

## El volumen del mixer: por qué no se puede medir en el banco (y la cuenta en dB)

`audio_set_play_vol()` (`src/audio.c`) aplica el volumen con **el elemento `AIF1 DA0` de la tarjeta
`default`**. En el banco **no hay tarjeta** (`/dev/snd` no existe en el chroot): el mixer no abre y la
GUI lo dice en su log. La salida real del modo audio incluye esas líneas:

```
[Warn] audio_set_play_vol: No se pudo preparar el mixer de 'default'   (in audio.c line #403)
[Warn] audio_set_rec_vol:  No se pudo preparar el mixer de 'default'   (in audio.c line #444)
```

y arriba, en el mismo log: `ALSA lib control.c: Invalid CTL default`. Está reproducido **fuera del
banco** con la libasound del host y la misma config (`alsa-evidence.txt` §3):

```
$ ALSA_CONFIG_PATH=... amixer -D default
ALSA lib control.c:1575:(snd_ctl_open_noupdate) [error.control] Invalid CTL default
amixer: Mixer attach default error: No such file or directory
```

Y hay un motivo de fondo: el único camino sin hardware para tener un elemento de mixer *seteable* es
`softvol`, y **`softvol` necesita una tarjeta** para colgar su control (`alsa-evidence.txt` §4):

```
ALSA lib pcm_softvol.c:783:(softvol_load_control) [error.pcm] Cannot open CTL hw:9
```

**Consecuencia medible** (derivación sobre el código + el log, *no* una captura): en el banco
`audio_set_play_vol()` devuelve el valor que le pasaron sin tocar nada, así que en `tx_player_stream()`
(`src/tx_player.c`):

```
play_gain_offset = audio_set_play_vol(gain_offset + 6.0f)   -> devuelve gain_offset + 6 (mixer inerte)
gain_offset     -= play_gain_offset                         -> queda exactamente -6.0 dB
```

es decir: **toda transmisión digital (FT8/FT4/JS8/WSPR/SSTV/WeFax) sale del banco con −6 dB de
ganancia de software, y ese nivel no depende del slider de volumen** (en la radio el slider sí manda,
porque el mixer devuelve los dB realmente aplicados). Es el #30 visto desde el lado digital, y es
también por qué una captura de TX *no* puede refutar/acompañar el slider en el banco: el valor no
cambia el flujo digital. La voz (`voice.c`) no pasa por `tx_player`, así que su nivel (−25 dBFS) es el
de espeak, sin volumen de por medio.

Nota de honestidad: con la captura variando ±1 dB entre corridas, una comparación "vol=0 vs vol=N" que
diera *igual* no sería concluyente. Por eso el volumen queda del lado de la Parte B, que ataca la
causa (una tarjeta con ctl) y no el síntoma.

## Parte B — loopback al audio real del host (PENDIENTE del `sudo` del owner)

El guard ya está en el script: con `CAP_LOOPBACK=si` **chequea `/dev/snd` y la placa Loopback** y, si
no están, imprime el runbook y sale con código 2 (sin tocar nada más):

```bash
sudo modprobe snd-aloop
docker run ... -v /dev/snd:/t/dev/snd ...     # el banco monta /dev/snd dentro del chroot
CHROOT=... CAP_AUDIO=/tmp/vol.wav CAP_LOOPBACK=si tools/captura-banco.sh
```

Con eso el script (ya escrito, **no ejecutado en esta corrida: acá no hay root**) arma:
`pcm.default = plug -> softvol("AIF1 DA0", tarjeta Loopback) -> plug -> hw:Loopback,1`, un
`ctl.default` apuntando a la tarjeta del loopback (para que `audio_set_play_vol()` encuentre el
elemento) y graba el otro lado con `arecord -D hw:Loopback,0` (que además es el consumidor que hace
andar el loopback). En ese modo el volumen del mixer **entra en la captura** y el "vol=0 vs vol=N" se
puede medir por fin; el mismo runbook sirve para escuchar la GUI en el audio del host.

Dos recaudos que quedan escritos en el script: (1) el `ALSA_CONFIG_PATH` siempre apunta `ctl.default` a
la tarjeta del loopback, así los `amixer` de `mixer_setup()` no pueden tocar el mixer del host; (2) la
config de la Parte B queda en `$CHROOT/tmp/banco-audio/alsa-banco.conf` para poder revisarla. Si en el
host `hw:Loopback,1` resultara ser el lado de grabación y `hw:Loopback,0` el de reproducción (depende
de la versión del módulo), se intercambian los dos números: es el único punto que no se pudo ensayar.

## Reproducir

```bash
cd x6200_gui    # o el worktree de la corrida
git rev-parse --short HEAD                     # 02c1311
CHROOT=../.vfix3 RECONSTRUIR=no CAP_AUDIO=/tmp/audio.wav tools/captura-banco.sh   # ~10 s
```
`RECONSTRUIR=no` sólo es válido si el binario del volumen corresponde a estos fuentes (el script avisa);
con `auto` reconstruye si hace falta. La salida con las cuentas es la que está en
`docs/evidencia/2026-10-15-banco-audio/`.

## Pendientes

1. **Parte B**: `sudo modprobe snd-aloop` + montar `/dev/snd` → medir el volumen del mixer en la
   captura (vol=0 vs vol=N) y escuchar la GUI en el host. El script ya está listo.
2. **La variación de ±1 dB entre corridas** (arranque de la voz): localizar el *race* (candidato: el
   arranque del sink de PulseAudio bajo QEMU) y, si se estabiliza, dejar la captura bit-reproducible.
3. **El sink real sigue siendo hardware**: lo medido acá es el camino digital (lo que la GUI escribe
   al sink) y, con la Parte B, el mixer path; el cuerpo del codec y el parlante no se miden en el banco.
4. El plugin `type file` **no** se usa como captura (sin reloj en el banco). Si algún día el banco
   corre con `/dev/snd`, la Parte B da el reloj que le falta y vuelve a ser opción.
