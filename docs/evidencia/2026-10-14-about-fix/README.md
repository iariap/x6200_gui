# Evidencia: About abre desde el menú de apps (banco QEMU, sin radio)

Follow-up del run `bands-settings`: el mismo defecto de Bands dejaba muerto el botón
**About** de `APP 5:6`. El owner lo había reportado como "el menú About no hace nada"
(`docs/evidencia/2026-10-14-bands-settings/README.md`, paso 8, `06-control-about-nada.png`).

Se verificó en el **banco** (binario ARM real de la imagen con `qemu-arm-static`,
framebuffer falso y control remoto por socket): el mismo camino que la radio.
Ver el skill `.agents/pruebas-sin-radio/SKILL.md`.

- **Antes**: commit `0684861`, binario `md5 061f8b88dea9` (el mismo post-fix de Bands:
  el case de About no existía en ninguno de los dos).
- **Después**: tip `9224734`, binario `md5 7f2c94ecb2c7`.

Las capturas salen de `banco/`. Todas menos las `before-*` usan el binario con el fix.

## La ruta del owner (tecla APP → páginas APP → About)

El botón "About" de las páginas APP (`src/buttons.cpp`, `make_app_btn`) llama a
`main_screen_start_app(ACTION_APP_ABOUT)`, que **no tenía `case`**: caía en el `default`.
La acción se despachaba solo en `main_screen_action()`, que el botón no usa.

| # | Comando | Lo que muestra | PNG |
|---|---|---|---|
| 1 | (ninguno) | pantalla principal | `banco/01-home.png` |
| 2 | `{"cmd":"page","text":"app"}` | la barra queda en `(APP 1:6)` FT8/JS8/WSPR/RTTY | `banco/02-ruta-owner-pagina-app.png` |
| 3 | `{"cmd":"btn","val":0}` ×4 | la barra queda en `(APP 5:6)` Settings/Callsign/QTH/**About** | `banco/03-ruta-owner-pagina-5.png` |
| 4 | `{"cmd":"btn","val":4}` (botón **About**) | **abre la ventana real de About** ("About", "Version v0.1.2-lu2dsx", "73 de LU2DSX", "Made with love by LU2DSX") | `banco/04-ruta-owner-about-abre.png` |

El recorrido del párrafo de arriba, con el **binario sin el fix** (`md5 061f8b88dea9`):
los pasos 1-3 son idénticos y el paso 4 **no cambia nada** (la pantalla sigue en
`(APP 5:6)`, solo avanza el reloj) — `banco/before-04-ruta-owner-about-nada.png`.

## La misma ruta por el control remoto (mismo dispatcher que el botón)

`{"cmd":"app","text":"About"}` llama a `main_screen_start_app(ACTION_APP_ABOUT)`,
**exactamente lo mismo que hace el botón** (`make_app_btn` → `button_app_page_cb`).

| # | Comando | Con el fix | Sin el fix |
|---|---|---|---|
| 5 | `{"cmd":"app","text":"About"}` desde la principal | About abre (`banco/07-app-about-abre.png`) | **NADA** (`banco/before-05-app-about-nada.png`) |

(La captura `banco/05-control-app-about.png` sale del recorrido largo con About **ya**
abierto: la orden es un no-op porque `dialog_construct()` no rearma un diálogo que ya
corre. La evidencia limpia de la orden es `07-app-about-abre.png`, con GUI fresca.)

El paso 4 y el 5 son el **mismo** `main_screen_start_app()`: que el botón real de la
barra y el comando del control remoto abran los dos la ventana es la prueba de que el
fix está en el dispatcher que comparten, no en uno de los dos caminos.

## Nota honesta sobre el banco: el reinicio por handshake

`tools/captura-banco.sh` encadena capturas dejando la GUI viva, pero en esta máquina el
**handshake `{"cmd":"get"}` falla en cada llamada** contra una GUI ya arrancada y el
script la **reinicia** (mismo síntoma en el preflight: "el banco no contesta o la GUI no
esta viva; reiniciando..."): la barra vuelve a `(VOL …)` entre llamadas, así que una
ruta de varios pasos por el script mide un reinicio, no el recorrido. Por eso las
capturas de `banco/` se hicieron con un **driver de una sola corrida** (arranca la GUI
una vez, manda las órdenes por el mismo `x6200-remote` y vuelca `tmp/fakefb0` a PNG sin
reiniciar); la GUI quedó viva los ~40 s que duró el recorrido en los dos lados
(`06-home-fresco.png` es de una corrida fresca). No se tocó `captura-banco.sh`: el
defecto del handshake es del banco, no del cambio, y queda como **PENDIENTE**.

## El arnés de host

`tools/ui_harness/` tiene una fixture nueva, `app-about` (la ruta del menú: páginas APP →
botón About de `APP 5:6`). `dialog_about` es **real** en el host, así que la fixture
renderiza la ventana real de About y `scenario_settle` corta la corrida si el botón no la
abre (`dialog_about->run`). Sin el case, **exit 1**; con el fix, **exit 0**
(transcript: `arnes-antes-despues.txt`). `harness-app-about.png` es lo que renderiza con
el fix: **byte a byte idéntico** a `dialog-about` (misma ventana por los dos caminos,
mismo sha256 en el gate de determinismo).

`dialog_about` es real (a diferencia de `dialog_bands`, que sigue siendo un mock en
`app-bands`): por eso esta fixture sí mide la **ventana**, no solo la ruta.
