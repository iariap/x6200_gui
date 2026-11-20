# Plan de medición de recovery (≤250 ms) — no ejecutado

## Referencia de tiempo independiente
- Grabadora externa/video con reloj visible iniciada junto a la sesión; **marcador de comando**
  audible/grabado (p.ej. el operador dice "kill" o GPIO-free: no se permite escribir GPIO/hardware).
- Señal que distingue ruta: fuente estable en RX; al perder Linux debe oírse BASE nativo; la transición
  se marca en la grabación externa.
- Timestamps de software (logs GUI/servicio) son **complementarios**, nunca la medición principal.

## Fases
1. **Detección independiente:** ¿el sistema detecta la falla sin depender del propio hilo que falla?
   (supervisor/lease); registrar el instante del comando.
2. **Comando/restore lógico:** `shutdown_safe`/lease→BASE; medir en logs.
3. **Efecto audible:** transición grabada externamente; medir desde el instante del comando (grabación)
   hasta el primer audio BASE reconocible. Objetivo ≤250 ms; repetir N≥5.
4. **Faults**: empezar por fallos benignos (SIGSTOP GUI, matar sink/worker) con abort/rollback. **No**
   provocar fallos destructivos con RX_LINUX activo hasta demostrar retorno BASE independiente
   (audible + estado seguro). Sin independencia ⇒ **STOP**, no test destructivo.

## Criterios de aborto
- No se observa BASE audible en la ventana de seguridad; se activa TX/ATU inesperado; MUX incierto;
  cualquier pérdida de control ⇒ rollback inmediato (GUI/firmware original, gate off, verificar state).
