# Spec — #133: Run/Stop del barrido de ROE por control remoto (`swrscan`)

Fecha: 2026-10-17. Rama: `factory/swrscan-remote-133` (base `c55e8b99` = trunk tip).
Issue: [iariap/x6200_gui#133](https://github.com/iariap/x6200_gui/issues/133).

## Problema

La app `SWRScan` se abre por el socket desde el #78
(`{"cmd":"app","text":"SWRScan"}` → `{"t":"ok"}`), pero **no había forma de iniciar el
barrido**: el `Run` del diálogo (`dialog_swrscan_run_cb`) es local y `{"cmd":"btn","val":N}`
sólo aprieta los botones 0..4 de la pantalla principal. El QA no podía medir TX/ROE por el
ciclo automático y tenía que pedirle al owner que apretara `Run` en la pantalla.

## Contrato del socket

Con la app **SWRScan abierta**:

| Pedido | Efecto | Respuesta |
|---|---|---|
| `{"cmd":"swrscan","val":1}` | **Run**: arranca la misma sesión que el botón (0.1 W, LSB, restore #94/#102, sesión #125) | `{"t":"ok"}` |
| `{"cmd":"swrscan","val":0}` | **Stop**: detiene el barrido y restaura radio/VFO/memoria por el mismo camino que el botón | `{"t":"ok"}` |
| `{"cmd":"swrscan"}` / `val` fuera de 0..1 | no se ejecuta | `{"t":"err","err":"Invalid value"}` |
| con la app cerrada (u otra app abierta) | no se ejecuta, **sin TX** | `{"t":"err","err":"app not open"}` |

El comando entra por `app_cmds[]` (`src/remote_server.cpp`) y se aplica en el hilo de LVGL a
través del scheduler, como el resto. La respuesta `ok` significa "aceptado", no "terminado":
el resultado se lee del estado (`get`).

### Estado para `{"cmd":"get"}`

Mientras la app está abierta, el campo `ax` de la línea de estado incluye:

```json
{"swrscan_state":"Scanning","passes":1,"pass":1,"points":51,"swrscan_tx":true}
```

- `swrscan_state`: `Idle` | `Scanning` | `Completed` | `Stopped` | `Error` (token estable; el
  texto humano con el detalle sigue en el readout del chart).
- `passes`: pasadas pedidas en el diálogo (1 = Quick; 3/5 están gateadas, ver abajo).
- `pass`: pasada actual (1-based; 0 si no está barriendo).
- `points`: puntos de la grilla (51 fijos, #125; también en `Idle`).
- `swrscan_tx`: el TX del barrido está encendido (lo que el QA espera que vuelva a `false`).

Ejemplo de espera del ciclo automático:

```sh
/tmp/x6200-remote '{"cmd":"app","text":"SWRScan"}'
/tmp/x6200-remote '{"cmd":"swrscan","val":1}'
# repetir get hasta que ax.swrscan_state sea Completed/Stopped/Error y swrscan_tx=false
/tmp/x6200-remote '{"cmd":"get"}'
```

## Diseño y guardas (por qué es seguro)

El comando **no duplica** la lógica del barrido: la función pública nueva
`dialog_swrscan_remote_run(bool start)` (`src/dialog_swrscan.[ch]`) llama al **mismo callback
del botón** (`dialog_swrscan_run_cb(&btn_run)`). Así el camino de potencia/modo/restore
(`radio_start_swrscan`/`radio_stop_swrscan`, #94/#102) y el de la sesión (`swr_session`, #125)
es exactamente el del operador.

Guardas duras, en este orden:

1. **Sólo con la ventana del SWR Scan abierta**: `dialog_type_is_run(&dialog)`. Si la app se
   cerró entre el pedido y la ejecución (el scheduler la aplica después), devuelve `false` sin
   tocar la radio ni el chart. El servidor además valida `app_open()` antes de encolar; la
   guarda del diálogo cubre la carrera.
2. **Idempotencia**: `start == run` es un no-op que devuelve `true`. Run con el barrido
   corriendo **no** reinicia la sesión ni re-transmite; Stop sin barrido tampoco hace nada.
   Esta comparación va **antes** del chequeo de estado de radio, porque durante el barrido el
   equipo está en `RADIO_SWRSCAN` (no en RX).
3. **Arrancar exige `RADIO_RX`**: nunca se inicia el TX del barrido sobre una TX ajena. Stop
   **no** tiene esa guarda: tiene que funcionar transmitiendo (el caso que `btn` no alcanzaba,
   hallazgo del #119).
4. **Gate del diálogo intacto**: si la multipasada 3/5 está pedida (gate
   `SWRSCAN_MULTIPASS_TX_ENABLED = 0`), o la grilla está fuera de la banda de TX, el callback
   deja el estado en `Error` y no hay TX. El comando remoto no saltea el gate.

En `app_cmds[]` la fila es `{"swrscan", "SWRScan", dialog_swrscan_remote, 0, 1, 2}`. El
`stop = 2` es deliberado: `is_stop()` acepta `val < stop`, así que **Run (1) y Stop (0) se
aceptan también mientras la radio transmite**. Sin eso, un segundo Run durante el barrido se
rechazaría con `"Radio is transmitting"` en vez de ser el no-op idempotente; y Stop quedaría
sujeto al mismo bug que sufría `btn` (#119). El diálogo es el que decide si hay TX que hacer.

## Seguridad (el comando TRANSMITE)

`swrscan` con `val:1` **inicia una transmisión real a 0.1 W** (el piso del equipo; no se sube
la potencia, no se toca RF/DSP). Reglas de uso:

- **Sólo con autorización explícita del owner** y con la radio **en RX**, antena o carga
  ficticia conectada.
- El QA **no** usa este comando para pruebas de RX: es el único comando de app que arranca TX
  sin que el operador toque la pantalla.
- El presupuesto de TX del #117 (una pasada = 51 × 35 ms = 1785 ms) y la multipasada gateada
  del #125 **no cambian**.
- Stop (`val:0`) es siempre seguro y debe ser la primera acción si algo no cierra.

## Verificación

- **Nativo determinista** (`tests/native/test_swrscan.c`, `test_swrscan_remote`): app abierta →
  arranca; app cerrada u otra app abierta → rechaza sin TX; doble Run → idempotente; Stop con
  `RADIO_SWRSCAN` → detiene, apaga TX y restaura el VFO; equipo en TX ajena → no arranca; gate
  de multipasada → `Error` sin TX; cerrar la ventana con el barrido corriendo (#85) y pedir Run
  después → sin crash y sin TX. RED→GREEN en el reporte del issue.
- **A/B `AB-SWR133`**: sólo cambian `src/dialog_swrscan.c`, `src/remote_server.cpp` (no lo
  compila el arnés) y los tests. El arnés maneja el diálogo por el botón real, no por el
  comando remoto, así que las capturas deben quedar **pixel-idénticas**; se declara.
- **requires-radio** (procedimiento exacto en el reporte del issue): abrir la app por socket,
  `swrscan val:1`, esperar `Completed`/`Stopped` en `get`, capturar la pantalla, y probar
  `swrscan val:0` durante el barrido. Sin este paso no se puede afirmar nada sobre la ROE
  medida ni sobre la restauración de potencia/modo en el equipo.
