# Evidencia #133 — comando remoto `swrscan` en el banco QEMU

Fecha: 2026-10-18. Rama de evidencia: `factory/swrscan-remote-133` (rebasada sobre
`origin/lu2dsx/x6200-rom`). HEAD: `efae955e` (`QA: la radio sigue caida 3h despues de la cuarta caida`).
El código del comando ya está en el trunk: `d3e84ed` (PR #135, squash de `aea54215`).

**Alcance:** sólo evidencia. No se modificó código de producto, tests ni herramientas.

## Método

1. `docker ps` antes de empezar: sin contenedor `banco-apps`; imagen `x6200-buildroot` disponible.
2. Build **limpio** con el procedimiento del banco y el gate #86:

   ```sh
   RECONSTRUIR=si \
   CHROOT=/home/pablo/desa/x6200/.vfix3 \
   BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot \
   tools/captura-banco.sh ... '{"cmd":"app","text":"off"}'
   ```

   Salida cruda: `build-gate86.log`. El build hizo `dirclean` + rebuild, pasó el gate #86
   (`OK: ... es el firmware del port (ELF32 ARM, 13757724 B, 3/3 marcas)`), commit `efae955e`,
   fuente `e59f334d4b87`, binario **`ffef63df58cb854a5e63bcf88f645594`**.

3. **Nota de integridad del banco:** mientras se preparaba la corrida, otro proceso del host
   reemplazó el binario del chroot compartido `.vfix3` por otro distinto
   (`5f5bfb3b9381aa7ccfdeb1919b0cee46`, 13.762.952 B) y recreó el contenedor `banco-apps`.
   Para no atribuir la evidencia a un binario ajeno, la secuencia final se corrió sobre una
   **copia privada del mismo rootfs** (`/tmp/opencode/vfix3-swr133`) que contiene el binario
   recién construido y verificado. Se volvió a pasar el gate #86 sobre esa copia y se comprobó
   el md5 **antes y después** de la secuencia: `ffef63df58cb854a5e63bcf88f645594` en los dos
   extremos (`binario-y-gate86.txt`). El build se hizo con el `CHROOT`/`BUILDROOT` pedidos.

4. Secuencia por socket, sin radio, con la GUI ARM real bajo `qemu-arm-static` y framebuffer
   falso. Cada pedido y cada respuesta cruda quedan en `02-secuencia-completa.txt`; las capturas
   se rotaron 270° igual que `tools/captura-banco.sh`. El banco no tiene radio ni flow: no se emite
   RF y no se puede medir potencia entregada.

5. `docker rm -f banco-apps banco-apps-swr133` al terminar (`docker-ps-final.txt`).

## Tabla pedido → respuesta → estado

Las respuestas `{"t":"state",...}` están recortadas en la tabla (se muestran los campos que
importan); el JSON completo, con todos los campos, está en `02-secuencia-completa.txt`.

| # | Pedido | Respuesta cruda | Estado observado (`get`) |
|---|---|---|---|
| 1 | `{"cmd":"get"}` (baseline) | `{"t":"state",...,"mode":"LSB-D","tx":false,"app":"","ax":{}}` | app cerrada, `tx:false` |
| 2 | `{"cmd":"swrscan","val":1}` con la app cerrada | `{"t":"err","err":"app not open"}` | sigue `app:""`, `ax:{}`, `tx:false` (no hubo TX) |
| 3 | `{"cmd":"app","text":"SWRScan"}` | `{"t":"ok"}` | `app:"SWRScan"`, `ax:{"swrscan_state":"Idle","passes":1,"pass":0,"points":51,"swrscan_tx":false}`, `tx:false` |
| 4 | `{"cmd":"swrscan","val":1}` (Run) | `{"t":"ok"}` | +0,5 s: `mode:"LSB"` (antes `LSB-D`), `filter:2670`, `tx:true`, `ax:{"swrscan_state":"Scanning","passes":1,"pass":1,"points":51,"swrscan_tx":true}` |
| 5 | `{"cmd":"swrscan","val":1}` (doble Run durante `Scanning`) | `{"t":"ok"}` | sigue `Scanning`, `pass:1`, `tx:true`; no pasó a `Stopped` ni reinició |
| 6 | `{"cmd":"get"}` cada 1 s, 10 veces | 10 × `{"t":"state",...,"ax":{"swrscan_state":"Scanning","passes":1,"pass":1,"points":51,"swrscan_tx":true}}` | **se quedó en `Scanning`/`tx:true` los 10 s; no hubo `Error: timeout de RF` ni `Completed`** |
| 7 | `{"cmd":"swrscan","val":0}` (Stop) | `{"t":"ok"}` | +1 s: `mode:"LSB-D"` y `filter:2800` restaurados, `tx:false`, `ax:{"swrscan_state":"Stopped","passes":1,"pass":0,"points":51,"swrscan_tx":false}`; botón de la app: `Run` |
| 8 | `{"cmd":"swrscan","val":2}` (valor inválido) | `{"t":"err","err":"Invalid value"}` | sigue `Stopped`, `tx:false`; no se encoló nada |
| 9 | `{"cmd":"app","text":"off"}` (cierre) | `{"t":"ok"}` | — |

## Capturas

- `01-app-abierta.png`: la app `SWRScan` abierta en el banco, `Idle`, `sin datos medidos`,
  `Run`, `Span: 50 kHz`, `Passes: 1`.
- `03-run-scanning.png`: durante el Run, `Scanning pass 1/1 p 1/51`, botón `Stop`, barra en `LSB`.
- `04-post-poll-10s.png`: **10 s después**, todavía `Scanning pass 1/1 p 1/51`; el estado por
  socket sigue `Scanning` y `swrscan_tx:true`.
- `05-stopped.png`: después de `val:0`, la barra volvió a `LSB-D` y el botón a `Run`; el `get`
  dice `Stopped`/`tx:false`.
- `06-final.png`: después de `val:2` (sigue `Stopped`).

## Por qué no se vio el timeout en QEMU

En el banco no hay BASE/radio. `radio_init()` reintenta `x6200_control_init()` 30 veces y, si no
responde, **retorna antes de crear el hilo de radio** (`src/radio.c:447-457`; el
`pthread_create(radio_thread)` está en `src/radio.c:582`). El log lo dice:

```
[Error] radio_init: La BASE no respondió: la GUI sigue sin control de la radio (in radio.c line #456)
```

El watchdog del barrido (`dialog_swrscan_poll()`) se ejecuta **desde `radio_thread`/`radio_tick`**,
en la rama en la que el flow no entrega paquetes (`src/radio.c:268-272`). Sin hilo de radio no hay
quien llame al poll, así que la sesión queda en `Scanning` con `swrscan_tx:true` y no dispara el
timeout de `SWRSCAN_PASS_TIMEOUT_MS` (2.285 ms, `src/dialog_swrscan.h:40`). Esto es una **limitación
del banco sin radio**, no una conclusión sobre hardware: el camino de timeout está cubierto por el
test nativo `test_swrscan_timeout` (`tests/native/test_swrscan.c:813-842`) y por
`test_swr_session`; en la radio real el hilo de radio existe y recibe el flow.

El comando en sí **sí cumple el diseño** en lo que el banco puede medir:

- **Run** cambia el estado a `Scanning`, pone `swrscan_tx:true` y fuerza `LSB` (el `get` lo muestra).
- **Doble Run** responde `{"t":"ok"}` y **no reinicia ni para**: el estado sigue `Scanning` con
  `pass:1` y `tx:true`. Si hubiera llegado al callback del botón con `run=true`, ese callback hace
  `swrscan_stop()`; el hecho de que siga `Scanning`/`tx:true` prueba que fue un no-op por la guarda
  de idempotencia del comando remoto. La captura `04-post-poll-10s.png`, tomada 10 s después del
  doble Run, sigue mostrando el botón `Stop` (no volvió a `Run`).
- **Stop** responde `{"t":"ok"}`, deja `Stopped`, apaga `swrscan_tx` y restaura modo/filtro
  (`LSB-D`, 2800) y el botón `Run`. Las guardas de app cerrada y de valor inválido también responden
  como está especificado.

## Hallazgo colateral (UI, no del comando)

En `05-stopped.png`, después del Stop, el `get` ya dice `Stopped` y el botón dice `Run`, pero la
leyenda del chart **sigue mostrando `Scanning pass 1/1 p 1/51`**: `swrscan_stop()` no envía
`LV_EVENT_REFRESH` al chart (`src/dialog_swrscan.c:256-269`) y el camino del botón Stop tampoco
(`dialog_swrscan_run_cb`, `src/dialog_swrscan.c:681-688`). Es un defecto de repintado del camino
Stop, preexistente al comando remoto (el botón usa el mismo `swrscan_stop()`). No se tocó código:
queda documentado para que el owner decida si va como issue aparte. El estado lógico y el TX sí
quedan correctos.

## Archivos

- `build-gate86.log` — build limpio + gate #86 (salida cruda).
- `binario-y-gate86.txt` — md5, tamaño, gate #86 y HEAD del binario que corrió.
- `02-secuencia-completa.txt` — todos los pedidos y respuestas crudas, con timestamps.
- `banco-out.txt` — log completo de la GUI en el banco (746 líneas).
- `banco-out-relevante.txt` — `radio_init`, `CRASHLOG`, `crash.txt`.
- `01-app-abierta.png`, `03-run-scanning.png`, `04-post-poll-10s.png`, `05-stopped.png`,
  `06-final.png` — capturas.
