# QA36 — Por qué no sale PCM audible: comparación H1 y evidencia previa

Fecha: 2026-11-03. H1 instalado. Sin nuevo flash.

## 1. Estados de registros

| Configuración | reg12 | reg24 | reg0x34 | reg0x36 | reg0x37 | reg0x39 |
|---|---:|---:|---:|---:|---:|---:|
| BASE original | 0x08 | 0x28 | 4 | 0x40 | 4 | 0x40 |
| Candidato `cc7b4aca` | 0x0A | 0x28 | field80\|0x40 ≈0x44 | field82\|0x40 ≈0x40 | field83 | field85 |
| H1 `df1e274f` | 0x0A | 0x28 | field80 ≈4 | field82\|0x40 ≈0x40 | field83 | field85 |

H1 conserva `reg0x36/0x37/0x39` y filtros; sólo elimina el bit `0x40` de `reg0x34` en la
rama DAC. Si `field80=4`, H1 deja `reg0x34=4`, igual que BASE.

## 2. Controles ALSA medidos

En QA35 se usó ALSA directo con:

- `DAC Playback Switch=on`, `DAC Playback Volume=192`
- `AIF1 DA0 Playback Volume=160`
- `AIF1 Slot 0 Digital DAC Playback Switch=on`
- `Headphone Playback Switch=on`, `Headphone Playback Volume=58`
- `Headphone Source Playback Route=DAC`

Es decir, el camino **AIF1 DA0 → DAC → Headphone** estaba configurado. Aun así no hubo tono
en DAC.

La prueba de tono en **BASE** tampoco debía sonar: QA11b ya demostró que BASE filtra el DAC
(D: BASE+TONE = sólo RX). Por tanto, el silencio en BASE **no es un fallo del codec**.

## 3. Evidencia previa reconciliada

- **QA11b es evidencia válida** de PCM Linux audible en DAC:
  - C: DAC + TONE → operador oye **sólo tono**;
  - D: BASE + TONE → operador oye **sólo RX**.
  Esto fue **antes de H1**, con el bit `0x40` presente.
- **QA33 (QSO audible)** no es evidencia de PCM: era RX nativo, no reproducción Linux.
- **QA10/10b** confirman que la ruta PCM existía, pero con eco/realimentación; no invalidan QA11b.

## 4. Diagnóstico

- **H1_DAC_ROUTE_DISABLED = LIKELY**: H1 quitó el único bit que diferenciaba DAC de BASE en
  `reg0x34`; con la ruta ALSA correctamente configurada no hubo tono, y QA11b con el bit
  presente sí lo tuvo.
- **LINUX_PCM_PATH_FAULT = UNKNOWN**: no se identificó un fallo del driver/codec; el PCM fue
  aceptado y los controles estaban puestos. La causa más probable es que H1 deshabilitó la
  ruta DAC, no un fallo ALSA.
- **PREVIOUS_AUDIBLE_EVIDENCE = VALID** para QA11b (PCM en DAC), pero **no** para el QSO de QA33,
  que era RX nativo.
- **BASE_RX_PRESERVED_WITH_DAC = NO también con H1**: QA32 midió B=143,0 frente a A=2160,7.
  H1 **no recupera RX de captura**.
- **H1_DAC_ROUTE_DISABLED = LIKELY, NO CONFIRMADO**: H1 quitó el bit y QA11b previo con el
  bit presente sí tuvo DAC+TONE; pero QA35 usó ALSA directo, no la ruta GUI/PulseAudio.
- **COMBINED_RX_DAC_ROUTE = UNPROVEN**: el candidato con bit `0x40` conserva DAC pero no RX
  de captura; H1 no conserva RX y probablemente pierde DAC. Ninguno logra ambos.

## 5. Próximo paso: rollback, no otro experimento

No se repetirán pruebas de audio ni se generará un tercer candidato sin un mecanismo nuevo.
El siguiente paso recomendado es restaurar el firmware oficial 1.0.8 mediante el updater vendor,
previa autorización específica. Ver `rollback-plan.md`.

Un modo combinado RX+DAC requeriría identificar otro registro/bit o un modo del FPGA; no existe
candidato fundamentado.

## 6. Estado

H1 instalado; radio restaurada; GUI original, RX, tx=false, ALSA intacto, sin montajes.
Sin nuevo flash. No se repitieron pruebas de volumen máximo.
