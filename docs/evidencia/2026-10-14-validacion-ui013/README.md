# Validacion en la radio del fix de UI-013 (PR #23, sin mergear)

PR validado: **#23** (`7d4e89a` — *params: el store del nombre de tema guarda lo que el menu ofrece*),
un archivo: `src/params/types.h`, `char x[16]` -> **`char x[32]`**. Es el mismo cambio que habia
diagnosticado al reportar UI-013 (el nombre "Outdoor-Negative" necesita 17 bytes y el store truncaba).

## Prueba en la radio (discrimina porque tambien el *load* trunca)

Metodo: escribir `theme_name = "Outdoor-Negative"` (16 caracteres) en `/mnt/params.db` con la GUI
muerta, arrancar y mirar el log y la pantalla. La radio corria **nuestro** build (sin el fix) y despues
el build del PR, construido en un worktree aparte (sin mergear nada):

| Build en la radio | md5 de `x6200_gui` | Log | Tema en pantalla |
|---|---|---|---|
| nuestro (`lu2dsx/x6200-rom`) | `71a10c9aeab9` | `Theme Outdoor-Negativ not found, using IU1WXY` | IU1WXY (fallback) |
| PR #23 (con el fix) | `6f8275c7e099` | **sin avisos** | **Outdoor-Negative aplicado** |

Capturas: `base-outdoor-negative.png` (nuestro build, se ve el fallback) y
`fix-outdoor-negative.png` (con el fix: fondo oscuro y texto claro del tema Outdoor-Negative, y los
iconos de barra del fix de UI-011 siguen bien).

## Resultado

**UI-013 queda validado en la radio**: con el store de 32 bytes el nombre entra entero, el tema se
aplica y no aparece el aviso de "not found". El fix no rompio nada de lo ya validado (iconos UI-011,
frecuencia UI-010, celda VFO UI-012).

Nota: al terminar la prueba la radio quedo con **nuestro** build otra vez (md5 `71a10c9aeab9`) y el
tema en `Material 3`, que era el valor original.
