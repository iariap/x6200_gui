# Protocolo de observación acústica no invasiva (para futura autorización)

Objetivo: observar cambio de ruta efectiva y retorno a BASE sin tocar firmware/registros no
documentados, usando señales diferenciables y evidencia externa.

## Fuente y señales
- Fuente autorizada y repetible (generador RF del owner o estación conocida) en VFO/modo fijos; sin
  grabaciones de terceros, sin TX/PTT/ATU.
- Señal diferenciable: tono/patrón de audio conocido en RX (p.ej. tono modulado) que permita distinguir
  "BASE nativo" vs "ruta Linux procesada" por espectro/nivel/textura ya existentes; **no** se agregan
  filtros ni presets nuevos.
- Grabadora externa (teléfono/grabadora) en posición fija frente al parlante, con reloj/timestamps
  independientes; el operador marca en voz/tiempo los instantes de comando.

## Tomas
1. BASE (QA off) con señal de referencia, grabación de 20-30 s.
2. Activación QA (autorizada), misma fuente/volumen/preset, 20-30 s; monitorear `pactl` y logs.
3. Fallo benigno autorizado (p.ej. SIGSTOP GUI) y observar transición a BASE; repetir N≥5 con
   intervalos; luego rollback.
4. Separar detección (log/supervisor), comando (timestamp de log) y efecto audible (grabación externa).

## Límites
- Evidencia acústica ≠ readback del MUX; no prueba selección de registro, sólo dependencia/efecto
  audible. No se usa para declarar `MUX_PHYSICAL_STATE_VERIFICATION` PASS.
- Sin referencia/timestamps externos la medición es sólo estimación (PARTIAL), no PASS.
