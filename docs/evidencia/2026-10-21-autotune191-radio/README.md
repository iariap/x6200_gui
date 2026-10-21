# Auto Tune v2 (#190 / PR #191) verificado en la radio — alcance RX

- Rama `lu2dsx/x6200-rom`, merge **`e6925db6`** ("[RX][CW] Auto Tune v2: safe one-shot CW fine tuning (#191)").
- Build en la radio: **`81d087bdf92239a8f23b4883cfc7e102`** (deploy del otro agente).
- Radio: 1 GUI, `/mnt` ext4 `rw`, uptime continuo (sin reinicios del equipo), sin TX.

## Verificado por control remoto

| Caso | Esperado | Medido |
|---|---|---|
| El action existe y se puede pulsar | `ACTION_ATUNE` en **APP 6:6** (celda 4 "Auto Tune") | ✔ `01-pagina6-menu.png` |
| **En CW sin señal**: muestra el estado y no inventa corrección | banner **"Auto Tune: tuning…"** y, sin señal confiable, **no toca el dial** | ✔ `03-cw-autotune-inicio.png` + a los 3 s la radio sigue en **7.030.000 CW** (`04-cw-autotune-3s.png`) |
| **Fuera de CW (LSB)**: no mueve el dial | dial intacto | ✔ 7.082.988 LSB antes y después (`02-autotune-lsb.png`) |

## No medido (y por qué)

- **La corrección con señal CW real** (que centre el tono respecto del pitch): requiere una señal de CW
  estable en RX; en 40 m a esta hora no hay señal y este rol no transmite. Es el caso que valida el
  lazo completo (FFT + mediana + una escritura + verificación).
- **El mensaje de rechazo fuera de CW**: en la captura de LSB (1.5 s después) no se ve banner; puede ser
  un aviso muy breve. No lo cuento como hallazgo: no lo medí con el tiempo adecuado.

## Observacion aparte (no atribuida)

Durante esta sesion la GUI se reinicio una vez (pid 4318 -> 5938) **sin CRASHLOG** (log guardado
`x6200_log_2026-10-21_18-11-05.txt`, corte a mitad de linea = kill, no excepcion) y el socket remoto
quedo unos segundos en `ECONNREFUSED` hasta que la nueva instancia lo re-bindeo. No puedo atribuir la
causa (puede ser un relanzamiento por deploy del otro rol o el watchdog del daemon); queda anotado por si
suma al evento "radio inalcanzable 90 s" del ciclo 272.

## Estado dejado

7.082.988 kHz LSB, BW 2800, pagina GEN, RX, sin TX.
