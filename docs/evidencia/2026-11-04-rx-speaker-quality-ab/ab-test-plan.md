# Protocolo A/B reproducible (físico y offline) — sin ejecutar acá

Estado de esta tarea: **PENDING_PHYSICAL_VALIDATION**. Ningún resultado audible/offline de este
documento se declara medición acústica.

## 0. Condiciones y seguridad

- Radio en RX, sin TX/PTT/ATU; una sola sesión; AGC/volumen/banda/modo fijados y registrados.
- **BASE original** como referencia obligatoria (firmware oficial 1.0.8, GUI original; sin MUX/router).
- **Native optimized**: mismas condiciones, sólo controles nativos existentes (DFL/NR/NB/DNF/AGC/RX EQ) vía GUI/API.
- **Linux processing**: sólo medición **offline/evidencia** del PCM de captura; **la ruta audible experimental B** se permite únicamente con autorización física del owner y después de cerrar MUX seguro (QA363) y estabilidad del router; no en esta tarea.
- **Hybrid audible**: no evaluar hasta que B sea seguro; no inventar topología.
- Si algo falla (MUX, audio, GUI): volver a BASE, restaurar preset/volumen/ajustes, un solo `x6200_gui`; no reintentar.

## 1. Señales (misma fuente por bloque, ~15–20 s c/u)

1. SSB limpia (QSO fuerte).
2. Ruido blanco/de banda (sin señal).
3. SSB débil en umbral.
4. Interferencia adyacente (portadora/estación vecina).
5. Impulsos (QRN).
6. Cambios fuertes de nivel (fading).

## 2. Bloques A/B (ciego simple, orden aleatorio, ≥2 repeticiones)

| bloque | configuración | comparación |
|---|---|---|
| A0 | BASE + defaults del operador | referencia |
| A1 | BASE + filtro más angosto (p. ej. edges SSB 100–2400 Hz) | selectividad |
| A2 | A1 + NR nivel moderado (p. ej. 20, no 40) | ruido |
| A3 | A2 + NB on (nivel/ancho default 10/10) | impulsos |
| A4 | A3 + DNF manual centro 1000 Hz ancho 50 Hz | heterodino |
| A5 | A4 + AGC slow/auto con knee -60 / slope 6 / hang off | estabilidad |
| A6 | A5 + RX EQ neutro (off; probar bandas sólo si A4/A5 no alcanzan) | timbre |
| B0 | captura + `audio_enh_process` preset **Off** (offline/medición) | control Linux |
| B1 | captura + preset **SSB voice** (offline/medición) | efecto NR |
| B2 | (sólo owner+seguridad) ruta audible experimental Off vs SSB voice | A/B audible experimental |
| C | híbrido audible | no aplica hasta B seguro |

## 3. Métricas (sólo donde son válidas)

- **Acústico/físico (radio)**: reporte del operador (inteligibilidad percibida, fatiga, artefactos) + grabación externa si el owner autoriza; A/B ciego.
- **Offline (captura, no audible)**: SNR de la señal de prueba, atenuación de ruido, clipping, distorsión (THD), latencia de cadena; STOI/ESTOI **sólo con habla real con licencia** (hoy no disponible → SKIPPED).
- **Sistema**: CPU/RAM medidos en host para la cadena Linux (no extrapolar al R16 sin medir); latencia de buffers (100 ms captura + 20 ms NR + 400 ms ring).
- **No válido**: `pactl RUNNING`, `writes/drops/underruns` como calidad; audibilidad simulada; métricas de un modelo presentadas como firmware.

## 4. Criterios de decisión

- **A** si el DSP nativo, bien ajustado, es igual/mejor que Linux en inteligibilidad percibida y no peor en artefactos.
- **B** sólo si supera audiblemente a A de forma consistente, con MUX seguro, restore verificado y stream estable (0 underruns reales), y el owner lo autoriza.
- Sin estas condiciones: **no elegir ganador**; dejar A como producción y B como experimental.
