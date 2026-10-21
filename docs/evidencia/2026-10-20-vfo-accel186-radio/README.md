# Sintonía adaptativa del VFO (#186 / PR #187) verificada en la radio (lo que se puede por remoto)

- Rama `lu2dsx/x6200-rom`, merge **`23313505`** ("Dial VFO: sintonía adaptativa determinista por
  detents/s, umbral configurable y Settings en inglés (issue #186) (#187)"), publicado en `b60a032d`.
- Build en la radio: **`760af1734ea7e57b10419d4e33df9368`** (deploy del otro agente), PID 25499,
  equipo sin reiniciar, 1 sola GUI.

## Verificado

| Item | Resultado | Evidencia |
|---|---|---|
| Deploy del build | md5 `760af173…`, verifica `TODO OK` | — |
| **Settings -> Operation: "Accel threshold"** | presente, valor **07**, unidad **detents/s** | `01-settings.png` |
| **"Freq acceleration"** | **Lite** (multiplier None/Lite/Strong) | `01-settings.png` |
| Settings en inglés | sí ("Freq acceleration", "Accel threshold", "detents/s") | `01-settings.png` |
| Alineación de Settings (#175) | respetada: "Lite" y "07" en la misma columna | `01-settings.png` |
| Estabilidad 180 s | PID 25499 estable, 0 logs nuevos, 0 CRASHLOG | — |

## No verificable por remoto (es físico, lo valida el owner)

- **El "feel" del dial**: aceleración por detents/s, histéresis 0.80x, freno a 180 ms, reverso x1. El
  propio commit lo dice: "No medí el feel del dial: es físico".
- Cambiar el multiplicador a None/Strong y el umbral: son controles táctiles.
