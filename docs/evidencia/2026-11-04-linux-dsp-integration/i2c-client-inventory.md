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
