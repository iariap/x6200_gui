# #351 — Validación física del pipeline de captura RX (2026-10-29)

Tercer nivel de evidencia del issue [#351](https://github.com/iariap/x6200_gui/issues/351), separado
de la implementación y de la evidencia de software:

1. **Implementación**: PR [#352](https://github.com/iariap/x6200_gui/pull/352) (merge `f6fe242f`).
2. **Pruebas de software**: [`../2026-10-29-audio-capture-351/`](../2026-10-29-audio-capture-351/README.md).
3. **Validación en hardware**: este directorio.

Este documento no redefine el contrato de #351 ni promete audio audible: valida en la radio la
**copia de captura RX** que alimenta al grabador y a los decodificadores, y deja explícitos los
límites de lo medido.

## Qué se validó y qué no

- **Sí**: build y despliegue de la GUI de `main` con el fix de #351; captura continua; grabador de
  audio; grabador de pantalla con su pista de audio; presets de `Capture filters`; waterfall; CPU,
  crashlog y estabilidad de la GUI.
- **No**: audio audible del BASE (el parlante no pasa por esta cadena), RNNoise/Speex en vivo,
  decodificadores CW/RTTY/FT8 (H5), underruns forzados (H3), p99 del callback (sin instrumentación
  en este build) y variantes AM/Digital de la comparación objetiva MP3.
- **No hubo TX/PTT, ni flasheo de ROM, ni escrituras a registros del BASE, ni cambios de ruteo de
  audio.** El operador (owner) participó en las pruebas que requieren escucha y en el disparo del
  grabador de pantalla.

## Versión desplegada

| Campo | Valor |
|---|---|
| Base | `main` = `fc18a831a3db1e15f316be7d613152fcefc705ec` |
| GUI sha256 | `4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0` |
| GUI md5 | `80bbe5bd70b345e1e9a563d06d047a1c` (prefijo `80bbe5bd70b3` en `docs/PRUEBAS.md`) |
| Tamaño | 13.478.088 B (ELF32 ARM EABI5) |
| `boot_identity` | `42cd5c541237db86d23441505637f5cd` |
| Backup del binario previo | `ac2f6febb63b518b398779503d14f6993b721b21fb873cd630b8599e12f73d87` (fuera del repo, en `/mnt/ui-bak-191601/`) |

Detalle de build, tests, backup, rollback y verificación de arranque: [`deployment.md`](deployment.md).
Comandos y códigos de salida: [`commands.log`](commands.log).

## Veredicto H1–H9

| Ítem | Estado | Resultado |
|---|---|---|
| **H1** PCM a grabadores | **PASS** | AVI de 95,50 s con 764 frames @ 8 fps y pista PCM 44100 mono de 4.214.248 muestras (95,56 s); referencia `parec` de 4.585.904 muestras; correlación de envolventes 0,856, lag −0,28 s. |
| **H2** captura continua | **PASS** | 600 s de `parec`: **26.460.000 muestras** en 599,78 s de reloj; sin pérdida observable a nivel de total (resolución del método: 1 s). |
| **H3** hole → silencio | **NO EJECUTADO** | No se forzaron underruns (instrucción del owner). |
| **H4/C1/C2** presets vs parlante | **PASS** | Operador: **“no noté ningún cambio audible”** en Off → SSB voice → AM broadcast → Digital → Off. La copia sí cambia: Off rms 2023 → SSB rms 926 (−54,2 %); 3–6 kHz −12,4 dB. |
| **H5** decodificadores | **NO EJECUTADO** | Requiere señal CW/FT8 y cambio de modo; no disponible en la sesión. |
| **H6** AVI sincronizado | **PASS** | Video 95,50 s y audio 95,56 s: diferencia **0,06 s** (< 1 frame = 0,125 s); `ffprobe` sin errores; frame 100 con contenido. |
| **H7** MP3 íntegro | **PASS** (Off vs SSB; AM/Digital no ejecutados) | MP3 de **60,04 s**, 302.261 B, 40,3 kbps, reproducible; comparación objetiva de la copia Off vs SSB: rms 2023 → 926. |
| **H8** waterfall por preset | **PASS** | 5 capturas: std 14,71–15,15 y media 5,57–5,78; correlaciones 0,75–0,95 por evolución temporal; sin diferencia atribuible al preset. |
| **H9** CPU/pops/crashlog | **PARCIAL** | GUI **65,7 %** de un core con captura (p50 65,8 %, p95 71,0 %) y **63,4 %** baseline; **crashlog 0**; operador **sin pops/clicks**; **p99 del callback UNKNOWN**. |

Filas máquina-legibles, con método y evidencia de cada ítem: [`measurements.csv`](measurements.csv).

## Resultados y evidencia

- **H1/H6**: [`logs/h1-h6-ffprobe.txt`](logs/h1-h6-ffprobe.txt),
  [`logs/h1-h6-analysis.txt`](logs/h1-h6-analysis.txt),
  [`logs/h1-h6-watcher.log`](logs/h1-h6-watcher.log),
  [`captures/avi-frame-100.png`](captures/avi-frame-100.png).
- **H2/H9**: [`logs/h2-h9.log`](logs/h2-h9.log), [`logs/h2.json`](logs/h2.json),
  [`logs/h2-cpu.log`](logs/h2-cpu.log), [`logs/cpu-baseline-120s.log`](logs/cpu-baseline-120s.log).
- **H4/C1/C2**: [`logs/h4-operator-listening.txt`](logs/h4-operator-listening.txt),
  [`logs/h4-preset-transitions.csv`](logs/h4-preset-transitions.csv).
- **H7**: [`logs/h7-metadata.txt`](logs/h7-metadata.txt),
  [`logs/h7-off-10s.log`](logs/h7-off-10s.log), [`logs/h7-ssb-10s.log`](logs/h7-ssb-10s.log),
  [`logs/h7-clean2.log`](logs/h7-clean2.log).
- **H8**: [`logs/h8-waterfall-stats.txt`](logs/h8-waterfall-stats.txt),
  [`captures/waterfall-01-off.png`](captures/waterfall-01-off.png),
  [`captures/waterfall-02-ssb-voice.png`](captures/waterfall-02-ssb-voice.png),
  [`captures/waterfall-03-am-broadcast.png`](captures/waterfall-03-am-broadcast.png),
  [`captures/waterfall-04-digital.png`](captures/waterfall-04-digital.png),
  [`captures/waterfall-05-off-cierre.png`](captures/waterfall-05-off-cierre.png).
- **Deploy y UI**: [`logs/build.log`](logs/build.log), [`logs/native-tests.log`](logs/native-tests.log),
  [`logs/quick-tests.log`](logs/quick-tests.log), [`logs/deploy.txt`](logs/deploy.txt),
  [`logs/backup.txt`](logs/backup.txt), [`captures/settings-audio-capture-filters.png`](captures/settings-audio-capture-filters.png),
  [`captures/deploy-antes-home.png`](captures/deploy-antes-home.png),
  [`captures/deploy-despues-home.png`](captures/deploy-despues-home.png).
- **Estado final**: [`logs/runtime-state.txt`](logs/runtime-state.txt),
  [`captures/cierre-home-preset-off.png`](captures/cierre-home-preset-off.png).
- **PulseAudio (topología)**: [`logs/pulseaudio-topology.txt`](logs/pulseaudio-topology.txt).
- **Integridad de esta evidencia**: [`logs/SHA256SUMS`](logs/SHA256SUMS).

## Limitaciones que no se ocultan

1. **H3 no se ejecutó**: no se provocaron underruns; no se observó un hole natural.
2. **H5 no se ejecutó**: no había CW/FT8 en el aire durante la sesión.
3. **H7 AM/Digital no se ejecutaron**: el preset no se puede cambiar por control remoto; se
   demostró el contrato con Off vs SSB voice.
4. **p99 del callback: UNKNOWN**. Este build no instrumenta los tiempos del callback; sólo se midió
   CPU total del proceso GUI.
5. **`Capture filters` no toca el parlante**: la ausencia de cambio audible es lo esperado y no se
   declara mejora audible alguna.
6. **H8**: el waterfall evoluciona en el tiempo; las capturas no son idénticas píxel a píxel. Se
   comparó la distribución de la región del waterfall, no una identidad de imagen.
7. **Incidente de UI**: una vez el contenido central quedó en negro con Ajustes abierto; se reinició
   la GUI y no reapareció. No se determinó la causa; ver
   [`logs/ui-incident.txt`](logs/ui-incident.txt). El rollback está disponible.
8. **La pista de audio del AVI es la copia procesada por la GUI**, no el source crudo; por eso la
   correlación muestra-a-muestra con la referencia `parec` no es 1, aunque el conteo y la envolvente
   sí coinciden.

## Privacidad

- **No se publican** los MP3/AVI ni el audio crudo de RX: pueden contener QSO de terceros. Sólo se
  publican metadatos, hashes, capturas de UI/waterfall y análisis.
- No se publican credenciales, direcciones IP privadas ni el binario/backups de ejecutables; los
  backups quedan fuera del repositorio, en la radio.
- Las capturas autorizadas son de la UI del equipo y del waterfall; no incluyen audio.

## Reproducción

El procedimiento completo, con build, hashes, backup, rollback y verificación, está en
[`deployment.md`](deployment.md); los comandos y sus códigos de salida en
[`commands.log`](commands.log). El plan original del handoff sigue en
[`../2026-10-29-audio-capture-351/hardware-handoff.md`](../2026-10-29-audio-capture-351/hardware-handoff.md).
