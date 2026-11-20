# Evaluación del DSP nativo (DFL/NR/NB/DNF/AGC/RX EQ)

Fuente principal: QA-NATIVE-DSP (#392, PR `[RESEARCH][DSP][REA] Xiegu native RX filters and Linux DSP integration`), sobre MCU 1.0.8/GUI vendor. Base 681074e0 no contiene esa evidencia; se cita el PR/rama.

## Estado por función (gates de #392)

| función | control | estado | potencial/condición | riesgo |
|---|---|---|---|---|
| DFL/RX filter (SSB/CW/AM/NFM/WFM) | `x6200_rxfilter` (idx30) + perfiles 45–50; edges por modo | control VERIFIED; algoritmo UNKNOWN | es el filtro principal de selectividad; candidato a A/B de ancho/edges | estrechar demasiado agrava ringing/inteligibilidad (no medido) |
| NR | reg23: enable bit24 + nivel [7:0]; MCU logs dnr threshold/type | bits VERIFIED; algoritmo UNKNOWN | reducir fatiga en ruido; nivel moderado | sobre-suprimir voz débil; artefactos (no medidos) |
| NB | reg23: enable bit25 + width [15:8] + nivel [23:16] | bits VERIFIED; algoritmo UNKNOWN | impulsos/QRN | en exceso, perfora voz |
| DNF | reg24: modo [25:24] off/manual/auto, centro [11:0], ancho [23:12] | bits VERIFIED; algoritmo UNKNOWN | notch de portadora interferente | auto puede mover el notch fuera del objetivo |
| AGC | modo por VFO (5/11), knee int8 (27), slope [11:8], hang bit12, tiempo (28) | bits VERIFIED; detector/tiempos UNKNOWN | estabilizar nivel en fading | AGC mal ajustado bombea/pumping y sube ruido en pausas |
| RX EQ | reg35/36/37: enable bit25 + 5 bandas ×5 bits | bits VERIFIED; frecuencias/ganancias UNKNOWN | compensar respuesta del parlante/micrófono interno | EQ agresivo puede recortar inteligibilidad; WFM/MIC fuera de alcance |

## Potencial real (con condiciones)

- Es la **única cadena audible de producción** hoy (ver `audio-paths.md` A).
- Ya está expuesta por la API; el GUI actual usa DFL/AGC/NR/NB/DNF/EQ (observers en `src/radio.c`). No hace falta firmware nuevo.
- **Condición**: el efecto acústico de cada parámetro no está validado; no se agregan controles nuevos "porque la API exista" (regla del issue). Los candidatos de preset (ver `recommendation.md`) son para **A/B físico**, no defaults.

## Riesgos / degradaciones a evitar

- NR/nivel alto y NB agresivo sin escucha: riesgo de artefactos y pérdida de sílabas (no medido).
- DNF auto en señales con múltiples portadoras: puede llevarse la voz.
- Cambios de AGC en medio de una comparación: invalidan A/B; fijar AGC antes.
- WFM/MIC EQ/TX: fuera del alcance, sin evidencia; no tocar.
