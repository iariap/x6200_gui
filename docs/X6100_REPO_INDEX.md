# Índice del repo del X6100 (IU1WXY) y plan de uso

Catálogo de **todo** lo que hay en [`IU1WXY/X6100-IU1WXY`](https://github.com/IU1WXY/X6100-IU1WXY)
y para qué nos sirve en el X6200. Es la referencia que venimos usando para portar features
(ver [X6100_FEATURE_GAP.md](X6100_FEATURE_GAP.md) y [NEW_FEATURES.md](NEW_FEATURES.md)).

## Cómo tener la referencia local (reproducible)

```bash
git clone --depth 1 https://github.com/IU1WXY/X6100-IU1WXY.git .x6100_ref
cd .x6100_ref && git submodule update --init --recursive --depth 1   # lvgl (fork NEON de gdyuldin)
```

> El submódulo `lvgl` **hay que inicializarlo**: sin él, el arnés de UI del propio repo no
> compila y varios módulos no tienen sus headers. Los otros directorios que parecen
> submódulos (`third-party/js8core`, `third-party/wsprd`, `third-party/cty`) están vendorizados
> y vienen en el clon.

## Mapa del repo

| Carpeta | Contenido | Tamaño | Para nosotros |
|---|---|---|---|
| `src/` | 436 archivos: GUI + módulos de modos digitales, cluster, field… | 10 MB | ✅ portado en parte · 🔜 resto |
| `tools/` | Arnés de UI, *loopbacks* de DSP, generadores de assets | 210 MB | ✅ método del arnés verificado · 🔜 el resto |
| `tests/` | Suite Catch2 con mocks (gpiod, cfg, storage) | 500 KB | 🔜 infra reutilizable |
| `test-audio/` | `js8_test.wav` de ejemplo | 976 KB | 🔜 al portar JS8 |
| `third-party/` | `js8core` (380), `wsprd` (15), `cty` (2) | 22 MB | 🔜 motores listos para portar |
| `rootfs/` | Overlay que se instala en el equipo (scripts + assets) | 1,1 MB | ✅ parte aplicada · 🔜 assets |
| `themes/` | 2 temas JSON + assets `RS/` + generadores | 480 KB | 🔜 cuando portemos temas |
| `webserver/` | App Python/bottle, 12+ páginas | 472 KB | 🔜 base del web configurator ampliado |
| `docs/` | Manual (778 líneas) y planes de diseño | 11 MB | 📖 referencia |
| `sql/` | CSVs de bandas, modos, memorias, params | 48 KB | 📖 comparar con los nuestros |
| `images/` | Assets del panel/mensajes `.bin` | 1,6 MB | 📖 ya tenemos equivalentes |
| `boot/` | `logo.png` | 24 KB | ➖ |
| `lv_drivers/` | Drivers LVGL (fbdev, etc.) | 116 KB | 📖 |
| `lvgl/` | Submódulo: fork NEON de gdyuldin | — | 📖 el nuestro también es un fork |
| `u-boot/` | — | — | ➖ **removido**: el boot se construye desde fuente en el Buildroot (`uboot-2023.01`, 556.120 B, md5 cd22949d…). El prebuilt vendor viejo (584.708 B, md5 4512f2ba…) no se usaba (paso de CI comentado) |
| `buildroot/` | Scripts de build de la app (`build.sh`, `bt_start.sh`) | 20 KB | 📖 |

## `tools/` — lo más valioso

| Herramienta | Qué hace | Uso previsto |
|---|---|---|
| `apps_ui_harness/` | Renderiza los diálogos reales (FT8, JS8, SSTV, WeFax, NavTex, Cluster, Ajustes, Wireless, topbar) **headless en el PC**, con auto-stubs de los símbolos del firmware | ✅ método verificado en [UI_HARNESS.md](UI_HARNESS.md); 🔜 adaptarlo a nuestras fuentes |
| `js8_ui_harness/` | Igual pero corriendo la app JS8 real con su decodificador; además trae `fetch_deps.sh` que baja LVGL 8.3.11, `ft8_lib`, `X6100Control` y `liquid-dsp` | ✅ usé su `fetch_deps.sh`; 🔜 base del nuestro |
| `ft8_loopback.c` | Driver de ida y vuelta del codec FT8 | 🔜 al tocar FT8/FT4 |
| `sstv_loopback.c` | Driver del codec SSTV (RX/TX) | 🔜 al portar SSTV |
| `wefax_loopback.c` | Driver del codec WeFax | 🔜 al portar WeFax |
| `navtex_loopback.cpp` | Driver del decodificador NavTex | 🔜 al portar NavTex |
| `js8_wavgen/` | Genera WAV de JS8 para pruebas | 🔜 al portar JS8 |
| `flags_gen.py`, `world_map_gen.py` | Generan banderas y mapamundi a partir de `cty.dat` | 🔜 para el cluster y el mapa de QSO |
| `field_icons.py`, `font_gen.py`, `boot_logo.py` | Generan iconos de Field Ops, fuentes y logo | 🔜 al portar Field Ops / temas |

**Los *loopbacks* son la clave para los modos digitales**: permiten validar los codecs en el host
(entrada WAV → salida decodificada) sin radio, que es exactamente la disciplina de pruebas que
venimos usando.

## `tests/` y `test-audio/` — infra reutilizable

Suite con **Catch2 + ASan/UBSan** y, sobre todo, **mocks**:

- `tests/mock_gpiod/gpiod.h` → deja compilar el código que habla con GPIO en el host.
- `tests/cfg/mocks/{mock_pending_writes,mock_storage}.h` → mocks del motor de configuración.
- Casos: `test_js8.cpp`, `test_sstv.c`, `test_wefax.c`, `test_wspr.cpp`, `test_rtty_tx.c`,
  `test_tx_level.cpp`, `test_cluster.cpp`, `test_field.cpp`, `test_cw_decoder/`,
  `test_cfg/*` (atu_cache, computed, db, digital_modes, manager, memory), `test_x6100_field.py`.

**Uso previsto**: tomar el patrón (Catch2 + mocks) para nuestros tests nativos y, cuando
portemos los modos digitales, reutilizar sus casos como referencia de comportamiento esperado.

## `rootfs/` — lo que se instala en la radio

| Archivo | Para qué | Estado |
|---|---|---|
| `etc/default/btcat`, `etc/init.d/S94btcat`, `usr/sbin/btcat-{setup,ctl}`, `usr/sbin/btaudio-ctl`, `etc/pulse/system.pa.d/btaudio.pa`, `etc/init.d/S39btpairings` | Bluetooth: CAT por SPP y auriculares A2DP | ✅ **portado** (`09117f1`) |
| `etc/default/bluetoothd` | `BLUETOOTHD_ARGS="-n -C"`: el modo compat es necesario para que `sdptool` publique el registro SPP de btcat | ✅ **aplicado** (overlay del buildroot) |
| `etc/modprobe.d/x6100-blacklist.conf` | Blacklist de `sunxi_cedrus`, `joydev`, `bnep` | ✅ **aplicado** como `x6200-blacklist.conf` |
| `usr/share/x6100/sstv/` | 4 imágenes y 6 plantillas de texto para **SSTV TX** | 🔜 al portar SSTV |
| `usr/share/x6100/field/` | Iconos de Field Ops (bands/log/pota/sota en 64 y 96 px) | 🔜 al portar Field Ops |
| `usr/share/x6100/broadcast.csv` | Base de emisoras de radiodifusión | 🔜 si retomamos broadcast |
| `usr/sbin/x6100-field`, `x6100-pota-spots` | Scripts de soporte de Field Ops | 🔜 al portar Field Ops |
| `etc/init.d/S95gui`, `usr/sbin/x6100_daemon` | Arranque del GUI | ✅ tenemos equivalentes |

## `themes/`

- `Holo.json`, `Material 3.json`: temas completos (colores, formas, fuentes).
- `RS/`: assets del tema (bar.png, btn_active.png, btn_off.png…).
- `lum.py`, `zoom.py`, `make_rs.py`: generadores/ajustadores de assets.
- `README.md`: formato del tema.

**Uso previsto**: cuando encaremos el port del sistema de temas (el que toca 42 archivos con 428
referencias de color), esta carpeta es el modelo y trae los temas ya hechos.

## `webserver/` — el web configurator ampliado

App Python/bottle con plantillas: `backup`, `bands`, `cluster`, `cw_messages`, `digital_modes`,
`field`, `files`, `gallery`, `index`, `logs`, `memories`, `remote`, `time`, `wfview`
(+ `apps.py`, `models.py`, `settings.py`).

**Uso previsto**: base directa para el port de "web configurator ampliado" (HTTPS, control
remoto, backup, galería). Es Python, así que no depende del toolchain cruzado: se puede probar
en el host.

## `docs/` — referencia de diseño

| Documento | Líneas | Para qué nos sirve |
|---|---|---|
| `MANUAL.md` | 778 | Manual funcional completo: cada app, ajustes, páginas web, comandos CAT, archivos |
| `TX_PLAN.md` | 209 | Plan de la cadena de TX (compresión, CESSB, calibración) |
| `T6_PLAN.md` | 118 | Plan del T6/IQ |
| `THEMES.md` | 121 | Formato del sistema de temas |
| `SPOTS_PLAN.md` | 76 | Plan del cluster de spots |
| `RESEARCH.md` | 109 | Investigación de hardware/protocolo |
| `privacy-x6100-link.md` | 85 | Qué datos salen del equipo (útil para decidir qué NO portar) |
| `releases/`, `mockups/`, `screenshots/` | — | Bocetos y capturas de referencia |

## `src/` y `third-party/` — estado del port

Detalle completo en [X6100_FEATURE_GAP.md](X6100_FEATURE_GAP.md). Resumen:

- ✅ **Portado**: About, adaptive tuning, audio enhancement, UDP WSJT-X, aviso de versión, aguja
  S-meter, band plan, grabación de pantalla, Bluetooth (CAT + audio).
- 🔜 **Pendiente con motor ya disponible en la referencia**: WSPR (`src/wspr` + `third-party/wsprd`),
  SSTV (`src/sstv`), WeFax (`src/wefax`), NavTex (`src/navtex`), JS8 (`src/js8` +
  `third-party/js8core`, 380 archivos), cluster (`src/cluster`), Field Ops (`src/field`),
  temas (`src/theme.c`), wfview (`src/wfview_server.c`), web ampliado (`webserver/`),
  DRM/KMS (`lv_drivers` + `screen_rec` del fork), teclados (`kbd_rollover.c`), CW trainer y
  CW keyboard.

## Regla de uso

1. **Antes de escribir un módulo nuevo, buscar si ya existe acá** (`src/`, `third-party/`,
   `webserver/`, `themes/`, `rootfs/`): casi todo lo que falta tiene implementación probada.
2. **Antes de dar una feature por buena, buscar su test o su loopback** en `tests/` y `tools/`:
   es la forma de verificarla sin radio.
3. Mantener la atribución y la licencia (LGPL-2.1-or-later) en cada archivo portado.
