# Temas

Todos los colores de la GUI salen de un **rol** (fondo, botón, fila seleccionada, texto de
encabezado, OK, TX...). Un tema le da un color a cada rol, más unas formas, la fuente y la
paleta del waterfall.

## Los que vienen con la ROM

Desde el issue #73 el **selector de Ajustes ofrece SOLO los seis temas v2** (Sunlit, Redshift,
Sprint, Cathode, Trail, Aurora): la colección es la oferta completa del menú. Todo lo demás
sigue en la ROM y el motor lo carga por nombre —params guardados, fixtures históricas del
arnés—, pero no tiene entrada en la lista; retirarlo del todo y migrar el valor guardado (al
default nuevo) es el #74 (acá el default no cambia). Un tema propio de la partición DATA
tampoco se lista: su archivo se conserva, pero no aparece en el menú.

**Incorporados** (tablas en el código, sin archivos) — ninguno en el selector desde #73:

| Nombre | Qué es |
|---|---|
| **IU1WXY** | el de fábrica del X6100, oscuro con digitos blancos y turquesa |
| **Classic** | negro y gris, al estilo IC-7300 |
| **Outdoor** | claro, para el sol |
| **Outdoor-Negative** | Outdoor con la luminosidad invertida (oscuro, mismos tonos) |
| **Legacy** | se conserva pero **no se ofrece** en el menú |
| **VISION** | los colores de IU1WXY en "antracita cálido": dígitos blancos sobre gris oscuro, naranja para lo encendido, marcos agua |

**Archivos JSON** (`rootfs/usr/share/x6200/themes/`); sólo los seis v2 entran al selector:

| Tema | Archivo | En el selector (#73) |
|---|---|---|
| **Sunlit** | `themes/Sunlit.json` | sí |
| **Redshift** | `themes/Redshift.json` | sí |
| **Sprint** | `themes/Sprint.json` | sí |
| **Cathode** | `themes/Cathode.json` | sí |
| **Trail** | `themes/Trail.json` | sí |
| **Aurora** | `themes/Aurora.json` | sí |
| **Holo** | `themes/Holo.json` | no (legado, carga por nombre) |
| **Material 3** | `themes/Material 3.json` | no (legado, carga por nombre) |
| **RS** | `themes/RS/theme.json` + los PNG de sus botones (`btn.png`, `bar.png`, `strip.png`, `dialog.png`, `mark.png`, ...) | no (legado, carga por nombre) |

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
    "waterfall_signal_width": 3,
    "images": {
        "btn": { "file": "btn.png", "slice": [8, 8, 8, 8] }
    }
}
```

Sin `base`, el tema parte de IU1WXY. Un archivo con JSON inválido, con un color mal escrito o
que no se puede leer se avisa por el log y se sigue con el tema anterior: nunca deja la GUI
sin tema.

## Ancho de la línea de señal del waterfall (`waterfall_signal_width`) — decisión del owner, 2026-10-15

El waterfall dibuja una **línea vertical** en el centro (la marca de la señal sobre la paleta,
`src/waterfall.c`, `draw_middle_line()`), que hoy sale del color `panel_border` del tema. Hasta
acá su ancho estaba fijo en **1 px**. El owner pidió, con feedback directo del 2026-10-15, que
**los temas outdoor la traigan más gruesa**: *"los temas que son para outdoor deberían traer la
barra del waterfall más gruesa porque al ser tan fina (1 px) no se distingue bien del resto"*.

**Decisión de producto post-epic.** El epic de temas v2 había pedido *"sólo esquema real, sin
agregar campos del parser"*; esto lo **extiende por orden del owner**: el ancho pasa a ser una
**clave de tema**, no un valor fijo.

- **Clave**: `waterfall_signal_width` (entero, **px**).
- **Default**: **1** (los temas que no la definen quedan idénticos a hoy: retrocompatibilidad
  cero — el motor sólo lee la clave cuando el archivo la trae).
- **Rango**: **1..8**. El parser (`src/theme.c`, `load_file()`) clampea: un `0` o un valor
  negativo cae a 1 (la línea no puede desaparecer) y cualquier cosa por encima de 8 a 8 (no
  puede tapar la cascada). El default del struct (`use_builtin()`) es 1.
- **Los temas outdoor**: `themes/Sunlit.json` y `themes/Trail.json` la fijan en **3**. Los otros
  trece temas servidos (Redshift, Sprint, Cathode y Aurora, más los seis incorporados —IU1WXY,
  Legacy, Outdoor, Classic, Outdoor-Negative y VISION— y los tres archivos legados —Holo,
  Material 3 y RS—) **no la setean**: quedan en 1 px y sus renders no cambian.
- **WCAG**: la clave **no** cambia el color de la línea (sigue siendo `panel_border`), sólo su
  ancho, así que no hay un par nuevo que medir; el ancho no interviene en el contraste (WCAG
  1.4.11 mide color, no grosor).

El test nativo (`tests/native/test_theme.c`) fija el parse: Sunlit y Trail en 3, un tema que no
la define en 1, y el clamp (`0` → 1, `99` → 8). Los renders que cambian son las fixtures `-agua`
de Sunlit y Trail (las que dibujan la línea, con `waterfall_center_line` encendido): los píxeles
que se mueven son la propia línea —la región `waterfall`, más los de la misma línea que caen en
la banda de botones, porque el objeto del waterfall llega hasta el fondo de la pantalla—; el
resto de la suite queda byte-idéntico.

## Para agregar uno propio

1. Copiá tu archivo a la partición **DATA**, en `themes/`: `/mnt/themes/Mi tema.json`
   (o `/mnt/themes/Mi tema/theme.json` si trae imágenes al lado).
2. Un tema de DATA con el nombre de uno de los seis del selector lo **reemplaza** (el
   directorio DATA gana sobre la ROM), sin recompilar nada. Un nombre propio —que no sea uno
   de los seis— **no aparece en el menú** (issue #73): el motor lo carga si algo lo pide por
   nombre, pero el selector ofrece sólo la colección. Convertirlo en una séptima opción de
   menú es un cambio de producto, no un archivo suelto.

## Tests

`tests/native/test_theme.c` prueba los incorporados, la carga de un JSON, el respaldo cuando
el nombre no existe y —con `X6200_SHIPPED_THEMES` apuntando a `rootfs/usr/share/x6200/themes`—
que el **selector ofrezca exactamente los seis v2** (`theme_list()`), que los seis **carguen**
y que los legados (Holo, Material 3, RS) sigan cargando por nombre pero **no** aparezcan en la
lista. Si un tema del selector falta, o si un legado se cuela en la lista, el test falla: es lo
que garantiza la oferta de seis del menú del equipo y no sólo la presencia de los archivos en
la imagen.

## Sunlit — sol y exteriores (THEMES v2, issue #67)

Tema de **exteriores/día**: fondo **marfil cálido** (`bg #F7F5EB`, `bar #E7E4D8`, `panel #FFFFFF`),
lecturas en **azul profundo** (`text #162436`, `text_value #162436`, `vfo #03457F`, `text_head
#053F80`) y **amarillo** para la acción/atención (`btn_active #FFD43B`, `accent #0355A7`, foco
`#054B9B`). Los roles "fríos" que el esquema hereda del `base` se reescriben en la familia
cálida/azul para que ningún rol de `Outdoor` se cuele. Fuente **Noto Sans** (`"font": "notosans"`,
la familia compilada en ROM desde #60, sin descargar nada), **sin** `font_digits` (frecuencia y
reloj con la familia por defecto). Bordes **firmes** (`radius 2`, `radius_btn 3`, `radius_popup
4`, `border 2`), sin gradientes (`btn_gradient`/`box_gradient` en `false`), foco sólido y
`pill_border 2`. El waterfall (`#FFFFFF → #DAEAF3 → #A7CDE5 → #568FBF → #15528A → #102845`) lleva
el piso de ruido al marfil y la señal al azul profundo.

**Es una meta de diseño, no una garantía medida.** "Alta visibilidad a pleno sol" es la
intención del tema (fondo claro, contraste alto); el brillo real bajo el sol **no se midió** —
el arnés y QEMU no demuestran legibilidad al sol ni consumo de energía (LCD con backlight).

**Cobertura de roles.** El archivo escribe **122 de 122 roles** de `src/theme.h` (nada queda
heredado de `Outdoor`, que es su `base`): `x6200-ab themes` lo reporta como
`Sunlit (...; base Outdoor): 0/122 roles from the base`. El `base` es `Outdoor`, que **se retira
del menú** en #74: como el archivo define los 122 roles, la migración de la base no puede
contradecir ningún rol de Sunlit.

**Alcance real en la pantalla.** El motor de temas JSON colorea los roles que el firmware ya lee:
el fondo (`bg`), la fila de frecuencia (`text`), los bordes de panel (`panel_border`), la paleta
del waterfall, los items del info box (`bar`), los switches/sliders de Ajustes
(`control_border`/`panel`) y el recolor del botón activo/deshabilitado. Las cajas de la barra
superior (reloj, S-meter), los botones inferiores y el marco de los diálogos todavía se dibujan
con las imágenes fijas `images/top_*_dark.bin`, `btn_dark.bin` y `dialog_dark.bin`
(`src/styles.c`, `setup_theme_simple`), así que quedan en el gris azulado del asset en **todos**
los temas (ver *Limitaciones conocidas del motor* abajo). `theme_shape.radius`, `radius_btn`,
`btn_gradient`, `box_gradient`, `focus_solid`, `mode_box`, `pill_border`, `mark` e `images` se
cargan del JSON pero hoy el código no los usa en la pantalla principal: los radios y el borde de
Sunlit no son un delta respecto de los otros temas en esta revisión.

Pares medidos (WCAG 2.1, sRGB) para los roles de la identidad, todos por encima del umbral:

| Par (fg sobre bg) | Ratio | Mínimo |
|---|---:|---:|
| `text` sobre `bg` / `panel` | 14.34 / 15.67 | 4.5 |
| `text` sobre `bar` / `dialog` | 12.31 / 15.67 | 4.5 |
| `text` sobre `row` / `chip` | 13.83 / 12.61 | 4.5 |
| `text_value` sobre `bg` | 14.34 | 4.5 |
| `text_dim` sobre `bg` | 8.76 | 4.5 |
| `text_head` sobre `bar` | 8.12 | 4.5 |
| `btn_text` sobre `btn` | 11.98 | 4.5 |
| `btn_active_text` sobre `btn_active` | 11.15 | 4.5 |
| `focus_text` sobre `focus` | 8.45 | 4.5 |
| `sel_text` sobre `sel_bg` / `near_bg` | 10.83 / 13.67 | 4.5 |
| `vfo` sobre `vfo_bg` / `side` | 8.43 / 8.43 | 4.5 |
| `tx` sobre `tx_bg` | 5.55 | 4.5 |
| `mode_dig` sobre `bar` | 6.18 | 4.5 |
| `info` / `ok` / `err` / `accent` sobre `panel` | 7.10 / 6.49 / 6.76 / 7.34 | 4.5 |
| `control_border` sobre `dialog` / `panel` | 5.46 / 5.46 | 3.0 |
| `bar_border` / `btn_border` / `sel_border` sobre su fondo | 7.81 / 7.67 / 5.85 | 3.0 |
| `card` sobre `bg` | 9.10 | 3.0 |

`tests/check_wcag.py` mide **128 pares aplicables** a este tema y no reporta **ninguna falla**;
`x6200-ab themes` (los nueve pares que sólo viven en Ajustes) suma 9/9 OK. En el framework, el
**no-texto por píxeles** (WCAG 1.4.11) sobre el render dorado del tema
(`golden/main-tema-sunlit.png` y `-agua`) da el arco `meter_over` del S-meter **3.66:1** y la
columna de señal del waterfall **8.89:1** (mínimo 3:1), los dos declarados en
`tools/ab_testing/quality/nontext.yaml`. El arco del S-meter vive sobre la cara de bitmap fija
(`top_long.bin`, oscura) —por eso `meter_over` de un tema claro es una tinta **clara**,
`#FFC9B8`— y no sobre el `bg` del tema.

**Capturas por estado.** Las fixtures del arnés que rinden Sunlit (todas con el mismo estado de
radio que su gemela de otro tema, así el tema es la única variable): `main-tema-sunlit`
(principal, RX), `main-tema-sunlit-agua` (waterfall en movimiento), `main-tema-sunlit-tx` (TX
simulado), `main-tema-sunlit-eco` (batería baja + ECO), `bt-dialog-tema-sunlit` (diálogo) y
`settings-tema-sunlit` (Ajustes, con el foco sobre la fila del tema). La galería ARM real y la
comparación consolidada contra los otros cinco temas se cierran en el #73 (ver *Galería
consolidada*).

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

## Sprint — concursos / operación rápida (THEMES v2, issue #69)

**Qué es.** Un tablero de contest: **marino profundo** (`bg #091222`, `bar #11233E`,
`panel #071529`), **ámbar** para lo que pide atención (foco, botón activo, selección, títulos,
franja de band plan) y **cian hielo** para las lecturas (`vfo #7CE4EE`, barras del S-meter,
spots, `info`). El sistema de color es una jerarquía, no una paleta: ámbar = acción/atención,
cian = lectura, coral (`#FF8E83`/`#FF6E5C`) = TX y alarma, verde = OK. El waterfall
(`#030A17 → #173050 → #21689F → #2DBFCB → #F6D47D → #FF825E`) lleva el piso de ruido al marino
y la señal del cian al ámbar: la misma lectura de jerarquía en la cascada.

**Forma y fuentes.** `font: robotocondensed` (la familia compilada en ROM desde #60, sin
descargar nada) y `font_digits: dseg14`: los paneles de dígitos —la fila de frecuencia y el
reloj— dibujan con DSEG14 en los slots reales 30/38 (`font_digit()`, `src/styles.c`), con los
separadores y el `8` fantasma que trae la familia segmentada (fixtures `main-frecuencia-dseg`
y las de Sprint). El nombre del tema entra en `params.theme_name` entero (`Sprint`, 6 de 31
caracteres), así que sobrevive el round-trip y el tema queda elegido también en una
instalación actualizada (contrato `theme_name_fits_params`).

La prioridad de la fuente no cambia con este tema: la que nombra el tema **manda** sobre
`params.font` (el orden de `src/main.c`: `theme_shape.font` primero, después la elección del
usuario), y `font_digits` sólo toca los paneles de dígitos. Por eso en la fixture
`settings-tema-sprint` la fila *Font* de Ajustes sigue mostrando la elección persistida
(`Fira Sans`) mientras la pantalla se dibuja con Roboto Condensed: es el comportamiento
documentado, no una divergencia de esta corrida.

`radius 0 / radius_btn 2 / radius_popup 2 / border 2`, sin gradientes (`btn_gradient` y
`box_gradient` en `false`) y `focus_solid: true`: cantos angulares. **Ojo con lo que la
revisión actual dibuja**: `theme_shape.radius`, `radius_btn`, `btn_gradient`, `box_gradient`,
`focus_solid`, `mode_box`, `pill_border`, `mark` e `images` se cargan del JSON pero hoy el
código no los usa en la pantalla principal (`src/styles.c` fija `radius 0` y dibuja la barra
de botones con el bitmap de la ROM, y `theme_image()` no tiene llamadores): el ángulo del
tema no es un delta respecto de los otros temas en esta revisión, y `btn`/`btn_text` se ven
sólo por el recolor de botón activo/deshabilitado (`btn_mark_active` / `bg` + `text_mute`).
Los roles se miden igual —`x6200-ab themes` los mide estáticamente— y quedan declarados acá
para que el día que la superficie de forma vuelva a dibujarse el tema ya la traiga.

**Cobertura de roles.** El archivo escribe **122 de 122 roles** de `src/theme.h` (nada queda
heredado de `Classic`, que es su `base`): `x6200-ab themes` lo reporta como
`Sprint (...; base Classic): 0/122 roles from the base`. La cobertura semántica del issue:
frecuencia (`text`/`text_value`, dígitos DSEG14), modo (`mode_dig`, `sel_bg`/`sel_border`,
`info`), selección (`sel_bg`/`sel_text`/`sel_border`, `near_bg`, `focus_bg`), foco
(`focus`/`focus_text`), botones (`btn*`, `btn_off_*`, `btn_page*`, `btn_mark*`),
S-meter (`meter_weak`/`meter_bar`/`meter_low`/`meter_high`/`meter_over`), RX/TX
(`tx`/`tx_bg`/`tx_box`, `alc`, `bar_high`/`bar_mid`), warning/error (`warn*`, `err*`, `ok*`)
y la paleta del waterfall.

**WCAG 2.1 (medido, no estimado).** `tests/check_wcag.py` mide **128 pares** aplicables de
Sprint con **0 fallas** (texto 4.5:1, no-texto 3:1); los ratios que fijan la identidad:
`text/bg` 17.44:1, `text/panel` 17.03:1, `btn_text/btn` 10.60:1,
`btn_active_text/btn_active` 9.78:1, `focus_text/focus` 11.79:1, `sel_text/sel_bg` 6.92:1,
`text_head/bar` 10.98:1, `mode_dig/bar` 8.15:1, `control_border/dialog` 4.78:1,
`bar_border/bar` 3.53:1, `btn_border/btn` 3.94:1, `sel_border/sel_bg` 4.68:1, `card/bg` 3.14:1.
Los colores apagados a propósito (`btn_off_text`, `text_mute`, `text_faint`, `icon_off`,
`switch_off`, `off`, `track`) siguen la exención *disabled/incidental* de WCAG 1.4.3 y se
informan sin hacer fallar el chequeo. En el framework, `x6200-ab themes` mide además los pares
que sólo existen en Ajustes (9/9 OK) y el **no-texto por píxeles** (WCAG 1.4.11) sobre el
render dorado del tema: el arco `meter_over` del S-meter da **5.46:1** y la columna de señal
del waterfall **8.31:1** (mínimo 3:1), los dos declarados en
`tools/ab_testing/quality/nontext.yaml`.

**Capturas por estado.** Las fixtures del arnés que rinden Sprint (todas con el mismo estado
de radio que su gemela de otro tema, así el tema es la única variable):
`main-tema-sprint` (principal, RX, con DSEG14), `main-tema-sprint-agua` (waterfall en
movimiento), `main-tema-sprint-tx` (TX simulado), `main-tema-sprint-eco` (batería baja + ECO),
`bt-dialog-tema-sprint` (diálogo) y `settings-tema-sprint` (Ajustes, con el foco sobre la fila
del tema). El detalle de la corrida está en `docs/AB_TESTING.md`.

## Cathode — tubo CRT sepia/ámbar (THEMES v2, issue #70)

**Qué es.** Instrumentación **CRT sepia/ámbar**: vidrio **café-negro** (`bg #100B07`,
`bar #24170C`, `panel #140D08`) con lecturas **ámbar/crema luminiscentes** (`text #F9D9AA`,
`text_value #FFE7BF`, `vfo #FFD59B`) y **cobre** para lo que pide atención (foco, botón activo,
selección, encabezados: `accent #F1BE75`, `sel_bg #5E3A19`, `focus #F5C27C`). No es un
videojuego ni una TV skeuomorphic: **no hay gradientes, glow, scanlines ni trails**; la
asociación CRT sale de la **tipografía, el color y la jerarquía**. El sistema de color es una
jerarquía de un solo tono: ámbar = acción/atención, crema = lectura, y el **naranja-rojizo**
(`tx #FFA27A`, `err #FF9B77`, `btn_mark_active #FF9760`) para TX y alarma, que se distingue por
forma/posición además del tono. El waterfall
(`#090605 → #241308 → #4F2B0E → #86511D → #C78A3D → #F2C57C → #FFF1CB`) es **térmico ámbar
monocromático**: negro → espresso → burnt umber → copper → ámbar → crema pergamino, con los
niveles de señal visiblemente distintos.

**Forma y fuentes.** `font: ibmplexsans` (la familia compilada en ROM desde #60, sin descargar
nada) y `font_digits: dseg14`: los paneles de dígitos —la fila de frecuencia y el reloj— dibujan
con DSEG14 en los slots reales 30/38 (`font_digit()`, `src/styles.c`), con los separadores y el
`8` fantasma que trae la familia segmentada (fixtures `main-tema-cathode` y hermanas, y
`main-frecuencia-dseg`). Cantos **casi cuadrados** (`radius 1`, `radius_btn 2`, `radius_popup 3`,
`border 1`), sin gradientes (`btn_gradient`/`box_gradient` en `false`) y `focus_solid: true`. El
nombre del tema entra en `params.theme_name` entero (`Cathode`, 7 de 31 caracteres), así que
sobrevive el round-trip y el tema queda elegido también en una instalación actualizada (contrato
`theme_name_fits_params`).

La prioridad de la fuente no cambia con este tema: la que nombra el tema **manda** sobre
`params.font` (el orden de `src/main.c`: `theme_shape.font` primero, después la elección del
usuario), y `font_digits` sólo toca los paneles de dígitos. Por eso en la fixture
`settings-tema-cathode` la fila *Font* de Ajustes sigue mostrando la elección persistida
(`Fira Sans`) mientras la pantalla se dibuja con IBM Plex Sans: es el comportamiento
documentado, no una divergencia de esta corrida.

**Ojo con lo que la revisión actual dibuja**: `theme_shape.radius`, `radius_btn`, `btn_gradient`,
`box_gradient`, `focus_solid`, `mode_box`, `pill_border`, `mark` e `images` se cargan del JSON
pero hoy el código no los usa en la pantalla principal (`src/styles.c` fija `radius 0` y dibuja
la barra de botones con el bitmap de la ROM, y `theme_image()` no tiene llamadores): el ángulo
del tema no es un delta respecto de los otros temas en esta revisión. Los roles se miden igual
—`x6200-ab themes` los mide estáticamente— y quedan declarados acá para que el día que la
superficie de forma vuelva a dibujarse el tema ya la traiga.

**Cobertura de roles.** El archivo escribe **122 de 122 roles** de `src/theme.h` (nada queda
heredado de `Classic`, que es su `base`): `x6200-ab themes` lo reporta como
`Cathode (...; base Classic): 0/122 roles from the base`. El JSON de partida del issue #70
nombraba tres claves que ya no existen en el esquema (`strip`/`strip_grad`/`strip_border` y
`mode_text`/`mode_bg`/`mode_border`, podadas en #77): incluirlas habría dejado el tema con
colores desconocidos, así que se reasignaron a sus roles reales (`bar_grad`/`panel`, y
`sel_bg`/`sel_text`/`mode_dig`). La cobertura semántica del issue: frecuencia
(`text`/`text_value`, dígitos DSEG14), modo (`mode_dig`, `sel_bg`/`sel_border`, `info`),
selección (`sel_bg`/`sel_text`/`sel_border`, `near_bg`, `focus_bg`), foco (`focus`/`focus_text`),
botones (`btn*`, `btn_off_*`, `btn_page*`, `btn_mark*`), S-meter
(`meter_weak`/`meter_bar`/`meter_low`/`meter_high`/`meter_over`), RX/TX (`tx`/`tx_bg`/`tx_box`,
`alc`, `bar_high`/`bar_mid`), warning/error (`warn*`, `err*`, `ok*`) y la paleta del waterfall.
Ningún rol del `base` Classic (blancos, celestes, azules, violetas) se cuela: la identidad
café/sepia se mantiene en los 122.

**WCAG 2.1 (medido, no estimado).** `tests/check_wcag.py` mide **128 pares** aplicables de
Cathode con **0 fallas** (texto 4.5:1, no-texto 3:1); los ratios que fijan la identidad:
`text/bg` 14.48:1, `text/panel` 14.24:1, `text_value/bg` 16.25:1, `btn_text/btn` 10.45:1,
`btn_active_text/btn_active` 6.05:1, `focus_text/focus` 11.00:1, `sel_text/sel_bg` 8.06:1,
`text_head/bar` 12.13:1, `mode_dig/bar` 14.33:1, `tx/tx_bg` 5.75:1, `control_border/dialog`
4.26:1, `bar_border/bar` 3.14:1, `btn_border/btn` 3.67:1, `sel_border/sel_bg` 5.80:1,
`card/bg` 4.90:1. Los colores apagados a propósito (`btn_off_text`, `text_mute`, `text_faint`,
`icon_off`, `switch_off`, `off`, `track`) siguen la exención *disabled/incidental* de WCAG 1.4.3
y se informan sin hacer fallar el chequeo. En el framework, `x6200-ab themes` mide además los
pares que sólo existen en Ajustes (9/9 OK) y el **no-texto por píxeles** (WCAG 1.4.11) sobre el
render dorado del tema: el arco `meter_over` del S-meter da **8.11:1** y la columna de señal del
waterfall **6.82:1** (mínimo 3:1), los dos declarados en
`tools/ab_testing/quality/nontext.yaml`.

**Capturas por estado.** Las fixtures del arnés que rinden Cathode (todas con el mismo estado
de radio que su gemela de otro tema, así el tema es la única variable):
`main-tema-cathode` (principal, RX, con DSEG14), `main-tema-cathode-agua` (waterfall en
movimiento), `main-tema-cathode-tx` (TX simulado), `main-tema-cathode-eco` (batería baja + ECO),
`bt-dialog-tema-cathode` (diálogo) y `settings-tema-cathode` (Ajustes, con el foco sobre la fila
del tema). La galería ARM real y la comparación consolidada contra los otros cinco temas se
cierran en el #73; el selector/default se migra en el #74. El detalle de la corrida está en
`docs/AB_TESTING.md`.

## Galería consolidada (THEMES v2, issue #73)

Las **hojas de contactos** de la colección completa —los seis temas v2 en el **mismo estado de
radio**, así el tema es la única variable— viven en
`docs/evidencia/2026-10-15-themes-integration-73/galeria/` (renders reales del arnés de UI,
800×480, LVGL 8; no ilustraciones):

| Hoja | Estado | Fixture por tema |
|---|---|---|
| `galeria-principal.png` | pantalla principal, RX | `main-tema-<tema>` |
| `galeria-ajustes.png` | Ajustes → Apariencia, foco en la fila del tema | `settings-tema-<tema>` |
| `galeria-dialogo.png` | diálogo de app | `bt-dialog-tema-<tema>` |
| `galeria-tx.png` | TX simulado | `main-tema-<tema>-tx` |
| `galeria-cascada.png` | waterfall en movimiento | `main-tema-<tema>-agua` |
| `galeria-eco.png` | batería baja + ECO | `main-tema-<tema>-eco` |

Las compone `tools/galeria-temas.py` (Pillow, sin red) a partir de los renders que deja
`x6200-ab capture`; el comando exacto está en el README de la carpeta. Cada tema, en una línea:
Sunlit (día, marfil/azul/amarillo, Noto Sans), Redshift (noche, negro/rojo cálido, Source Sans
3), Sprint (concurso, marino/ámbar/cian, Roboto Condensed + DSEG14), Cathode (CRT sepia/ámbar,
IBM Plex Sans + DSEG14), Trail (campo, bosque/oliva/arena, IBM Plex Sans) y Aurora (diario,
berenjena/lavanda/coral, Inter).

**Los temas no se asocian a ahorro del LCD.** El fondo oscuro no ahorra Wh: el LCD tiene
backlight, y el brillo real no se mide acá (ni el arnés ni QEMU lo demuestran).

## Limitaciones conocidas del motor (gaps, no del tema)

La revisión actual dibuja algunos roles con assets fijos y deja roles declarados sin uso. No se
migran en la colección (el epic #66/#73 no lo pide: son refactors de motor aparte); quedan
documentados acá para que la ficha de un tema no prometa lo que el render no hace.

1. **Bitmaps fijos (`theme_image()` no tiene llamadores).** `src/styles.c`
   (`setup_theme_simple`) pinta la barra superior (`top_short_dark.bin`, `top_long_dark.bin`,
   `top_big_dark.bin`), los botones inferiores (`btn_dark.bin`), el marco de los diálogos
   (`dialog_dark.bin`), los paneles (`panel_dark.bin`) y los mensajes (`msg_dark.bin`,
   `msg_tiny_dark.bin`) con **imágenes de la ROM, iguales en todos los temas**. Por eso las cajas
   del reloj/S-meter, la barra de botones y el marco de los diálogos quedan en el gris azulado
   del asset en los quince temas, y `theme_image()` (el `images` de un tema JSON) se carga pero
   hoy nunca se dibuja.
2. **`bp_*` declarados pero no leídos.** Los roles `bp_cw`/`bp_dig`/`bp_ssb`/`bp_bcn`/`bp_text`
   (`src/theme.h`) están en el esquema y `tests/check_wcag.py` los mide, pero `src/band_plan.c`
   dibuja la franja del band plan con una paleta **literal** (`{0x3366CC, 0x33AA55, 0x888888,
   0xCC8800}`, Clase 2 de literales) y elige la etiqueta por luminancia, así que un tema no
   cambia los colores del band plan todavía.
3. **La forma declarada no se dibuja.** `theme_shape.radius`/`radius_btn`/`radius_popup`,
   `btn_gradient`/`box_gradient`, `focus_solid`, `mode_box`, `pill_border`, `mark` e `images` se
   cargan del JSON pero `src/styles.c` fija `radius 0` y usa el bitmap de la barra: en esta
   revisión los radios, los gradientes y la caja de modo **no son un delta** entre temas (quedan
   declarados para el día que esa superficie vuelva a ser del tema).

La migración de estos gaps a un esquema por tema, si se decide, es trabajo de motor con su
propio issue.
