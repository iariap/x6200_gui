# Validacion en la radio del fix de UI-011 (glyphs de iconos con VISION)

Fix validado: `8512d86` — *fonts: VISION keeps icon glyph ranges when scaling (UI-011 fix, UI-04 EXP-1) (#20)*,
un archivo (`src/fonts/font_select.c`): cada fuente escalada conserva su tamano original como
`fallback`, asi el tamano escalado dibuja el texto y el original aporta el cmap de iconos.

## A/B en la radio (misma pantalla, mismo tema, mismo dia)

Metodo: con el tema **VISION** (puesto en `/mnt/params.db`, `theme_name`), se construyo y desplego
**solo ese archivo revertido** (`git checkout 8512d86^ -- src/fonts/font_select.c`), se capturo la
barra superior, se restauro el archivo, se reconstruyo y se volvio a capturar. Capturas con
`tools/captura-radio.sh` (framebuffer 480x800x32, rotado), recorte 0 0 800 70.

| Captura | WiFi (barra) | Reloj de potencia |
|---|---|---|
| `vision-barra-SIN-fix.png` | **rectangulo vacio** (glyph ausente) | **caja vacia** en lugar del simbolo de bateria |
| `vision-barra-CON-fix.png` | **glifo de WiFi** correcto | **bateria** (8.6V) y **enchufe** (11.8V) correctos |

## Resultado

**UI-011 queda validado en la radio** (no solo en el arnes): con VISION los iconos vuelven.

## Nota (UI-012, no es regresion)

En la misma captura sigue viendose la celda de la izquierda como `VFO-`: con VISION el texto de la
barra crece tres pasos y la celda de 56x26 no, que es exactamente el hallazgo **UI-012**, todavia
abierto. No es una regresion de este fix: este fix restaura los glifos, no cambia tamanos de celda.
