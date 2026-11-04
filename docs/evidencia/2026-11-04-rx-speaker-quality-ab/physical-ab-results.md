# A/B físico nativo — resultados preliminares (2026-11-04)

Clase: **AUDIBLE (reporte del operador)** + **HARDWARE** (estado verificado por `x6200-remote`).
No es A/B ciego formal de laboratorio ni medición acústica objetiva.

## Condiciones

- CB-D canal 6, **27.475.000 Hz**, USB, firmware oficial 1.0.8, GUI original.
- Volumen fijo **43** durante los pares; frecuencia y modo verificados en cada bloque.
- Sin MUX, sin `reg12/reg24/0x34`, sin firmware/flash, sin TX/PTT/ATU/Bluetooth.
- Dos bloques de 12 s por caso, orden aleatorio oculto; el operador no sabía qué configuración
  correspondía a cada bloque hasta responder.
- El estado BASE se restauró al final de cada caso: filtro 2700, NR/NB off, AGC AUTO.

## Caso 1 — NR (2 pares)

| par | bloque 1 | bloque 2 | preferencia |
|---|---|---|---|
| 1 | BASE (NR off) | **NR20** | **NR20** |
| 2 | **NR20** | BASE (NR off) | **NR20** |

Reporte: menor ruido de fondo, voz clara, natural y cómoda; sin voz robótica ni artefactos
evidentes. **Preferencia consistente 2/2 por NR20** en esta muestra.

## Caso 2 — DFL (1 par; NR20 fijo)

| bloque | filtro | reporte |
|---|---:|---|
| 1 | **2700** | mejor voz; más ruido de fondo |
| 2 | **2400** | menos ruido; voz algo apagada |

El operador indicó que depende de la señal/QRM; **ambos cómodos**. No hay ganador único.
Queda como **compromiso dependiente de condición**. No se cambia el default.

## Caso 3 — NB (1 par; NR20 + filtro 2700)

NB off vs NB on (nivel/ancho 10/10): **iguales**. Sin reducción de impulsos ni degradación
evidente en esta muestra. Resultado **INCONCLUSIVE / sin beneficio demostrado**; no se propone
NB como mejora por defecto.

## Caso 4 — AGC (2 pares; NR20 + filtro 2700)

| par | bloque 1 | bloque 2 | preferencia |
|---|---|---|---|
| 1 | AUTO | **SLOW** | SLOW, con duda (“creo”) |
| 2 | **AUTO** | SLOW | **AUTO** |

El primer par fue ambiguo; el segundo favoreció AUTO. Resultado **inconsistente**:
**no hay evidencia de mejora estable de AGC SLOW**. Se mantiene **AUTO** por defecto.

## No probado

- **DNF** y **RX EQ** no están expuestos por `x6200-remote`; no se probaron en esta sesión.
- Inteligibilidad objetiva STOI/ESTOI, THD, latencia y CPU en R16: **PENDING/BLOCKED**
  (sin voz licenciada ni instrumentación).
- No se probó la ruta Linux→parlante; sigue experimental y no segura para producción.

## Conclusión operativa preliminar

- **NR20**: único candidato con preferencia audible consistente (2/2), a validar en más señales.
- **DFL**: ajuste según QRM; no fijar 2400 por defecto; conservar 2700 como base.
- **NB**: no demuestra mejora en esta muestra.
- **AGC**: conservar **AUTO**; SLOW no estable.
- Arquitectura: **A (DSP nativo)**; sin firmware nuevo, sin MUX, sin DSP Linux audible.
- Estado restaurado: CB-D ch6, 27.475.000 USB, filtro 2700, NR/NB off, AGC AUTO, volumen 43,
  RX y `tx=false`.
