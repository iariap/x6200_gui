# Evidencia #133 — `swrscan` (Run/Stop del barrido de ROE por socket)

Fecha: 2026-10-17. Rama: `factory/swrscan-remote-133`. Commit de código: `aea54215`
(base `c55e8b99`). Build ARM: `ffef63df58cb`.

## TDD nativo (determinista, sin radio)

- `tdd-red.log`: la suite final (457 comprobaciones) contra un stub que no hace nada:
  **39 fallos**. Es el RED real de las guardas y del estado por `get`.
- `tdd-green.log`: el mismo binario con `dialog_swrscan_remote_run()` implementado:
  **457 comprobaciones, 0 fallos**.
- `tdd-green-clang-asan.log`: el mismo test compilado con **clang-18 + ASan + UBSan**:
  **457 comprobaciones, 0 fallos**, sin errores de sanitizer.

Casos que cubre (`tests/native/test_swrscan.c`, `test_swrscan_remote`):

1. app cerrada → rechaza sin TX ni movimiento del VFO;
2. otra app abierta → rechaza (la ventana del SWR Scan no es la actual);
3. app abierta → Run remoto arranca por el mismo camino que el botón (0.1 W, sesión #125);
4. doble Run → no-op (no reinicia ni re-transmite);
5. Stop con `RADIO_SWRSCAN` → apaga TX, vuelve a RX y restaura el VFO al centro;
6. Stop sin barrido → no-op;
7. sesión completa → `Completed` y `swrscan_tx:false`;
8. equipo en `RADIO_TX` por otra causa → no arranca;
9. gate de multipasada 3/5 → `Error` sin TX (el comando remoto no saltea el gate);
10. cerrar la ventana con el barrido corriendo (#85) y pedir Run después → sin crash, sin TX.

## Banco QEMU (humo, SIN TX)

`banco-swrscan-idle.png`: la GUI ARM real (`ffef63df58cb`) en QEMU, app `SWRScan` abierta por
socket. Muestra `Idle`, `sin datos medidos`, `Run`, `Span: 50 kHz`, `Passes: 1`.

`banco-get.json`: la línea de `get` en ese estado:

```json
"app":"SWRScan","ax":{"swrscan_state":"Idle","passes":1,"pass":0,"points":51,"swrscan_tx":false}
```

**No se envió `{"cmd":"swrscan","val":1}` en el banco**: el banco no tiene radio y la regla es
no iniciar TX sin el equipo. El Run real es `requires-radio` (procedimiento en el reporte).

`banco-run.log`: salida de `tools/captura-banco.sh` (binario verificado por el gate #86,
respuesta `{"t":"ok"}` al abrir, captura útil, sin `crash.txt`).

## Gates (`gates.log`)

- A/B `AB-SWR133` (A=`c55e8b99`, B=`aea54215`): **PASS**, 85 escenarios **pixel-idénticos**,
  873 contratos. El arnés maneja el diálogo por el botón real y no compila
  `remote_server.cpp`, por eso no hay cambio visual esperado.
- `contracts`: 873 checks, 0 fallos.
- `audit`: 11 issues conocidos, 0 reproducidos; 0 hallazgos estructurales sobre 9462 objetos.
- Determinismo del arnés: PASS (PNGs idénticos en las dos corridas de cada fixture).
- Cross-compile ARM: OK (log del build con `dialog_swrscan.c.o` y `remote_server.cpp.o`).

## requires-radio (pendiente, no ejecutado)

Ver el procedimiento exacto en el reporte del issue. Resumen: abrir `SWRScan` por socket,
`{"cmd":"swrscan","val":1}`, esperar en `get` a que `ax.swrscan_state` sea
`Completed`/`Stopped`/`Error` con `ax.swrscan_tx:false`, capturar la pantalla y probar
`{"cmd":"swrscan","val":0}` **durante** el barrido para confirmar que corta y restaura
potencia/modo. Sólo con autorización del owner, antena o carga ficticia, en RX y QRP (0.1 W).
