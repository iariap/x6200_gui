# Protocolo repeatable de provenance Linux-DSP (para sesión autorizada)

Objetivo: atribuir una diferencia audible al DSP Linux (Speex/notch/nivelador) con **una** diferencia
experimental: ruta Linux ON/OFF; sin nuevos efectos/settings.

## Preparación
- Build QA reproducible de #396 (feature ON + patch X6200Control + gate QA); GUI original de rollback
  respaldada con sha256; firmware oficial 1.0.8; operador presente.
- Fuente autorizada y estable: generador RF/ señal conocida en VFO fijo (p.ej. 27.635 MHz USB) o
  grabación inyectada autorizada; sin TX/PTT/ATU; sin Bluetooth/USB/audio externo.
- Congelar: modo USB, filtro 2700, AGC AUTO, NR/NB off, volumen fijo, VFO; documentar
  `state` pre-sesión y preset nativo (uno solo) sin cambios en el DSP nativo.
- Aislamiento acústico: grabador externo (teléfono/grabadora) frente al parlante interno; misma
  posición/ganancia durante todas las tomas.

## Tomas (mínimo 3 A/B/A alternadas)
1. **Bypass/native:** ruta Linux OFF (QA inactiva, BASE nativo audible), 30 s de fuente estable.
2. **Linux:** misma fuente, activar QA (gate+env), mismo VFO/volumen, 30 s.
3. Repetir A/B/A para descartar tendencia/persistencia; monitorear `pactl list short sink-inputs`
   (capturar salida) y logs QA (`activation`, `BASE`).
4. Cancelación/rollback inmediato si: TX no intencional, audio ausente/incorrecto, sospecha de MUX,
   comportamiento no repetible. Restaurar GUI original y verificar state/volumen/BASE.

## Análisis
- Comparar espectro/nivel/SNR entre A y B con el mismo segmento; buscar diferencia reproducible que
  desaparezca al volver A. Atribuir a Linux sólo si: única variable = ruta QA; diferencia repeatable;
  rollback restaura A. Aun así, **provenance física del MUX** queda separada (sin readback).
- Registros: grabaciones + logs + command outputs, con timestamps; software timestamps sólo
  complementarios.
