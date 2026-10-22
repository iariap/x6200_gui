# Verificacion en la radio del PR 3 de #173 (rueda / sin botones -/+)

- Rama `lu2dsx/x6200-rom`, commit **`7a5ce9d7`** ("DFL: la rueda opera el filtro (rotary_cb) y se
  sacan los botones +/- (#177)"), correccion pedida por el owner sobre el PR 2.
- Build compilado y desplegado por este rol: **`c8eda34bac1a1ab71bd0af45e95e3489`**, gate del #86 OK
  (ELF32 ARM, 13.870.048 B, 3/3 marcas), GUI relanzada por el daemon, equipo sin reiniciar.

## Verificado

| Item | Resultado | Evidencia |
|---|---|---|
| Editor abre (5.a celda de la pagina DFL) | si | `01-editor-rueda.png` |
| Botones **-/+** | **ya no estan** (filas LOW/HIGH/BW a todo el ancho) | `01-editor-rueda.png` |
| Fila seleccionada resaltada | si: **LOW** resaltada con los roles del tema (seleccion inicial LOW en LSB) | `01-editor-rueda.png` |
| Valores | LOW 0 Hz / HIGH 2000 Hz / BW 2000 Hz (LSB, 40 m) | `01-editor-rueda.png` |
| Texto del BW en el espectro | "2.0k" (exacto) | `01-editor-rueda.png` |
| Cerrar (5.a celda "Cerrar DFL") | cierra y vuelve a la pantalla principal | `02-cerrado.png` |
| RTTY / CW / Settings abren y cierran | **PID 12989 estable**, sin crash | medicion abajo |
| Barrido de 23 apps | 21 ok + **3 "REINICIO" que fueron FALSOS POSITIVOS** | ver abajo |
| `verifica-radio` | `TODO OK` | — |

## Ojo: falsos positivos del barrido (no es un bug del firmware)

En el barrido, `RTTY`, `CW` y `Settings` salieron como "REINICIO (dejo de responder)". **No hubo ningun
reinicio**:

- `prueba-apps-radio.sh` llama `uptime_radio()` que va **por SSH**; si ese SSH devuelve vacio, el
  script lo interpreta como "dejo de responder" y lo rotula **REINICIO** (`tools/prueba-apps-radio.sh:72-100`).
- El **PID de la GUI nunca cambio** (12989 antes y despues) y **no aparecio ningun log guardado nuevo**
  (689 → 690 solo por el deploy, no por el barrido) ni **CRASHLOG**.
- Reproduccion dirigida: abrir **RTTY, CW y Settings** 14 s cada uno → `{"t":"ok"}` al abrir y cerrar,
  **PID 12989 estable** en las tres.
- Red medida aparte: **20/20 pings** y **10/10 `get`** por socket, sin fallos.

Conclusion: fueron **3 fallos transitorios de SSH** (el mismo tipo de flakiness que motivo el #172),
**no** crashes ni reinicios. El build `c8eda34b` esta sano.
