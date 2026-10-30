# Croquis estático — banco de medida S/P y separación de gates (#371)

Todos los diagramas son **estáticos**: describen qué habría que conectar y qué falta. **Nada fue
conectado, medido ni conmutado.** Estado 2026-10-30: `P0_PENDING_EQUIPMENT` · `P1_BLOCKED_NO_SAFE_RESTORE`.

## 1. Conector S/P del manual Rev.05 (3,5 mm TRS macho, 3 contactos)

Fuente: manual Xiegu X6200 Rev.05, “Interface Description → Interface definitions” (p. 8 impresa;
PDF p. 12). El dibujo publicado del conector S/P etiqueta:

```text
        AUDIO (tip / punta) ───────────────►  señal de audio de salida
        NC    (ring / anillo)  ──────────╳    sin función ("Empty"); no conectar
        GND   (sleeve / manguito) ───────►    masa / referencia común

        Representación del plug macho (concepto):

              ┌────────── punta: AUDIO
              │ ┌──────── anillo: NC
              ▼ ▼
        ┌─────┬─┬───────────────┐
        │ ███ │ │               │
        └─────┴─┴───────────────┘
                    ▲
                    └────────── manguito: GND
```

**No** se usa TS mono (2 contactos): el manual advierte cortocircuito o silencio. **No** se usa KEY ni ACC.
**No** se asignan tensiones: nivel/impedancia/offset del S/P son **UNKNOWN** hasta medirlos.

## 2. Banco externo no invasivo (lo que hay y lo que falta)

```text
   ┌──────────────────────────┐
   │  X6200 en RX (BASE)      │  PRESENTE; acceso físico desde 2026-11-12
   │  S/P MODE = EAR PHONE    │  (modo lo selecciona el OPERADOR)
   │  volumen bajo inicial    │
   └───────────┬──────────────┘
               │  TRS macho 3,5 mm · 3 contactos · AUDIO/NC/GND
               │  conexión SÓLO con radio APAGADA
               ▼
   ┌──────────────────────────┐
   │  Cable TRS verificado    │  ┌─ NO DECLARADO: cable + medidor de continuidad
   └───────────┬──────────────┘
               ▼
   ┌──────────────────────────┐
   │  Atenuador/limitador     │  ┌─ NO DISPONIBLE: valores dependen del nivel medido
   └───────────┬──────────────┘
               ▼
   ┌──────────────────────────┐
   │  Interfaz LINE aislada   │  ┌─ NO DISPONIBLE / NO DECLARADO:
   └───────────┬──────────────┘      entrada LINE, alta Z, límite declarado
               ▼
   ┌──────────────────────────┐
   │  Host: WAV 44100 Hz      │  PRESENTE-HOST (no es instrumento de medida)
   │  reloj monótono + sha256 │
   └──────────────────────────┘

   Camino paralelo permitido (sólo lectura, si está autorizado):
   radio ── PCM source PA (`parec`, pre-Speex) ── host ── para estimar afinidad,
   SIN confundirlo con la salida procesada de la GUI.
```

## 3. Dos caminos que NO se pueden confundir

```text
   ┌────────────────────────────────────────────┐
   │ CAMINO AUDIO (P0)                          │
   │ S/P  ──►  TRS  ──►  atenuador  ──►  LINE   │
   │  └─ demuestra, como máximo, AUDIO presente │
   │     en la salida física (AUDIO_OUTPUT_…)   │
   └────────────────────────────────────────────┘

   ┌────────────────────────────────────────────┐
   │ CAMINO ESTADO DEL MUX (P1)                 │
   │ reg 12 bit voice_rec (0x8)                 │
   │   ├─ readback REAL (no sombra RAM)         │
   │   ├─ dueño único sin competir con idle     │
   │   └─ restore selectivo ≤250 ms VERIFICADO  │
   │        (MUX_SELECT_PHYSICALLY_VERIFIED,    │
   │         RESTORE_VERIFIED)                  │
   └────────────────────────────────────────────┘

   ╳  No hay flecha entre los dos caminos.
   ╳  Escuchar/capturar el S/P NO lee el registro 12
      ni prueba que el MUX cambió ni que se puede restaurar.
```

## 4. Secuencia física desde 2026-11-12 (cada paso puede terminar en STOP)

```text
   [0] ¿Está TODO el equipo mínimo declarado y verificado?
        │ sí                                   │ no
        ▼                                      ▼
   [1] Radio APAGADA: pinout TRS con medidor   STOP  P0_PENDING_EQUIPMENT
        │                                      (no se conecta nada)
        ▼
   [2] Radio APAGADA: conectar TRS (+ atenuador si corresponde)
        │
        ▼
   [3] Encender; S/P MODE = EAR PHONE; volumen bajo; tx=false
        │
        ▼
   [4] Medir DC/AC del S/P con instrumento declarado ⇒ elegir atenuación
        │  (sin instrumento: no se conecta la interfaz)
        ▼
   [5] Radio APAGADA: insertar interfaz LINE aislada (ya atenuada)
        │
        ▼
   [6] Encender; verificar tx=false; capturar 10 s de RX propia/ruido
        │   + registro read-only del PCM source PA (si autorizado)
        ▼
   [7] Registrar fila(s) del CSV: peak/rms/hashes/latencia según aplique
        │
        ▼
   [8] Cerrar: radio APAGADA, desconectar, estado final verificado
        (tx=false, hashes), sin MUX, sin deploy, sin TX.

   En cualquier paso: TX/PTT/ATU visible, clipping/pop, inestabilidad,
   calor/olor, duda del operador  ──►  STOP + apagar + desconectar + registrar
```

## 5. Decisión de P1 (no se ejecuta en esta ventana)

```text
   ¿Existe fuente de verdad INDEPENDIENTE del estado del MUX?
     ├─ readback real del reg 12 (no la sombra)        ─┐
     ├─ sonda externa no invasiva de select/feedback    ├─ no identificada
     └─ documentación OEM verificable con ventana       ─┘
                    │
                    ▼
        NO  ⇒  P1_BLOCKED_NO_SAFE_RESTORE  ⇒  NO conmutar MUX
                    │
                    ▼
     Aunque el S/P suene: AUDIO ≠ estado del registro ≠ restore.
```

Estos croquis no son evidencia de hardware: son el plano de lo que falta. La evidencia real de una
medición futura va en su propio directorio, con la planilla completada y los hashes de captura.
