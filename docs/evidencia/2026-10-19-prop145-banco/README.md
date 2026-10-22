# Banco QEMU de la app Propagación (#145)

Corrida del banco sin radio (gate #86): el **binario ARM real** de este commit
(md5 `61faf46a4e60`, 13.828.248 B, `tools/verifica-firmware-port.py` OK) corre
con `qemu-arm-static` dentro de un chroot del rootfs y framebuffer falso, y se lo
comanda por su socket de control. No transmite y no necesita radio.

```
CHROOT=/dev/shm/prop145-chroot VOLUMEN=x6200-build-output-prop145 \
    docs/evidencia/2026-10-19-prop145-banco/correr.sh
```

El **chroot es una copia privada** (`/dev/shm/prop145-chroot`, 2,2 GB) del
`.vfix3` compartido: la primera corrida falló a mitad del barrido porque **otra
corrida de la fábrica** levantó su propio `banco-apps` sobre el mismo `.vfix3` y
reemplazó el binario del chroot (en `out-banco.txt` quedó un arranque con **otro**
`exe_md5=4893b8f3…`). Con la copia privada la corrida es reproducible. El volumen
de build también es el **dedicado** (`x6200-build-output-prop145`): el compartido
lo pisan otras corridas (el binario cambió a las 13:10 con otro build).

## Qué se midió

| Verificación (§7) | Resultado | Evidencia |
|---|---|---|
| La app abre desde la lista/socket | **OK** | `app=Propagation` en `estado.log`; `01-abrir.png` |
| Run: barrido de 4 pasos | **OK** (5 beacons, 42 s) | `02a-sincronizando.png`, `02-midiendo.png`, `03-resultado.png` |
| Cancelar | **OK** | `04-cancelado.png` (`state=Cancelled`) |
| No transmite | **OK: 50/50 sondeos `tx=False`, 0 `tx=True`** | `estado.log` |
| GUI responsive durante el barrido | **OK**: volumen 30 → 37 (pedido 37) con `state=Measuring` | `estado.log` |
| VFO restaurado al cerrar | **OK**: `freq=14030000 mode=LSB` antes y después | `estado.log`; `05-cerrada.png` |
| Sin crashes | **OK: `CRASHLOG: 0`**, sin `crash.txt` | `out-banco.txt` |
| Cero consumo con la app cerrada | **por construcción**: el único timer (1 s) se crea en `start_sweep()` y se borra en `sweep_ui_end()`/`destruct_cb()`; con la app cerrada no queda timer ni hilo | `src/dialog_propagation.c` |

### Consumo en reposo

Los cambios de contexto (`voluntary_ctxt_switches`) del proceso, medidos en
ventanas de 20 s, son **ruido dominado por el lazo de refresco normal de la GUI**
(~1700–2000 wakeups/s en todas las fases, con o sin app): cerrada +3808, abierta
en reposo +4271, cerrada de nuevo +4926. La contribución del timer de 1 s de la
app (20 wakeups en 20 s, ~0,5 %) **no se puede aislar** con esta instrumentación,
así que **no se declara como medición**: la garantía de "cero consumo en reposo"
es por construcción (no hay timer fuera del barrido). El `wus` de LASTBREATH **no
es un contador de wakeups** (es el microsegundo de la escritura anterior del
latido), así que no se usa como prueba.

## Pendientes de radio (`requires-radio`)

- **NTP vs "transmitiendo ahora"**: en el banco el reloj no está sincronizado y
  no hay radio; hay que comparar el indicativo del paso 2/4 contra un reloj
  externo en el equipo.
- **SNR real y escalera**: no hay RF en el banco (los 5 beacons se miden con
  `SNR -1`, `heard=0`, veredicto `CERRADA`). Falta validar el formato real de los
  dashes (ventanas fijas de la v1) y el piso de ruido a +5 kHz.
- **Restauración del VFO por hardware**: el banco verifica el estado que ve la
  GUI (`freq`/`mode` del `get`); falta confirmar el hardware por CAT/socket en la
  radio.
- **Encoders durante el barrido**: el banco prueba que la GUI atiende comandos
  (volumen) y sigue dibujando durante el barrido; falta girar los encoders
  físicos.

## Archivos

- `correr.sh` — guion de la corrida (idempotente).
- `00-home.png` … `05-cerrada.png` — capturas del framebuffer.
- `estado.log` — la secuencia de `get` (app, freq, mode, tx, estado del barrido,
  sondeo del `tx` y la prueba de volumen).
- `wakeups.log` — cambios de contexto por fase.
- `lastbreath.log` — LASTBREATH y uptime antes y después.
- `out-banco.txt` — log de la GUI (incluye `CRASHLOG-MOD-INIT` y el `CRASHLOG: 0`).
