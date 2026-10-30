# Cableado S/P — pinout verificado, reglas y advertencias

**Fuente normativa:** manual Xiegu X6200 **Rev.05**, “Interface Description → Interface definitions”
(página impresa 8; PDF p. 12): el dibujo del conector S/P etiqueta **AUDIO en la punta (tip)**, **NC en el
anillo (ring)** y **GND en el manguito (sleeve)**; la tabla de definiciones dice `AUDIO = Audio output`,
`NC = Empty`, `GND = Grounding`. El `sha256` del PDF público usado está en `commands.log`. **El dibujo hay
que contrastarlo con la revisión física del equipo antes de insertar cualquier cable** (tarea del handoff).

> El manual describe el S/P como “3.5mm stereo interface achieving stereo output”, pero la definición
> eléctrica del conector publicada tiene **una sola línea de audio (AUDIO)** y un contacto **NC**. Eso **no**
> autoriza asumir estéreo real, señal balanceada, nivel de línea ni impedancia conocida.

## Pinout del S/P (3,5 mm TRS macho, 3 contactos)

| Contacto físico | Etiqueta del manual | Función declarada | Uso permitido en P0 |
|---|---|---|---|
| Punta (tip) | `AUDIO` | Audio output (salida de audio) | única señal a capturar/medir |
| Anillo (ring) | `NC` | Empty (sin función) | **no conectar** |
| Manguito (sleeve) | `GND` | Grounding | referencia común |

**Prohibido:** plug TS mono (2 contactos), plugs de otras especificaciones, adaptadores 2→3 polos,
cables con divisor, conectar a los puertos **KEY** o **ACC**.

## Por qué TS/adaptadores están prohibidos (palabras del manual)

- S/P: *“Note: short circuit or silence will be caused if plugging the single track plug externally.”*
  (“plug de un solo contacto” = TS mono).
- KEY (puerto distinto, también 3,5 mm estéreo): *“X6200 may switch to transmitting mode if plugging in or
  unplugging the tapper plug when it is working.”* y *“Please cut off the power supply of X6200 before
  connecting or disconnecting the tapper.”* — se adopta la misma regla de apagado para el S/P como regla
  **más estricta** del issue #371.
- General: *“Using plugs of other specifications may damage the socket.”*
- S/P MODE: *“When connecting headphones outside the S/P port, select EAR PHONE. (…) when selecting a
  SPEAKER, do not connect the earphones and listen to it. Otherwise, it may damage the earphones and
  hearing.”* — para la captura P0 el modo es **`EAR PHONE`**, nunca `SPEAKER` con auriculares o interfaz.
- Seguridad auditiva: *“Do not use headphones at high volume.”*

## Reglas de conexión (todas obligatorias)

1. **Sólo el puerto S/P.** Nunca KEY ni ACC. Ambos son 3,5 mm estéreo con otro pinout: KEY es la llave de CW
   (puede poner la radio en TX al enchufar/desenchufar) y ACC lleva `ALC_V`, `TRX` y `BAND_V`.
2. **Conectar y desconectar sólo con la radio apagada**, sin alimentación (regla del issue #371; el manual
   ya la exige para el KEY y es la práctica conservadora para el S/P). Nada de “hot-plug”.
3. **Sólo TRS macho de 3 contactos.** Nada de TS mono ni adaptadores 2→3 polos (manual: cortocircuito o
   silencio; riesgo de dañar el zócalo).
4. **Verificar el cable con la radio apagada y el cable desconectado**, midiendo continuidad: punta↔AUDIO,
   anillo↔NC (sin contacto con otros), manguito↔GND. **Sin medidor declarado no se conecta**
   (`connector_verified=false` en la planilla).
5. **El S/P es una salida.** No se le inyecta audio, no se lo conecta a otra salida de audio, no se lo usa
   para leer registros. La señal va **del S/P hacia la interfaz**, nunca al revés.
6. **Modo `S/P MODE = EAR PHONE`** seleccionado por el operador **antes** de capturar. No se usa `SPEAKER`
   con auriculares/interfaz (advertencia de daño auditivo del manual).
7. **Volumen inicial bajo**; subir sólo si la captura es insuficiente y siempre con el operador presente.
   Si duele o molesta, se baja y se corta.
8. **Sin voltajes ni impedancias inventadas.** El manual sólo publica especificaciones del **parlante**
   (`Audio output: 0.5 W (4 Ω, ≤10 % THD)`; impedancia de carga 4–16 Ω). Eso es el PA/parlante, **no** el
   nivel eléctrico del S/P: **no** se usa para calcular un atenuador sin medir. Nivel, impedancia y offset
   de continua del S/P: **UNKNOWN** hasta medirlos con instrumento declarado.
9. **Referencia de masa e aislamiento:** el sleeve es GND. No crear lazos de masa; si la interfaz no es
   aislada o no tiene tolerancia declarada, **no conectar** (`BLOCKED_UNSAFE_AUDIO_INPUT`).
10. **Un solo cable, una sola interfaz, un solo destino.** Sin divisores, sin “probar con los auriculares y
    después con la interfaz”, sin conectar en paralelo.

## Croquis de la cadena (estado real: bloques NO DISPONIBLES)

```text
  Xiegu X6200 (salida S/P, 3,5 mm TRS)          [radio PRESENTE; ventana desde 12/11]
        │
        │  punta = AUDIO  ·  anillo = NC  ·  manguito = GND
        │  (cable TRS macho 3 contactos: NO DECLARADO / NO VERIFICADO)
        ▼
  ┌─────────────────────┐
  │ Cable TRS 3,5 mm    │  verificación de continuidad con radio APAGADA
  │ 3 contactos         │  (medidor/breakout: NO DECLARADO)
  └─────────┬───────────┘
            ▼
  ┌─────────────────────┐
  │ Atenuador pasivo /  │  valores calculados sólo si se midió el nivel de S/P
  │ limitador           │  (NO DISPONIBLE; NO se inventan valores)
  └─────────┬───────────┘
            ▼
  ┌─────────────────────┐
  │ Interfaz audio USB  │  entrada LINE, alta impedancia, aislamiento galvánico,
  │ externa (LINE)      │  límite de entrada declarado (NO DISPONIBLE / NO DECLARADO)
  └─────────┬───────────┘
            ▼
  ┌─────────────────────┐
  │ Host: WAV/PCM       │  PRESENTE-HOST (ThinkPad/Fedora, codec ALC257)
  │ 44100 Hz + timestamps│  ⚠ NO es instrumento de medida con límites declarados
  └─────────────────────┘

  Camino INDEPENDIENTE del estado del MUX (NO es este cable):
    estado físico del MUX  ←  readback real / sonda no invasiva / doc OEM
    = NO IDENTIFICADO  ⇒  P1_BLOCKED_NO_SAFE_RESTORE  ⇒  NO se conmuta el MUX
```

## Lo que este cable NO hace

- No lee el registro 12 ni ningún registro del BASE: el S/P es audio, no un canal de estado.
- No demuestra `MUX_SELECT_PHYSICALLY_VERIFIED` ni `RESTORE_VERIFIED`.
- No habilita TX/PTT/ATU ni cambia el modo de la radio.
- No convierte una escucha en medición: sin instrumento declarado y sin registrar límites, no hay
  `AUDIO_OUTPUT_VERIFIED`.

Ver también [`croquis.md`](croquis.md) para el esquema de pasos y la separación de gates.
