# Handoff presencial — P0 pasiva S/P (desde 2026-11-12, una página)

**Issue #371 · épico #362.** Objetivo único de la ventana: medir **audio RX del BASE en la salida S/P** con
un banco externo, sin tocar la radio más allá de `S/P MODE` y volumen (operador). **MUX, TX/PTT/ATU,
firmware, GUI y deploy quedan fuera.** Antes de empezar, leer [`preflight.md`](preflight.md),
[`wiring.md`](wiring.md) y [`equipment.md`](equipment.md).

## Antes de conectar (si algo falta: STOP)

1. **Equipo declarado**: cable TRS macho 3,5 mm **de 3 contactos**; medidor de continuidad/breakout;
   interfaz de audio **LINE** externa (no MIC con bias) con **límite de entrada y aislamiento declarados**;
   atenuador pasivo; multímetro/oscopio para medir DC/AC del S/P. Sin esto → `P0_PENDING_EQUIPMENT`, no se
   conecta nada.
2. **Manual Rev.05 en mano**: contrastar el dibujo del S/P (AUDIO tip · NC ring · GND sleeve).
3. **Host** listo para WAV 44100 Hz + `sha256`; **operador** presente y con autoridad para abortar.
4. Registrar **línea base** read-only: `tx=false`, modo/frecuencia/volumen, BT, `sha256` del GUI
   (referencia `4ee87aef…66b0`) y backup `/mnt/ui-bak-*` si existe. **P0 no cambia la GUI.**

## Secuencia (cada paso puede terminar en STOP)

1. **Radio apagada.** Verificar continuidad del cable desconectado: punta↔AUDIO, anillo↔NC, manguito↔GND.
2. **Radio apagada.** Conectar el TRS al S/P **solamente** (nunca KEY/ACC). Sin atenuador todavía.
3. Encender en RX, antena conectada, sin ATU/PTT. Operador: `S/P MODE = EAR PHONE`, **volumen bajo**.
4. Verificar `tx=false` (dos muestras separadas ≥10 s) y que modo/VFO/volumen no cambien solos.
5. Medir **DC y nivel AC** del S/P con el instrumento declarado. Anotar instrumento y límite de entrada.
6. **Radio apagada.** Insertar atenuador + interfaz LINE aislada; encender; re-verificar `tx=false`.
7. Capturar **10 s** de RX propia/ruido (sin publicar QSO de terceros). Si se autoriza, registrar en
   paralelo el PCM source PA (`parec`, sólo lectura) para estimar afinidad; no confundirlo con la salida
   procesada.
8. Completar `measurements-template.csv`: `utc, mode, s_p_mode=EAR PHONE, connector_verified=true,
   instrument, input_limit, attenuation, measured_peak, measured_rms, source_pcm_sha, external_pcm_sha,
   gate=P0, status`. Sin medición real, **no se inventa ninguna fila**.
9. Verificar que la captura tenga señal y que los niveles estén dentro del instrumento (sin clipping).
10. **Cierre**: radio apagada, desconectar, encender y verificar estado final (`tx=false`, GUI hash igual).

## Cuándo abortar de inmediato

- Clipping/saturación, pop/click fuerte o repetido, overload de la interfaz.
- Cualquier indicio de **TX/PTT/ATU** (`tx=true`, medidor de RF, ATU tuneando).
- Inestabilidad (GUI cae/reinicia, audio desaparece, modo/volumen cambian solos).
- Calor, olor, chispa, DC inesperada, calentamiento del conector.
- Molestia/dolor auditivo o cualquier duda del operador.

**Acción:** apagar, esperar, desconectar con radio apagada, registrar hora/estado/hashes y **no reintentar**
hasta entender la causa. No “arreglar” escribiendo registros ni reemplazando la GUI.

## Prohibido en esta ventana

- Conmutar el MUX o cargar el guardián de #367 (`P1_BLOCKED_NO_SAFE_RESTORE`).
- TX/PTT/ATU/CW keyed; abrir el chasis; soldar; sondear I²C; flashear/deployar; cambiar UI o ruteo.
- Conectar a KEY/ACC, usar TS mono/adaptadores, o conectar el S/P a otra salida de audio.
- Publicar audio de terceros, binarios o filas de medición inventadas.

## Reportar al proyecto

Qué se midió y qué no, con archivos/hashes y la planilla completada sólo con datos reales; si faltó equipo,
`P0_PENDING_EQUIPMENT` con el detalle. **No** se declara `AUDIO_OUTPUT_VERIFIED` sin captura instrumentada,
ni `MUX_SELECT_PHYSICALLY_VERIFIED`/`RESTORE_VERIFIED` por escuchar S/P: son tres claims distintos
([`safety-conclusion.md`](safety-conclusion.md)). #364/#365 siguen bloqueados.
