# Validacion en la radio del fix de UI-010 (Outdoor: franja de frecuencia invisible)

Fix validado: `858377e` — *main: the Outdoor frequency strip asks for the theme roles (UI-010 fix,
critical) (#22)*. Causa: `src/main_screen.c` cableaba `0xFFFFFF` en el markup de recolor del label
(`#RRGGBB `), y ese markup **pisa** el `text_color` del estilo, asi que en el tema claro Outdoor la
fila salia blanca sobre blanco; ademas iba con `%03X`, que LVGL no toma (necesita 6 digitos), con lo
que el color cableado quedaba. El fix usa los roles del tema (`TC_text` / `TC_text_soft` con el dial
trabado) y `%06X`.

## A/B en la radio (tema Outdoor, misma pantalla)

| Captura | Fila de frecuencia (`frecuencia`, y=160..196) |
|---|---|
| `outdoor-frecuencia-SIN-fix.png` | **en blanco**: no se ve la frecuencia |
| `outdoor-frecuencia-CON-fix.png` | `7.002` / **7.098.000** / `7.194`, legible |
| `outdoor-pantalla-completa.png` | pantalla completa con el fix: sin regresiones visibles en barra, cascada ni pie |

Medido sobre las capturas (pixeles con luminancia < 128 en la banda de la fila, y=28..62 del recorte):

- antes: **0 de 27200 (0,00 %)**
- despues: **2274 de 27200 (8,36 %)**

## Resultado

**UI-010 validado en la radio**: la informacion critica de la frecuencia vuelve a verse con el tema
claro. Sin regresiones en el resto de la pantalla.

Nota: la cascada con Outdoor sigue siendo ruido casi blanco (legibilidad discutible). Es una
observacion ya registrada dentro de la entrada UI-011 del registro de ellos, no un defecto nuevo y no
es regresion de este fix, asi que no se carga issue.
