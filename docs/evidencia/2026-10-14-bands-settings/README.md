# Evidencia: la app Bands no abría desde el menú de apps (banco QEMU, sin radio)

Repro y verificación del defecto reportado por el owner el **2026-10-14**:
*"el menú Bands no hace nada cuando lo ejecuto desde los settings"*.

Se hizo en el **banco** (binario ARM real de la imagen con `qemu-arm-static`,
framebuffer falso y control remoto por socket): el mismo camino que la radio.
Ver el skill `.agents/pruebas-sin-radio/SKILL.md`.

- **Antes**: commit `89851e6`, binario `md5 a31dbc8263eb`.
- **Después**: commit `9ea0353` (el fix), binario `md5 061f8b88dea9`.
- La GUI se manejó por su socket con `tools/remote/x6200-remote`; la ruta del
  owner (tecla **APP** → páginas APP) es el comando `{"cmd":"page","text":"app"}`
  (llama al mismo `buttons_load_page_group(buttons_group_app)` que la tecla APP),
  y los botones de la barra son `{"cmd":"btn","val":N}`.

## La secuencia del repro (ruta del owner, exacta)

| # | Comando | Lo que muestra | PNG |
|---|---|---|---|
| 1 | (ninguno) | pantalla principal | `01-home-antes.png` |
| 2 | `{"cmd":"page","text":"app"}` | la barra queda en `(APP 1:6)` FT8/JS8/WSPR/RTTY | `07-ruta-owner-pagina1.png` |
| 3 | `{"cmd":"btn","val":0}` ×2 | la barra queda en `(APP 3:6)` Recorder/**Field Ops**/GPS/**Bands** | `08-ruta-owner-pagina3.png` |
| 4 | `{"cmd":"btn","val":4}` (botón **Bands**) | **NADA**: la pantalla no cambia (solo el reloj) | `09-ruta-owner-bands-nada.png` |
| 5 | `{"cmd":"btn","val":3}` (botón **GPS**, control en la MISMA barra) | GPS abre | `10-control-gps-abre.png` |

El paso 4 se repitió con el mismo resultado: el botón `Bands` de las páginas APP
no hace nada, mientras que los otros botones de esa misma página (GPS, Field Ops,
Recorder) sí abren su app. Es el defecto reportado.

## La misma ruta por el control remoto (para descartar que sea del banco)

El comando `{"cmd":"app","text":"Bands"}` llama a `main_screen_start_app(ACTION_APP_BANDS)`,
**exactamente lo mismo que hace el botón** (`make_app_btn` → `button_app_page_cb`).

| # | Comando | Lo que muestra | PNG |
|---|---|---|---|
| 6 | `{"cmd":"app","text":"Bands"}` desde la principal | **NADA** | `04-app-bands-nada.png` |
| 7 | `{"cmd":"app","text":"Status"}` (control) | Status abre | `05-control-status-abre.png` |
| 8 | `{"cmd":"app","text":"About"}` | **NADA** (mismo defecto, misma causa: el botón About de `APP 5:6` usa el mismo `make_app_btn` y `ACTION_APP_ABOUT` tampoco tiene `case` en `main_screen_start_app`) | `06-control-about-nada.png` |

## La ruta desde Ajustes (el long press)

Ajustes se abre con `{"cmd":"app","text":"Settings"}` (`02-ajustes-abierto.png`).
La acción de Bands que ofrece la lista del long press de Ajustes
(`APP Bands`, `src/dialog_settings.c`) se despacha en `main_screen_action()` y
solo es alcanzable asignándola a un long press; por eso en el banco se ejercita
por el mismo camino que el resto de las apps (`{"cmd":"app","text":"Bands"}`),
que es el que comparte el botón del menú.

- `{"cmd":"app","text":"Bands"}` con Ajustes abierto **antes del fix**:
  `03-ajustes-tras-bands-nada.png` — Ajustes se cierra y **no abre nada**.

## Después del fix (commit `9ea0353`)

| # | Secuencia | Lo que muestra | PNG |
|---|---|---|---|
| 1 | `{"cmd":"page","text":"app"}` + `btn 0` ×2 | `(APP 3:6)` | `12-post-fix-pagina3.png` |
| 2 | `btn 4` (botón **Bands**) | **la ventana de Bands abre**: "BANDS 21.2500 MHz / RBN connected", las 12 bandas con su barra y los spots de cluster/RBN, y la barra `(BANDS 1:1) Test beacon Use band` | `13-post-fix-bands-abre.png` |
| 3 | `{"cmd":"app","text":"Settings"}` + `{"cmd":"app","text":"Bands"}` | Ajustes se cierra y Bands abre (con datos reales: 40m HIGH, 15m/6m POOR) | `14-post-fix-bands-desde-ajustes.png` |

## La causa

`main_screen_start_app()` no tenía `case ACTION_APP_BANDS`: el botón del menú de
apps llama a esa función (`buttons.cpp`, `make_app_btn`) y caía en el `default`.
La acción se despachaba **solo** en `main_screen_action()`, que además construía
`dialog_bands` **sin cerrar** la ventana abierta (el long press de Ajustes dejaba
la ventana anterior huérfana). El fix manda `ACTION_APP_BANDS` por
`main_screen_start_app()`, como el resto de las apps.

## `harness-app-bands.png`

Es la misma ruta en el **arnés de host** (`tools/ui_harness`, fixture `app-bands`),
no en el banco. `dialog_bands` es un mock inerte en el arnés: la barra de abajo
queda vacía porque la ventana que la cargaría no se compila. Sirve para ver que la
fixture mide **la ruta** (una aserción sobre el estado del firmware), no los
píxeles de la ventana; por eso el banco es la evidencia de que la ventana abre.
