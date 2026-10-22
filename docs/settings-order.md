# Ajustes: idioma y orden de los tabs (issue #109)

**Fecha**: 2026-10-16 · **Corrida**: `factory/settings-en-order-109` · **Issue**: #109 (orden del
owner: "traduce todos los settings a inglés de ham radio y ordenalos por utilidad en el uso del
equipo para hacer contactos").

Este documento es la fuente de verdad del **orden de los tabs** del diálogo de Ajustes
(`src/dialog_settings.c`) y del **porqué** de cada posición y de cada label. El manual del operador
(`docs/manual/MANUAL.md`, §5) describe lo mismo para el usuario final; acá está la decisión de
ingeniería.

## El orden final

La fila de abajo admite **5 botones**: 4 tabs más el botón de página (`docs/HALLAZGOS.md` 31). Con 8
grupos hay **dos páginas**.

| Página | Tabs (izquierda a derecha) |
|---|---|
| 1 | `Operation` · `Audio` · `Spectrum` · `Clock` · `More >` |
| 2 | `Display` · `Appearance` · `Actions` · `Info` · `< More` |

La grilla se arma en el mismo orden (el diálogo abre en su **primer** grupo, hoy `Operation`), y
`settings_goto()` lleva la vista y el foco al grupo del tab que se aprieta.

| # | Tab | Contenido real (filas del grupo) | Por qué está acá |
|---|---|---|---|
| 1 | **Operation** | Freq acceleration / Bluetooth, Discoverable (CAT), Devices, Charger / Cluster column / wfview server / Transverter | Es el grupo que trae la **aceleración del dial** (la perilla se toca en cada pasada de sintonía, dentro del contacto) y las filas de operación/estación (cluster, transverter, wfview). Es el único grupo del diálogo con algo que se usa *mientras* se opera. |
| 2 | **Audio** | Line-in, Line-out / Play,Rec gain / Audio enhance / Speaker mode / Voice mode, rate, pitch, volume | Las ganancias de entrada y salida y el realce de audio son el ajuste que más se retoca en un QSO (el volumen del equipo no está en Ajustes, ver issue #30). |
| 3 | **Spectrum** | Spectrum auto min, max / Waterfall auto min, max / Waterfall line, zoom / Waterfall smooth scroll / Waterfall battery mode | Es donde se ajusta la lectura del espectro y la cascada para **encontrar la señal**; se toca al buscar, no al hablar. |
| 4 | **Clock** | Day, Month, Year / Hour, Min, Sec / Clock view / Timeout Clock, Power, TX | La fecha/hora es la del **log** de contactos: se toca al operar (poner el equipo en hora), después de lo que hace al contacto en sí. |
| 5 | **Display** | Timeout, Brightness / Buttons brightness / S-meter needle / Band plan | Legibilidad (brillo, apagado automático, plan de banda) y la aguja del S-meter: ergonomía de la operación, no del contacto. |
| 6 | **Appearance** | Theme / Font | Estética (temas y familia de fuente, que se aplican al reiniciar). Se elige una vez. |
| 7 | **Actions** | GEN/APP/KEY/MSG/DFN/DFL long press / HMic F1/F2 press y long press | Asignación de teclas y pulsaciones largas: configuración, se hace una vez. |
| 8 | **Info** | Mag Freq, Info, ALC | Qué muestra cada zona de la barra superior. Un solo renglón y de uso ocasional: último. |

### Desvíos del esqueleto de la spec (y por qué)

La spec proponía `Operation → Audio → Spectrum → Clock → Display → Appearance → Actions → Info →
More`. El orden implementado es ese, con **un** desvío de lectura, no de orden:

- **No existe un grupo `More`.** En el firmware `More >` es *el botón de paginación*, no un tab con
  ajustes: la spec lo listaba como el grupo 9 ("lo eventual: voz, GPS, red, BT, transverter"), pero
  ese contenido no es un grupo — la voz está en **Audio**, el Bluetooth y el transverter en
  **Operation**, y GPS/red viven en las apps, no en Ajustes. Los ocho grupos de contenido son los de
  la tabla de arriba; la paginación (`More >` / `< More`) se recalcula sola a partir de las dos
  páginas.

### Riesgo considerado y descartado

El esqueleto de la spec suponía que `Operation` era "modo/banda/potencia/PTT". **No lo es**: en este
firmware eso vive en la pantalla principal (barra inferior y MFK), no en Ajustes. Aun así se
mantuvo `Operation` primero porque es el grupo que trae la aceleración del dial (uso continuo al
sintonizar) y el resto de las filas de la estación; moverlo detrás de `Audio`/`Spectrum` dejaba
primero dos grupos de solo-lectura y no cambiaba la utilidad percibida del primero. La alternativa
(`Audio` primero) se descartó por eso y por no desviarse del esqueleto pedido sin ganancia medida.

## El idioma: labels viejos → nuevos

Todos los **labels visibles** del diálogo quedan en inglés de ham radio. Inventario (grep de
acentos + español sobre `src/dialog_settings.c` y los diálogos que abre): lo único en español eran
los tabs y los dos botones de página; el resto de las filas ya estaba en inglés técnico y se
conserva.

| Viejo | Nuevo | Razón |
|---|---|---|
| `Reloj` | `Clock` | Término estándar del equipo (la pantalla se llama `CLOCK_POWER`/`CLOCK_TIME` en `src/clock.c`). |
| `Pantalla` | `Display` | Término estándar de UI/ham; "screen" sería el panel físico. |
| `Info` | `Info` | Ya correcto (abrevia "información de la barra superior"); no se toca. |
| `Audio` | `Audio` | Ya correcto y es el término ham estándar. |
| `Espectro` | `Spectrum` | Término estándar (el vocabulario del propio código: `spectrum.c`, `Spectrum auto min, max`). |
| `Operacion` | `Operation` | Traducción directa; sin acento en el original, igual que el resto de los labels ASCII. |
| `Apariencia` | `Appearance` | Traducción directa. |
| `Acciones` | `Actions` | Traducción directa (asignación de acciones a teclas). |
| `Mas >` | `More >` | Paginación a la página 2; `More` es el término estándar de UI. |
| `< Mas` | `< More` | Vuelta a la página 1. |

**Se conservan sin cambios** (ya son el término correcto): `S-meter`, `S-meter needle`, `Band plan`,
`ALC`, `ATU`, `ATT`, `PRE`, `VOX`-family (`Voice mode`), `RF`-family (`Freq acceleration`),
`Play,Rec gain`, `Line-in, Line-out`, `Audio enhance`, `Speaker mode`, `Waterfall ...`, `Discoverable
(CAT)`, `wfview server`, `Transverter`, `Theme`, `Font`, `Visual profile`, `HMic F1/F2 ...`,
`GEN/APP/KEY/MSG/DFN/DFL long press`, y la lista de acciones (`Screenshot`, `Screen rec`, `Mute`,
`ATU tune`, `APP ...`, `QTH Grid`, ...).

**No se tocan** (fuera de alcance por invariante y por el issue): los `LV_LOG_*`, los comentarios y
los identifiers del código, y el diccionario de voz de `src/voice_text.c` (los anuncios TTS ya
traducen al español nombres en inglés).

## Fixtures del arnés (la navegación es por índice)

Los tabs se aprietan por **posición** (`buttons_press(N)` en `tools/ui_harness/src/screens.c`), así
que el reordenamiento obligó a remapear cada fixture para que siga abriendo la **misma pantalla**.
El mapeo y su verificación están en la evidencia de la corrida; el resumen:

| Fixture | Antes | Ahora | Misma pantalla |
|---|---|---|---|
| `settings-home` | (sin press: abría en el grupo 1 = Clock) | `buttons_press(3)` = tab `Clock` de la página 1 | Sí: fila Day/Month/Year + reloj |
| `settings-wfbat` | `4` (`Mas >`) + `0` (`Espectro` de pág 2) | `2` = tab `Spectrum` de la página 1 | Sí: fila `Waterfall battery mode` |
| `settings-perfil` (+ `settings-tema-*`) | `4` (`Mas >`) + `2` (`Apariencia` de pág 2) | `4` (`More >`) + `1` (`Appearance` de pág 2) | Sí: fila `Visual profile (Eco)` |
| `settings-audio` | `3` = tab `Audio` de la página 1 | `1` = tab `Audio` de la página 1 | Sí: fila `Play,Rec gain` |

Cada uno de esos saltos tiene su **aserción** en `scenario_settle` (`label_on_screen`): si el tab
esperado no está en la barra, o la fila no queda en pantalla, el arnés **corta** en vez de capturar
otra cosa y reportar verde.

## Ver también

- `docs/manual/MANUAL.md` §5 — la versión para el operador.
- `docs/AB_TESTING.md` — el pipeline de render (las fixtures de Ajustes y sus goldens).
- `tools/ui_harness/README.md` — cómo correr el arnés y qué mide cada fixture.
