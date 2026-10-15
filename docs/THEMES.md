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
| **Redshift** | `themes/Redshift.json` |
| **Aurora** | `themes/Aurora.json` |
| **Trail** | `themes/Trail.json` |

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

## Redshift — operación nocturna (THEMES v2, issue #68)

Tema de **noche**: negro casi absoluto con tinte cálido, texto coral rojo cálido, foco salmón
y waterfall granate. Sin cian, sin azul y sin superficies blancas: los roles "fríos" que el
esquema hereda del `base` (info cian, digi violeta, ACCENT azul, TX blanco) se reescriben en la
familia cálida para que ningún rol de `Classic` se cuele y contradiga la identidad. Fuente
**Source Sans 3** (`"font": "sourcesans3"`), geometría recta (`radius 0`, `radius_btn 1`,
`radius_popup 2`), sin gradientes (`btn_gradient`/`box_gradient` en `false`) y foco sólido.

El archivo **define los 122 roles** de `src/theme.h` (no sólo los ~40 del borrador del issue):
dejar roles sin definir los heredaba de `Classic`, que trae blancos, celestes y azules. El JSON
del issue además nombraba tres claves que ya no existen en el esquema (`strip`, `mode_bg`,
`mode_text`, podadas en #77): incluirlas habría dejado el tema con colores desconocidos.

**Alcance real en la pantalla.** El motor de temas JSON colorea los roles que el firmware ya lee:
el fondo (`bg`), la fila de frecuencia (`text`), los bordes de panel (`panel_border`), la paleta
del waterfall, los items del info box (`bar`), los switches/sliders de Ajustes
(`control_border`/`panel`) y el recolor del botón activo/deshabilitado. Las cajas de la barra
superior (reloj, S-meter), los botones inferiores y el marco de los diálogos todavía se dibujan
con las imágenes fijas `images/top_*_dark.bin`, `btn_dark.bin` y `dialog_dark.bin`
(`src/styles.c:393-423`, `setup_theme_simple`), así que quedan en el gris azulado del asset en
**todos** los temas: RS, Material 3, Holo y Redshift renderizan ese marco idéntico (medido: el
fondo del diálogo de Ajustes es `#3c505c` bajo RS, bajo el tema por defecto y `#3b4e59` bajo
Redshift). Es un hueco preexistente del motor —documentado en el commit que agregó el sistema
JSON— y migrarlo es un refactor aparte, fuera del alcance de un tema suelto.

Pares medidos (WCAG 2.1, sRGB) para los roles de la identidad, todos por encima del umbral:

| Par (fg sobre bg) | Ratio | Mínimo |
|---|---:|---:|
| `text` sobre `bg` / `panel` | 11.77 | 4.5 |
| `text` sobre `bar` / `dialog` | 11.43 | 4.5 |
| `text` sobre `row` / `chip` | 11.13 / 10.80 | 4.5 |
| `text_value` sobre `bg` | 14.48 | 4.5 |
| `text_dim` sobre `bg` | 7.82 | 4.5 |
| `btn_text` sobre `btn` | 9.92 | 4.5 |
| `btn_active_text` sobre `btn_active` | 8.88 | 4.5 |
| `focus_text` sobre `focus` | 9.38 | 4.5 |
| `sel_text` sobre `sel_bg` / `near_bg` | 9.05 / 13.44 | 4.5 |
| `vfo` sobre `vfo_bg` / `side` | 10.26 / 11.55 | 4.5 |
| `tx` sobre `tx_bg` | 6.69 | 4.5 |
| `mode_dig` sobre `bar` | 11.20 | 4.5 |
| `control_border` sobre `dialog` / `panel` | 3.11 / 3.20 | 3.0 |
| `bar_border`/`panel_border`/`popup_border`/`card` sobre su fondo | 3.70–3.81 | 3.0 |
| `btn_border` sobre `btn` | 3.43 | 3.0 |
| `meter_bar` sobre `bg` | 8.65 | 3.0 |

`tests/check_wcag.py` mide 127 pares aplicables a este tema y no reporta **ninguna falla**;
`x6200-ab themes` (los pares que sólo viven en Ajustes) suma los nueve declarados y también
pasa. El arco del S-meter (`meter_over` = `#FFD0AA`) mide 14.33:1 sobre el `bg` del tema; el
repaso sobre el fondo **real compuesto en la radio** queda para la pasada física del #73 (el
banco no se usó en esta corrida).

Las capturas del arnés por estado son `main-tema-redshift` (principal), `settings-tema-redshift`
(Ajustes → Apariencia), `bt-dialog-tema-redshift` (diálogo), `main-tema-redshift-tx` (TX),
`main-tema-redshift-eco` (batería baja/ECO) y `main-tema-redshift-agua` (espectro + waterfall).
La galería ARM real y la comparación consolidada contra los otros cinco temas se cierran en el
#73; el selector/default se migra en el #74.

## Aurora — uso diario premium (THEMES v2, issue #72)

Tema **de uso diario**: **berenjena profundo** (fondo `bg #1A1227`, barra `bar #2C213E`,
panel `panel #1B1529`), **lavanda** para las lecturas (`text #F8EEFF`, `vfo #D4BBFF`,
`accent`/`text_head #D7C2FF`) y **coral** para lo que pide atención o energía
(`tx`/`err`/`mode_dig #FFAAA4`–`#FFB3A8`, `btn_mark_active #FF8E86`), con **notas turquesas**
en el S-meter y los spots (`meter_bar`/`alc`/`spot #7EE0D0`, `meter_low #5FD0C8`). Es el
único de la colección que arranca con **gradientes suaves encendidos** (`btn_gradient` y
`box_gradient` en `true`) y **radios grandes** (`radius 12`, `radius_btn 16`, `radius_popup 14`),
con foco sólido y caja de modo: la cara "premium" del epic, distinta del Material 3 azul/gris y
del Sprint angular.

**Fuente.** `font: inter` (la familia ya compilada en ROM desde #60; `src/fonts/font_families.c`).
El tema **no** fija `font_digits`: la frecuencia y el reloj siguen con la familia por defecto,
que es lo que la identidad de Aurora pide (números normales, sin panel segmentado).

**Cobertura de roles.** El archivo escribe **122 de 122 roles** de `src/theme.h` (nada queda
heredado de `IU1WXY`, que es su `base`): `x6200-ab themes` lo reporta como
`Aurora (...; base IU1WXY): 0/122 roles from the base`. El JSON de partida del issue #72 nombraba
tres claves que ya no existen en el esquema (`strip`, `mode_bg`, `mode_text`, podadas en #77):
incluirlas habría dejado el tema con colores desconocidos, así que se reasignaron a sus roles
reales (`bar_grad`/`panel` y `sel_bg`/`sel_text`). El `base` es `IU1WXY`, que **se retira** en
#74: como el archivo define los 122 roles, la migración de la base en #74 no puede contradecir
ningún rol de Aurora.

**Alcance real en la pantalla.** El motor de temas JSON colorea los roles que el firmware ya lee:
el fondo (`bg`), la fila de frecuencia (`text`), los bordes de panel (`panel_border`), la paleta
del waterfall, los items del info box (`bar`), los switches/sliders de Ajustes
(`control_border`/`panel`) y el recolor del botón activo/deshabilitado. Las cajas de la barra
superior (reloj, S-meter), los botones inferiores y el marco de los diálogos todavía se dibujan
con las imágenes fijas `images/top_*_dark.bin`, `btn_dark.bin` y `dialog_dark.bin`
(`src/styles.c`, `setup_theme_simple`), así que quedan en el gris azulado del asset en **todos**
los temas. `theme_shape.radius`, `radius_btn`, `btn_gradient`, `box_gradient`, `focus_solid`,
`mode_box`, `pill_border`, `mark` e `images` se cargan del JSON pero hoy el código no los usa en
la pantalla principal: los radios y los gradientes de Aurora no son un delta respecto de los
otros temas en esta revisión, y quedan declarados acá para que el día que la superficie de forma
vuelva a dibujarse el tema ya la traiga.

Pares medidos (WCAG 2.1, sRGB) para los roles de la identidad, todos por encima del umbral:

| Par (fg sobre bg) | Ratio | Mínimo |
|---|---:|---:|
| `text` sobre `bg` / `panel` / `bar` / `dialog` | 16.09 / 15.74 / 13.39 / 14.77 | 4.5 |
| `text` sobre `row` / `chip` | 13.53 / 12.85 | 4.5 |
| `text_value` sobre `bg` | 18.11 | 4.5 |
| `text_dim` sobre `bg` | 11.04 | 4.5 |
| `text_head` sobre `bar` | 9.36 | 4.5 |
| `btn_text` sobre `btn` / `btn_grad` | 11.29 / 12.39 | 4.5 |
| `btn_active_text` sobre `btn_active` / `btn_active_grad` | 5.55 / 5.08 | 4.5 |
| `focus_text` sobre `focus` | 10.43 | 4.5 |
| `sel_text` sobre `sel_bg` / `near_bg` | 6.91 / 9.52 | 4.5 |
| `vfo` sobre `vfo_bg` / `side` | 6.29 / 9.90 | 4.5 |
| `tx` sobre `tx_bg` | 5.70 | 4.5 |
| `mode_dig` sobre `bar` | 8.80 | 4.5 |
| `info` / `ok` / `err` / `accent` sobre `panel` | 11.18 / 12.44 / 9.68 / 9.62 | 4.5 |
| `action` / `ok` sobre `row` | 8.89 / 10.69 | 4.5 |
| `alert_text` sobre `alert_bg` | 8.81 | 4.5 |
| `control_border` sobre `dialog` / `panel` | 4.34 / 4.62 | 3.0 |
| `bar_border`/`panel_border`/`popup_border`/`card` sobre su fondo | 3.93–4.72 | 3.0 |
| `btn_border` sobre `btn` | 3.30 | 3.0 |
| `btn_active_border` sobre `btn_active` | 3.46 | 3.0 |
| `sel_border` sobre `sel_bg` | 4.78 | 3.0 |
| `meter_bar` sobre `bg` | 12.88 | 3.0 |

`tests/check_wcag.py` mide **134 pares aplicables** a este tema (es el que más pares tiene, por
traer gradientes y `pill_border`) y no reporta **ninguna falla**; `x6200-ab themes` (los nueve
pares que sólo viven en Ajustes) suma 9/9 OK. En el framework, el **no-texto por píxeles**
(WCAG 1.4.11) sobre el render dorado del tema da el arco `meter_over` del S-meter **6.23:1** y
la columna de señal del waterfall **6.57:1** (mínimo 3:1), declarados en
`tools/ab_testing/quality/nontext.yaml`.

**Capturas por estado.** Las fixtures del arnés que rinden Aurora (todas con el mismo estado de
radio que su gemela de otro tema, así el tema es la única variable): `main-tema-aurora`
(principal, RX), `main-tema-aurora-agua` (waterfall en movimiento), `main-tema-aurora-tx` (TX
simulado), `main-tema-aurora-eco` (batería baja + ECO), `bt-dialog-tema-aurora` (diálogo) y
`settings-tema-aurora` (Ajustes, con el foco sobre la fila del perfil). La galería ARM real y la
comparación consolidada contra los otros cinco temas se cierran en el #73; el selector/default se
migra en el #74.

## Trail — campo y expedición (THEMES v2, issue #71)

Tema de **campo**: verde bosque plano para las superficies, **oliva** para lo que pide atención
(foco, botón activo, selección, encabezados, S-meter) y **arena** clara para leer; el ámbar de
tierra queda para TX y alarma. Sin neón, sin superficies frías y sin berenjena: los roles
"fríos" que el esquema hereda del `base` (info cian, digi violeta, ACCENT azul, TX blanco) se
reescriben en la familia tierra para que ningún rol de `Classic` se cuele y contradiga la
identidad. Fuente **IBM Plex Sans** (`"font": "ibmplexsans"`, la familia compilada en ROM desde
#60, sin descargar nada), geometría sobria (`radius 5`, `radius_btn 5`, `radius_popup 6`,
`border 1`), sin gradientes (`btn_gradient`/`box_gradient` en `false`) y foco sólido. El
waterfall (`#0A150D → #25412B → #526A3C → #9EB370 → #E1C073 → #FC985E`) lleva el piso de ruido
al bosque y la señal del verde al ámbar de tierra: la misma jerarquía en la cascada.

El archivo **define los 122 roles** de `src/theme.h` (no sólo los ~30 del borrador del issue):
dejar roles sin definir los heredaba de `Classic`, que trae blancos, celestes y azules. El JSON
del issue además nombraba tres claves que ya no existen en el esquema (`strip`, `mode_bg`,
`mode_text`, podadas en #77): incluirlas habría dejado el tema con colores desconocidos.

**Alcance real en la pantalla.** El motor de temas JSON colorea los roles que el firmware ya lee:
el fondo (`bg`), la fila de frecuencia (`text`), los bordes de panel (`panel_border`), la paleta
del waterfall, los items del info box (`bar`), los switches/sliders de Ajustes
(`control_border`/`panel`) y el recolor del botón activo/deshabilitado. Las cajas de la barra
superior (reloj, S-meter), los botones inferiores y el marco de los diálogos todavía se dibujan
con las imágenes fijas `images/top_*_dark.bin`, `btn_dark.bin` y `dialog_dark.bin`
(`src/styles.c:393-423`, `setup_theme_simple`), así que quedan en el gris azulado del asset en
**todos** los temas: RS, Material 3, Holo, Redshift y Trail renderizan ese marco idéntico (medido:
el fondo del diálogo de Ajustes es `#3c505c` bajo RS, bajo el tema por defecto y `#3b4e59` bajo
Redshift). Es un hueco preexistente del motor —documentado en el commit que agregó el sistema
JSON— y migrarlo es un refactor aparte, fuera del alcance de un tema suelto.

Pares medidos (WCAG 2.1, sRGB) para los roles de la identidad, todos por encima del umbral:

| Par (fg sobre bg) | Ratio | Mínimo |
|---|---:|---:|
| `text` sobre `bg` / `panel` | 14.41 / 14.31 | 4.5 |
| `text` sobre `bar` / `dialog` | 11.77 / 12.91 | 4.5 |
| `text` sobre `row` / `chip` | 12.25 / 10.70 | 4.5 |
| `text_value` sobre `bg` | 15.97 | 4.5 |
| `text_dim` sobre `bg` / `dialog` | 11.74 / 10.52 | 4.5 |
| `btn_text` sobre `btn` | 10.13 | 4.5 |
| `btn_active_text` sobre `btn_active` | 10.05 | 4.5 |
| `focus_text` sobre `focus` | 11.66 | 4.5 |
| `sel_text` sobre `sel_bg` / `near_bg` | 5.72 / 9.11 | 4.5 |
| `vfo` sobre `vfo_bg` / `side` | 7.98 / 12.72 | 4.5 |
| `tx` sobre `tx_bg` | 6.20 | 4.5 |
| `mode_dig` sobre `bar` | 8.59 | 4.5 |
| `info` sobre `panel` / `bg` | 10.76 / 10.84 | 4.5 |
| `control_border` sobre `dialog` / `panel` | 4.23 / 4.68 | 3.0 |
| `bar_border`/`panel_border`/`popup_border`/`card` sobre su fondo | 3.15–3.58 | 3.0 |
| `btn_border` / `btn_active_border` sobre su botón | 3.10 / 5.00 | 3.0 |
| `sel_border` sobre `sel_bg` | 4.13 | 3.0 |
| `meter_bar` sobre `bg` | 10.02 | 3.0 |

`tests/check_wcag.py` mide 127 pares aplicables a este tema y no reporta **ninguna falla**;
`x6200-ab themes` (los pares que sólo viven en Ajustes) suma los nueve declarados y también
pasa. El arco del S-meter (`meter_over` = `#FFB59A`) mide 9.87:1 sobre el `bg` del tema; el
repaso sobre el fondo **real compuesto en la radio** queda para la pasada física del #73 (el
banco no se usó en esta corrida).

Las capturas del arnés por estado son `main-tema-trail` (principal), `settings-tema-trail`
(Ajustes → Apariencia), `bt-dialog-tema-trail` (diálogo), `main-tema-trail-tx` (TX),
`main-tema-trail-eco` (batería baja/ECO) y `main-tema-trail-agua` (espectro + waterfall).
La galería ARM real y la comparación consolidada contra los otros cinco temas se cierran en el
#73; el selector/default se migra en el #74.
