# Speaker Fast-Track — prueba 1: camino de reproducción Linux existente

Fecha: 2026-11-02. Base GUI `main` = `681074e0`. Rama aislada: `feat/rx-audio-live-fasttrack`.
Objetivo: comprobar, **sin código nuevo de MUX**, si la ruta de reproducción que ya usa la ROM
(`audio_play_en(true) -> audio_play() -> audio_play_en(false)`) hace audible audio generado por
Linux en el parlante interno y vuelve al audio BASE.

## Por qué esta prueba

- El camino ya existe en la ROM actual, p. ej. `src/dialog_recorder.c` (`play_thread`):
  `audio_play_en(true); play_item(); audio_play_en(false);`
- Usa únicamente APIs existentes; no hay I²C directo desde este banco.
- No se toca PTT/ATU/TX. No se usa el camino de mensajes de voz (`send_thread` hace PTT) ni FT8/JS8.
- `audio_play_en(false)` restaura `params.hmic`/`params.imic`; no debería quedar ganancia de mic
  modificada. La prueba lo verifica a ojo en los ajustes.

## Preflight (operador)

1. Radio en RX, volumen audible pero bajo; sin TX/ATU; sin menús de transmisión.
2. Backup OEM/microSD disponible y rollback conocido.
3. Anotar: frecuencia/modo, volumen, HMic/IMic en Settings, estado de waterfall/telemetría.
4. Confirmar que se escucha el audio RX original antes de empezar.

## Prueba (una sola vez)

1. Abrir **Recorder** (grabador de audio de la ROM; NO el mensaje de voz que transmite).
2. Elegir un WAV corto ya existente. Si no hay ninguno, grabar 1–2 s de audio RX (sin PTT) y parar;
   no hace falta transmitir.
3. Presionar **Play** y escuchar el parlante interno.
4. Al terminar, esperar a que vuelva el audio RX y confirmar que vuelve.
5. Verificar: sin indicación TX/PTT/ATU; frecuencia/controles/waterfall/telemetría operativos;
   HMic/IMic iguales a los anotados; sin parámetros persistentes cambiados.
6. Si no se escucha o no vuelve el RX: **detener**, no reintentar, apagar la radio si la GUI no
   responde y reportar el fallo.

## Evidencia a reportar

- `SPEAKER_AUDIO_HEARD`: sí/no, qué se escuchó.
- `NOMINAL_BASE_RETURN_PASS`: sí/no, cuánto tardó y cómo se notó.
- TX/PTT/ATU: sin cambios sí/no.
- HMic/IMic antes/después.
- Logs: `/mnt/app_logs/boot.log`, crashlog, timestamps; foto del estado si es posible.
- Cualquier anomalía (GUI colgada, audio cortado, volumen, ruido).

No se declara `PRODUCTION_READY` por esta prueba. No se usa `pactl RUNNING` como prueba acústica.
