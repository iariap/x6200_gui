# Inventario de clientes /dueño de `/dev/i2c-0` y escritores del registro 12

| actor | ¿abre /dev/i2c-0? | escribe registro 12 | notas |
|---|---|---|---|
| `x6200_gui` (libaether `refs/X6200Control/src/low/control.c`) | **sí** (único proceso, medido en #363/#367) | **sí**: `x6200_control_cmd`, `record_set`, `radio_thread` idle | `x6200_control_idle` reenvía 54 regs/218 B cada ≤1000 ms desde sombra; `get`=sombra RAM |
| `x6200_route_arbiter` (este PR, DEFAULT OFF) | **sí cuando se habilita** (transporte) | **sí, único lógico** (cuando la feature está ON) | en OFF no abre I2C ni se lanza |
| `x6200_route_supervisor` (este PR) | **no** | **no** (prohibido: evita race) | pide restore al árbitro por IPC; si no responde → FAULT |
| `x6200_daemon` | no | no | supervisor de GUI (backoff 2/5/15/60 s); no lanza árbitro en este PR |
| otros (btaudio-ctl, ALSA, GUI apps) | no | no | A2DP/loopback son streams PulseAudio, no I2C |
| MCU BASE | n/a | recibe escrituras y aplica (voice_rec handler) | no hay readback del reg 12 (R′ #370) |

## Contrato de dueño único (cuando ON)

1. Sólo `x6200_route_arbiter` escribe el registro 12; GUI/libaether envían `REQ_APPLY_SHARED`/ruta por
   IPC y reciben respuesta/errores; `x6200_control_idle` deja de escribir directo (proxy).
2. `apply_shared` nunca acepta `voice_rec=0x8` desde la GUI (ese bit es del árbitro).
3. Bits compartidos (PTT 0x40000, ATU 0x1000/0x2000, modem 0x4000, barrido 0x10, poweroff 0x10000) se
   preservan; si la GUI escribe bits de TX, el árbitro deshabilita RX Linux antes.
4. Error/restart/ownership loss ⇒ sombra inválida ⇒ RX_LINUX bloqueado; nunca se reconstruyen bits por
   suposición.

## Inventario de escritores del registro 12 (libaether) y cobertura del proxy

Todas escriben `x6200_sple_atue_trx` (idx 12) con RMW sobre `x6200_control_get()` (sombra RAM):

| API libaether | bits que toca | ruta |
|---|---|---|
| `x6200_control_record_set(bool)` | `voice_rec 0x8` | **proxy RECHAZA** (`OWNERSHIP`): dueño = árbitro |
| `x6200_control_ptt_set(bool)` | `iptt 0x40000` | proxy `ptt_set` (TX_PREPARE + APPLY_SHARED) |
| `x6200_control_atu_tune(bool)` | `atu_tune 0x2000` (+ `host_cmd 0x8004/0x8003`) | proxy reg12; **host_cmd queda directo** (no toca reg12) |
| `x6200_control_atu_set(bool)` | `atue 0x1000` | proxy `atu_set` |
| `x6200_control_modem_set(bool)` | `modem_trx 0x4000` | proxy `modem_set` |
| `x6200_control_swrscan_set(bool)` | `swrscan_trx 0x10` | proxy `swrscan_set` |
| `x6200_control_split_set(bool)` | `sple 0x2` | proxy `split_set` |
| `x6200_control_poweroff()` | `power_off 0x10000` (+ `kill(1,SIGUSR2)`) | proxy reg12; señal queda directa fuera de reg12 |
| `x6200_control_calibration_set(bool)` | `calibration 0x8000` | **no wrapper específico**: se enruta por `x6200_control_cmd(12, RMW)` (call-site a reemplazar) |
| `x6200_control_cmd(x6200_sple_atue_trx, val)` genérico | cualquiera | call-site genérico a reemplazar por `route_proxy_write_reg12()` |
| `x6200_control_idle()` | reescribe 54 regs/218 B | debe redirigirse al árbitro (proxy total) |
| `x6200_control_host_cmd(0xfffe/…)` | canal host (NO reg12) | directo por diseño (no comparte registro) |

## Cobertura actual del proxy (`route_proxy.c`, test 88 checks)

- `write_reg12`, `ptt_set`, `atu_set`, `modem_set`, `swrscan_set`, `split_set`, `poweroff` → IPC `TX_PREPARE` (si bit TX) + `APPLY_SHARED`; orden y timeout verificados.
- `record_set` → `OWNERSHIP` (nunca toca 0x8).
- Lectura `get_reg12` = caché del árbitro (no readback MCU), invalidada en error/timeout.
- **Pendiente mecánico**: reemplazar los call-sites de libaether/GUI (`radio.c`, `audio.c`, `dialog_*`) por el proxy y redirigir `x6200_control_idle`; sin eso, la GUI sigue escribiendo directo y **RX_LINUX permanece bloqueado**.

## Conexiones /dev/i2c-0

Sólo `libaether` (proceso GUI) en la base; el servicio `x6200_route_service --arbiter` sería el único
cliente cuando la feature ON y el proxy total estén cableados. Supervisor no abre I2C. No hay otras
conexiones (A2DP/ALSA son streams, no I2C).
