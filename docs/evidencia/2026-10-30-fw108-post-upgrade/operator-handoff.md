# Handoff del operador — retest de la imagen nueva #378

Esta etapa la ejecuta el **propietario**. El agente no escribe la SD, no flashea
firmware y no transmite.

## Antes de empezar

- Radio apagada y alimentación segura.
- **SD OEM 1.0.8** independiente y funcional (rollback).
- **SD de prueba** con la imagen nueva, escrita y verificada por SHA256.
- No hace falta tocar el firmware BASE/MCU: ya está en 1.0.8.

## Imagen a probar

- Archivo: `sdcard.img` de esta etapa.
- SHA256: `1e3a9748754a758d4ac83aab673812a7313371a48ec0793884d2f66717e2339b`
- Tamaño: 571.473.920 B
- Layout: MBR, p1 FAT32 32 MiB, p2 ext4 512 MiB, **sin p3**.
- La p3 DATA la crea el primer arranque con el `S01create_data` corregido
  (sha256 `da7d19b13d853cbe6e764f60b62194203b704cb595e5f0f3837a3b8f398c6963`).

## Retest (2–3 minutos, sólo RX)

1. Insertar la SD de prueba y encender.
2. Confirmar que la GUI arranca y se mantiene operativa (sin
   `STARTUP DELAYED`).
3. Girar el dial/encoder, cambiar de banda/modo, y comprobar que el waterfall
   se actualiza y el S-meter/batería responden.
4. Escuchar el audio RX original; comprobar teclas y estado de batería.
5. **No transmitir**, no pulsar PTT, no usar ATU, no activar MUX/macros TX.
6. Al primer fallo relevante: apagar, retirar la SD de prueba, volver a la SD
   OEM y reportar `NO_GO` con lo observado.

## Al terminar

- Apagar y retirar la SD de prueba.
- Dejar la SD OEM puesta si no se coordina otra sesión.
- Reportar: arrancó sí/no, waterfall sí/no, audio sí/no, dial/teclas, S-meter,
  batería, cualquier crash/calentamiento.
