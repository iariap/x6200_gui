# Arnés de UI: ver la interfaz sin la radio

Verificación del 2026-10-11 a partir de
[`IU1WXY/X6100-IU1WXY/tools/apps_ui_harness`](https://github.com/IU1WXY/X6100-IU1WXY/tree/main/tools/apps_ui_harness).

## ¿Se puede usar? Sí, y es la herramienta correcta

El arnés del X6100 **renderiza los diálogos reales de la GUI en el PC, sin radio**:

- Compila las **fuentes reales** de la GUI con **LVGL 8.3.11 stock** (el fork NEON del firmware no
  compila sin NEON) y escribe **PNG**.
- Los símbolos del firmware que no hacen falta se **auto-stubean**: `gen_stubs.py` lee las
  referencias indefinidas de un link fallido y genera `build/stubs_auto.S` (funciones que
  devuelven 0, datos en cero). Los valores que importan para el layout están a mano en `stubs.c`.
- `cfg_host.cpp` arma un `cfg_sm` real con valores por defecto (sin base de datos) para que
  Ajustes se pueda renderizar; Wireless muestra redes y dispositivos falsos.
- Bloquea la red con `-Wl,--wrap=connect`, así que ningún cliente sale a internet.

**Probado acá**: el método funciona en este entorno. `tools/ui_harness/probe.c` compila LVGL
headless y escribe `probe.png` (800x480):

```bash
docker build -t x6200-uiharness tools/ui_harness          # imagen con libpng/fftw/sndfile/glib/libnm/sqlite
docker run --rm -v "$PWD:/src" -w /src/x6200_gui/tools/ui_harness x6200-uiharness \
  bash -c 'mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./ui_probe'
```

![prueba de render headless](ui_harness_probe.png)

## Por qué no sirve tal cual para nuestra ROM

El arnés está atado al árbol del X6100:

| Pieza | X6100 | Nosotros |
|---|---|---|
| Lista de fuentes | JS8, SSTV, WeFax, NavTex, WSPR, Cluster, Field, `theme.c`, `cJSON.c`, `freq_strip/scale`, `spot_marks`, `cluster_column`, `dialog_modes`, `dialog_cw_trainer`… | no existen; tenemos `dialog_about`, `band_plan*`, `adaptive_tuning`, `audio_enhancement`, `update_check`, `screen_rec`, `ft8_udp` |
| `cfg_host.cpp` | arma su `cfg_sm` | necesita **nuestro** `cfg_init(sqlite3*)` |
| Temas | `theme.c` + `themes/*.json` | no tenemos sistema de temas (paleta fija) |
| Submódulos | `third-party/js8core`, `wsprd` | no los usamos (el build del arnés del X6100 falla justo ahí si se clona sin `--recurse-submodules`) |

## Plan de adaptación (para nuestra ROM)

1. **Lista de fuentes propia**: `dialog.c`, `dialog_settings.c`, `dialog_about.c`, `dialog_ft8.c`,
   `buttons.cpp`, `styles.c`, `meter.c`, `band_plan.c` + `band_plan_data.c`, `clock.c`, `info.c`,
   `tx_info.c`, `widgets/lv_waterfall.c` + `lv_finder.c`, `cfg/*`, `qth`, `scheduler.cpp`,
   `fonts/*` y las nuevas que queramos ver.
2. **Shim de cfg**: crear una base temporal desde `sql/` (mismo esquema que la imagen) con
   sqlite3 y llamar a `cfg_init(db)` + `cfg_mode_params_init(db)` + `params_init()`; así Ajustes
   y la pantalla principal usan los caminos reales de la GUI.
3. **Stubs**: copiar `gen_stubs.py` (LGPL) e iterar; en `stubs.c` dejar los valores que definen
   el layout (dial, bordes de filtro, espectro/waterfall falsos, estado de radio).
4. **Deps propias**: `fetch_deps.sh` que clone LVGL v8.3.11 y ft8_lib (hoy uso las del arnés del
   X6100, que viven en `.x6100_ref`). fftw3f, liquid y libpng para el espectro.
5. **Runner**: la imagen `x6200-uiharness` (Dockerfile en `tools/ui_harness/`) ya queda lista y
   aislada del toolchain cruzado.

## Qué podríamos verificar con esto

- La **aguja del S-meter** (geometría del dial, marcas y etiquetas) en sus dos estados.
- La **franja del band plan** sobre el espectro, alineada con la fila de frecuencia y con los
  segmentos recortados en los bordes.
- Las **filas nuevas de Ajustes** (Audio enhance, Waterfall battery mode, S-meter needle,
  Band plan) y el diálogo **About**.
- El **indicador de grabación** de pantalla.
- Y, a medida que se porten: los diálogos de WSPR, SSTV, JS8, NavTex, Field Ops y el cluster.

Es exactamente lo que faltaba para no trabajar a ciegas en lo visual: hoy las features de UI se
verifican solo por compilación y strings del binario.

## Estado

- ✅ Método verificado (render headless + PNG).
- ✅ Imagen de build reproducible (`tools/ui_harness/Dockerfile`) y sonda de render
  (`tools/ui_harness/probe.c`).
- ⏳ Adaptación completa del arnés a nuestras fuentes (paso siguiente).
