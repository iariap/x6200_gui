# Temas

Todos los colores de la GUI salen de un **rol** (fondo, botón, fila seleccionada, texto de
encabezado, OK, TX...). Un tema le da un color a cada rol, más unas formas, la fuente y la
paleta del waterfall.

## Los que vienen con la ROM

**Incorporados** (tablas en el código, sin archivos):

| Nombre | Qué es |
|---|---|
| **IU1WXY** | el de fábrica del X6100, oscuro con digitos blancos y turquesa |
| **Classic** | negro y gris, al estilo IC-7300 |
| **Outdoor** | claro, para el sol |
| **Outdoor-Negative** | Outdoor con la luminosidad invertida (oscuro, mismos tonos) |
| **Legacy** | se conserva pero **no se ofrece** en el menú |
| **VISION** | los colores de IU1WXY en "antracita cálido": dígitos blancos sobre gris oscuro, naranja para lo encendido, marcos agua |

**Archivos JSON** (migrados del X6100, con sus imágenes):

| Tema | Archivo |
|---|---|
| **Holo** | `themes/Holo.json` |
| **Material 3** | `themes/Material 3.json` |
| **RS** | `themes/RS/theme.json` + los PNG de sus botones (`btn.png`, `bar.png`, `strip.png`, `dialog.png`, `mark.png`, ...) |

## El arco del S-meter (`meter_over`) — medido en el equipo, 2026-10-15 (#44)

El arco `TC(meter_over)` del medidor hay que mirarlo **en la pantalla**, no sólo en el JSON:
el fondo real del arco en la radio es `(62,78,92)`, no el `panel` que declara el archivo.
RS es un **tema de ROM** — su `base` es Classic, así que el equipo resuelve la columna Classic
de la tabla compilada (`src/theme.h`, `X(meter_over, ...)`) — y el `theme.json` no garantiza
que el color llegue a la pantalla.

| Color | vs fondo real `(62,78,92)` | ¿3:1 (WCAG 1.4.11)? |
|---|---:|---|
| `#ff4040` (viejo, columna Classic de `src/theme.h`) | 2.47:1 | no |
| `#ff5252` (fix de #42, sólo en `theme.json`) | 2.69:1 | no |
| **`#f08080` (issue #44)** | **3.31:1** | sí |

El valor queda **igual en los dos lugares** (la columna Classic de la tabla compilada y
`themes/RS/theme.json`) para que banco y radio no diverjan: `#f08080`. En el arnés el mismo
color mide **3.96:1** (RS) y **4.41:1** (Classic) por píxel, contra el fondo que el propio
render dibuja, en `tools/ab_testing/quality/nontext.yaml` (objeto `s_meter_arc`, WCAG 1.4.11);
el repaso del tono en la radio queda en la pasada física (#15).

## Dónde se buscan

Dos directorios, en este orden:

1. **`/mnt/themes/`** — el del usuario, en la partición DATA: gana si hay un nombre repetido,
   así un tema propio con el mismo nombre reemplaza al de la ROM;
2. **`/usr/share/x6200/themes/`** — los que vienen en la imagen.

Un tema es un archivo `<nombre>.json`, o una carpeta `<nombre>/theme.json` cuando trae sus
propias imágenes (como el RS). `THEMES_DIR` y `THEMES_DIR_SHIPPED` cambian esos dos caminos
(lo usan los tests, para no tocar `/mnt`).

Elegir un tema en *Ajustes → Display → INTERFACE → Theme* reinicia la GUI con él.

## Formato del archivo

Todo campo es opcional: lo que un archivo no pone sale de su `base`.

```json
{
    "base": "Classic",
    "colors": {
        "bg": "#000000",
        "btn": "#5e5e5e",
        "btn_grad": "#262626",
        "sel_bg": "#1f5fbf",
        "text_head": "#40c8ff"
    },
    "radius": 3,
    "radius_btn": 4,
    "radius_popup": 4,
    "border": 1,
    "btn_gradient": true,
    "box_gradient": false,
    "focus_solid": true,
    "font": "roboto",
    "waterfall": ["#000000", "#000060", "#0040e0", "#00c8ff", "#ffff00", "#ff6000", "#ffffff"],
    "waterfall_steps": 0,
    "images": {
        "btn": { "file": "btn.png", "slice": [8, 8, 8, 8] }
    }
}
```

Sin `base`, el tema parte de IU1WXY. Un archivo con JSON inválido, con un color mal escrito o
que no se puede leer se avisa por el log y se sigue con el tema anterior: nunca deja la GUI
sin tema.

## Para agregar uno propio

1. Copiá tu archivo a la partición **DATA**, en `themes/`: `/mnt/themes/Mi tema.json`
   (o `/mnt/themes/Mi tema/theme.json` si trae imágenes al lado).
2. Aparece como *Mi tema* en la lista de temas, sin reiniciar ni recompilar nada.

## Tests

`tests/native/test_theme.c` prueba los incorporados, la carga de un JSON, el respaldo cuando
el nombre no existe y —con `X6200_SHIPPED_THEMES` apuntando a `rootfs/usr/share/x6200/themes`—
que **todos** los temas que vienen con la ROM se listen y carguen, incluido el RS con sus
imágenes. Si alguno queda afuera, el test falla: es lo que garantiza que estén en el menú del
equipo y no sólo en la imagen.
