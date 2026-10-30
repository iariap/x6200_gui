# Conclusión de seguridad — tres claims que NO se pueden confundir (#371)

**Estado a 2026-10-30: `PREP_READY / PHYSICAL_BLOCKED`.** Este documento separa los tres claims de
evidencia del épico #362. **Ninguno de los tres está verificado hoy.** El plan de instrumentación S/P no
los verifica: sólo prepara la **primera** medición pasiva posible (P0) y deja el gate de MUX/restore
explícitamente bloqueado.

| Claim | Estado actual | Qué lo acreditaría (evidencia exigida) | Qué **NO** lo acredita |
|---|---|---|---|
| **`AUDIO_OUTPUT_VERIFIED`** | **NO — `NOT_VERIFIED`** (P0 `P0_PENDING_EQUIPMENT`) | P0 ejecutada con operador + instrumento declarado: captura instrumentada de S/P en BASE, con límites de entrada y atenuación registrados, DC/AC medidos, `tx=false`, sin MUX, señal presente (peak/RMS/espectro) y `sha256` de las capturas. Sólo audio, nada más. | Escuchar el parlante o el S/P; un sink PulseAudio `RUNNING`; `parec` pre-Speex; el grabador de pantalla AVI; la documentación Mestre; “suena” sin registro. |
| **`MUX_SELECT_PHYSICALLY_VERIFIED`** | **NO — `NOT_VERIFIED`** | Ver P1: **lectura real del registro 12** (no la sombra RAM) **o** fuente instrumentada equivalente de la selección física del MUX, **en nuestro ejemplar**, con dueño único del registro. | Audio audible/capturado en S/P; que el audio “cambie” al escribir el bit; la sombra `all_cmd.arg[]`; `shadow reg12=0x1000`; `R_INCONCLUSIVE`; Mestre; mocks de host. |
| **`RESTORE_VERIFIED`** | **NO — `NOT_VERIFIED`** | Mecanismo de restore **independiente** que devuelva BASE, con verificación física del retorno y peor caso **≤250 ms medido en la radio real**, incluyendo inyección de fallo de GUI/crash y preservación de PTT/ATU. | Reiniciar la GUI (≳4 s, reescribe todo el register file); guardián en host con mocks (p99 103,3 ms); contar CIDs; escuchar el S/P; “volvió el audio solo” sin medir el estado. |

> **Regla central:** escuchar o capturar la salida **S/P** demuestra audio resultante, **no** que el MCU
> devuelva su estado (**no** es readback del registro 12) y **no** que un guardián pueda restaurar el MUX
> tras una caída. Son tres afirmaciones distintas y hoy las tres son **NO**.

## Estado de los gates

| Gate | Estado | Condición para moverse |
|---|---|---|
| **P0** — medida BASE pasiva S/P | `P0_PENDING_EQUIPMENT` | Equipo mínimo declarado y verificado en la ventana (ver [`equipment.md`](equipment.md)) + operador + preflight completo. No toca MUX. |
| **P1** — independencia de MUX y restore | **`P1_BLOCKED_NO_SAFE_RESTORE`** | (a) lectura real del reg 12 o fuente instrumentada equivalente; (b) dueño exclusivo del registro 12 que no compita con el idle; (c) guardián que sólo despeje `0x8` tras muerte de GUI sin perder PTT/ATU y con restauración física verificada; (d) peor caso ≤250 ms medido en radio real. **Ninguna se cumple.** |
| **P2** — verificación DAC→S/P | `P2_NOT_EXECUTED` | Sólo después de P1 PASS; corresponde al retest físico de #363. No se improvisa acá. |
| **P3** — router Linux al parlante | `P3_OUT_OF_SCOPE` | #364/#365, bloqueados por #363 `GATE_A=PASS`. |

## Evidencia que fundamenta el bloqueo (ya integrada en `main`)

- **#363 / PR #366** — `GATE_A = BLOCKED_SAFE_MUX`: `x6200_control_get()` es sombra RAM (REA del binario
  desplegado), el único dueño de `/dev/i2c-0` es la GUI, `x6200_control_idle()` reescribe 218 B cada ≤1 s,
  y no hay readback real del registro 12. A4 (`NOT_EXECUTED`) no midió salida física.
- **#369 / PR #370** — `R_INCONCLUSIVE`: una única pasada controlada devolvió `00 00 00 00` en `0x0038`
  (rxvol conocido = 1) y `0x0030`; el framing `0x2000/1B→0x01` se acepta pero la vía probada **no acredita
  readback**. Rollback del GUI verificado; MUX nunca conmutado. **No se repite el sondeo.**
- **#367 / PR DRAFT #368** — `A_BLOCKED_READBACK`: máscara y guardián seguros bajo mocks (9188
  comprobaciones, p99 host 103,3 ms), pero **sin restore físico verificado**; el guardián no se habilita.
- **Mestre** — documentación externa de otro equipo: dos fuentes (BASE RX / DAC SoC), bit `voice_rec`,
  PA por `rxvol>0`, reloj `pll-audio`. Se usa como **hipótesis a medir**, nunca como evidencia de nuestro
  ejemplar ni como permiso de escritura I²C.

## Decisión operativa

- **No se autoriza conmutar el MUX.** Motivo: `NO_SAFE_MUX_STATE_TRUTH` — no existe fuente de verdad
  independiente del estado físico del MUX y no hay restore verificado. Aunque en la ventana el S/P suene
  perfecto, la conmutación sigue prohibida.
- **No se habilitan el coordinador ni el guardián** de #367 en la radio mientras no exista readback real y
  restore probado. Una prueba de audio no reemplaza ese requisito.
- **#364 y #365 no se desbloquean con #371.** Tampoco #362 se cierra. El orden sigue siendo: #363
  `GATE_A=PASS` → integración de #367 → #364 `CODE_READY` → #365 `GATE_C_PASS`.
- El equipo queda exactamente como está: sin escrituras, sin deploy, sin cambios de ruteo/UI.

## Qué reabriría P1 (sin sondeo exploratorio)

Cualquiera de estas vías, **aportada de forma externa y verificable**, y recién después el retest físico de
#363 con operador:

1. **Readback real documentado**: documentación OEM del MCU/FPGA (no `.xgf` encriptado) que especifique una
   ventana de lectura del registro 12 con semántica conocida, o un reporte de estado del BASE que exponga la
   selección de MUX. Debe ser reproducible y contrastable contra un valor conocido (como el `rxvol=1` que
   falló en R′).
2. **Sonda externa no invasiva** del estado del MUX (señal de select/feedback digital fuera del equipo)
   que permita verificar la selección y el retorno a BASE **sin abrir el chasis**.
3. **Estudio de laboratorio** con el equipo fuera de servicio y un técnico (no el agente): abrir, soldar o
   instrumentar internamente es un trabajo separado con su propio issue, riesgo y autorización; **no** es
   una tarea de #371 ni de la ventana del 12/11.

Cualquier vía elegida debe además demostrar (c) y (d): guardián que sólo despeje `0x8` sin perder PTT/ATU y
restore ≤250 ms **medido en la radio real**. Sólo con eso `MUX_SELECT_PHYSICALLY_VERIFIED` y
`RESTORE_VERIFIED` podrían pasar de NO a algo, y sólo entonces se revisa #367/#364/#365.

## Frase para citar en el PR y en el handoff

> “La captura de S/P es un **indicador del audio resultante**, no del estado del registro 12 ni de la
> capacidad de restaurar el MUX. Sin fuente de verdad independiente y sin restore físico medido, P1 sigue
> **`P1_BLOCKED_NO_SAFE_RESTORE`** y el MUX no se conmuta, suene lo que suene.”
