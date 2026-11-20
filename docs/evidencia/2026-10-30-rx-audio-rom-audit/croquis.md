# Croquis — ruta de audio RX del X6200 y alternativas (#362)

Todos los croquis son **estáticos** (derivados de código/DT/kernel/rootfs y de evidencia medida previa integrada).
**Nada fue medido físicamente en esta fase, no se conmutó el MUX y no se tocó la radio.** Leyenda:
`[C]` = controlado por Linux/ROM que compilamos · `[B]` = dentro del BASE (MCU/FPGA propietario, firmware encriptado)
· `[U]` = **UNKNOWN / no verificado**.

## 1. Vista de sistema: qué lado del códec controla cada cosa

```text
┌──────────────────────────── SoC Allwinner A33/R16 (Linux 6.1.82) ────────────────────────────┐
│                                                                                              │
│   AIF1 (I2S, simple-audio-card "sun8i-a33-audio")                                            │
│      │                                                                                       │
│      ├── ADC  ◄── Left/Right ADC Mixer ◄── Mic1 Amp ◄── MIC1 (pin)  ──────┐                  │
│      │                                                                   │  [C] DT/ASoC/ALSA │
│      └── DAC  ──► Left/Right Mixer ──► HP Source Playback Route=DAC      │                  │
│                        │                                                 │                  │
│                        └──► HP Amp ──► HP (pines)  ────────────────────┐ │                  │
│                                                                        │ │                  │
│   i2c@1c2ac00 (I2C0, mv64xxx, sin nodos hijos)  ◄── /dev/i2c-0 crudo   │ │                  │
└────────────────────────────────────────────────────────────────────────│─│──────────────────┘
                                                                         │ │
                     enlace analógico entre el códec del SoC y el BASE  │ │
                     (nets/pinout exactos [U]; sin esquemático)           │ │
                                                                         ▼ ▼
┌──────────────────────────────── BASE (Xiegu) ──────────────────────────────────────────────┐
│  [B] MCU/FPGA: firmware .xgf ENCRIPTADO (sin fuente, sin símbolos, no analizable)           │
│                                                                                             │
│        selector de fuente de audio (MUX) ◄── I2C slave 0x72, reg 12 bit voice_rec=0x8       │
│           │                                                                                 │
│           ├── "audio de RX" (demodulado por el BASE) ──┐                                    │
│           └── "DAC del SoC" (viene de HP del códec) ───┤                                    │
│                                                        ▼                                    │
│        volumen de recepción rxvol = reg 14 ──► PA ──► selector de salida spmode (reg 21 bit 5)│
│                                                        │                                    │
│                                          ┌─────────────┴─────────────┐                      │
│                                          ▼                           ▼                      │
│                                     PARLANTE (4 Ω)              S/P 3,5 mm (AUDIO/GND)      │
│                                                                                             │
│  Todo el bloque [B] se controla solamente por I2C (regs) y UART (/dev/ttyS1); no hay        │
│  driver/DT/GPIO de Linux para el MUX.                                                       │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

## 2. Ruta de captura (RX del BASE → SpeexDSP) — **funciona y está medida en banco**

```text
[B] RX demodulado ──► MIC1 ──► [C] Mic1 Amp ──► ADC Mixer ──► ADC A33
      └──────────────────────────────────────────────────────────────────┐
                                                                         ▼
[C] AIF1 AD0 ──► simple-audio-card "sun8i-a33-audio" ──► ALSA hw:0,0 capture
                                                                         │
                                                                         ▼
[C] PulseAudio source  alsa_input.platform-sound.stereo-fallback   (src/audio.c:40)
                                                                         │
                                                                         ▼
[C] GUI  capture_stm  ── pa_stream_peek ──► read_callback()        (src/audio.c:85)
                                                                         │
                    ┌────────────────────────────────────────────────────┤
                    ▼                                                    ▼
[C] audio_enh_process()  = notch → AF leveler → SpeexDSP NR      [C] screen_rec_put_audio_samples()
    SpeexDSP 1.2.1, trama 441 (10 ms), 44 100 Hz                 [C] dsp_put_audio_samples()  (decoders/waterfall)
    (src/audio.c:76; src/audio_enhancement.c:208,518,556)        (src/audio.c:80-82)
                                                                         │
                                                                         ▼
                                                          FIN DE LA CADENA: no hay flecha al parlante
```

**Medido en #351 (host, `libspeexdsp` real):** delay 882 muestras (20 ms) con NR, partición invariable, 0 clipping;
**no** se midió calidad audible ni CPU del R16. Esta fase re-verifica por código que el destino del PCM procesado es
recorder+decoders (no `play_stm`).

## 3. Ruta de reproducción existente (apps/voz/TX) y por qué no sirve para el RX

```text
[C] audio_play()  (src/audio.c:326)  ◄── voz / recorder / FT8 / JS8 / SSTV / TX
        │  play_stm (sink "alsa_output.platform-sound.stereo-fallback", src/audio.c:37)
        ▼
[C] AIF1 DA0 ──► DAC A33 ──► Mixer (DAC Playback Switch) ──► HP Source=DAC
        │
        ▼
[C] HP Amp (Headphone Playback Switch/Volume) ──► HP (pines)
        │
        ▼
[B] MUX: si y sólo si voice_rec=1, el BASE toma este audio; si no, sigue el audio de RX
        │
        └──► PA ──► parlante / S/P

audio_play_en(true) = hmic=0 + imic=0 + record_set(true)   (src/audio.c:458-478)
                                   │
                                   └── reg 12 |= 0x8  (compartido con PTT 0x40000 y ATU en el mismo word)
```

Consecuencia: **no hay forma de que el DAC del SoC se oiga en el parlante sin el bit `voice_rec`**, y ese bit no tiene
readback real ni dueño serializado fuera de la GUI. Un sink PulseAudio en `RUNNING` sólo prueba PCM en el códec.

## 4. Alternativa (c): validación audible por A2DP (no es el parlante)

```text
[C] ALSA source alsa_input.platform-sound.stereo-fallback  (captura CRUDA, pre-SpeexDSP)
        │  module-loopback (btaudio-ctl rx on)                (rootfs/usr/sbin/btaudio-ctl:23,80-116)
        ▼
[C] bluez_sink.<MAC>.a2dp_sink ──► auriculares BT            ← audible real (medido 13/10, HALLAZGOS §25)
        │
        └── NO toca el BASE, NO toca el MUX, NO toca el parlante
```

Para validar **el stream procesado** por SpeexDSP de forma audible haría falta que la GUI escriba el PCM post-enhance
en un sink propio (p. ej. null-sink / sink A2DP) — cambio **sólo de GUI/PulseAudio**, sin registros del BASE; **no
implementado ni autorizado en esta fase** (propuesta en `alternatives.md`). El parlante del X6200 sigue con su audio
de RX; si el agregado falla, el audio del equipo no se interrumpe.

## 5. Alternativa de validación offline (sin radio)

```text
[C] host x86_64: tests/native (callback de captura #351 + machina NR con stubs)
[C] host x86_64: banco #351 con libspeexdsp real (audio sintético determinista)
[C] (opcional en la radio, sin MUX) snd-aloop=m / snd-dummy=m  → sink/source virtual, no audible
```

## 6. Recuperación: lo que hay y lo que costaría (no ≤250 ms)

```text
Fallo                                        Mecanismo existente                         Tiempo / límite
───────────────────────────────────────────  ──────────────────────────────────────────  ──────────────────────────
audio_play_en() sale bien                    record_set(false) en el mismo proceso      ms, pero exige que el proceso viva
GUI cae (crash/exit)                         x6200_daemon relanza la GUI                backoff 2 s + init 1,7–1,9 s ≈ 4 s
GUI cuelga (no muere)                        NINGUNO (el supervisor espera wait())      indefinido
PulseAudio se traba/se reinicia              NINGUNO sobre el MUX (PA no lo toca)       no aplica
pérdida de stream PCM                        NINGUNO sobre el MUX                       no aplica
watchdog del SoC                             CONFIG_SUNXI_WATCHDOG=y                    resetea el SoC, NO el BASE; boot >250 ms
reset del BASE                               GPIO 204 (probado en #244)                 invasivo: resetea todo el BASE; audio no recuperado
apagado del BASE                             S11radio_daemon stop: echo 's' >/dev/ttyS1 apaga el BASE; no es restore
broker nuevo (reg 12)                        no existe en la ROM                         sin readback, sin interlock PTT/ATU
```

## 7. Los tres claims de #371 en una imagen

```text
AUDIO_OUTPUT_VERIFIED ────────► NO (no hay medición física; P0 pendiente de equipo)
MUX_SELECT_PHYSICALLY_VERIFIED ► NO (reg 12 sin readback real; R_INCONCLUSIVE)
RESTORE_VERIFIED ─────────────► NO (no hay mecanismo independiente; ≈4 s por reinicio de GUI)

       ╳ sin fuente de verdad del MUX y sin restore verificado ──► NO se conmuta el MUX
       ╳ un sink RUNNING, un parec, un A2DP audible o "suena" no acreditan ninguno de los tres
```
