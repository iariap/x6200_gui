# STOI/ESTOI y corrección de artefactos (2026-11-04)

## Hallazgo 1 — artefactos WAV corruptos

Los WAV generados en la corrida original estaban **corruptos**: los bytes binarios >0x7F habían sido
reemplazados por la secuencia UTF-8 de reemplazo (`EF BF BD`). Incluso los WAV de corpus tenían RIFF
inválido. Por eso los archivos de audio de la primera corrida **no son reproducibles ni escuchables**.
No se usaron para la decisión final.

Se regeneraron desde cero:

1. `fetch_corpus.sh` descargó de nuevo CMU ARCTIC.
2. `sha256sum -c corpus.sha256` verificó los 8 WAV base (el script tenía un error de directorio; corregido).
3. `prepare.py` + `run_all.sh` regeneraron clean/degraded/processed/blind con el DSP real.
4. Todos los WAV del nuevo paquete abren con `wave` (336/336).

## Hallazgo 2 — latencia DSP de 20 ms

Las salidas C/D1/D2 tienen un retardo medido de **882 muestras = 20 ms** respecto de B (bypass).
La cross-correlación después de alinear es ~0.994. STOI/ESTOI sin alinear da cifras engañosas
(penaliza el retardo como si fuera distorsión). El paquete ciego ahora **alinea C/D1/D2 por 882
muestras** antes de nivelar; B no se desplaza.

## Métricas STOI/ESTOI (pystoi 0.4.1 + scipy 1.18.1, alineadas)

Entorno local Python 3.12, `pystoi`/`scipy` en venv temporal. Referencia: WAV limpio de la misma
frase; 120 muestras por modo (4 speakers × 2 frases × 15 condiciones/niveles).

| modo | STOI | ESTOI |
|---|---:|---:|
| B bypass | 0.8976 | 0.7737 |
| C SSB Voice | 0.8964 | 0.7764 |
| D1 conservador | 0.8787 | 0.7659 |
| D2 NR55 | 0.8952 | 0.7749 |

- C y D2 quedan **estadísticamente casi iguales a B** en STOI; C sube marginalmente ESTOI.
- D1 empeora STOI/ESTOI de forma consistente.
- No hay evidencia de que C/D1/D2 superen al bypass en inteligibilidad objetiva.
- Estas cifras son **offline/sintéticas**, no medición del parlante X6200.

Resultados por muestra/condición: `stoi-aligned-results.csv`.

## Decisión

Se mantiene **SSB Voice actual** como default. D1/D2 no se recomiendan sin escucha humana.
`AUDIBLE_PENDING`; `PRODUCTION_READY=FAIL`.
