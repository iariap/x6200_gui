# Evidencia: banco ARM/QEMU del conjunto THEMES v2 (issue #73) — 2026-10-15

La pasada del **binario ARM real** (el `x6200_gui` de la imagen, bajo `qemu-arm-static`
en un chroot del rootfs con framebuffer falso) con **los seis temas v2**, un solo
binario. Cierra el PENDIENTE "captura ARM por tema" heredado de #67/#68/#69/#70/#71/#72:
el banco se usó **una sola vez** (un `dirclean` + rebuild) y las seis capturas salen del
mismo binario.

**Esto es el binario real, no el arnés.** El arnés (host x86) está en
`../galeria/`; esto corre el ejecutable ARM que va en la radio. Igual no dice nada del
brillo al sol ni del consumo: QEMU no mide energía.

## Qué se probó

| Dato | Valor |
|---|---|
| Binario | `x6200_gui` (ARM), **md5 `eb0794cffd8a`** — el mismo en las seis capturas |
| Fuente (`src/` + `lvgl/src`) | md5 `4ce411b06cf3` (build) / `e5d0f7052b79` (tras el commit de comentarios) |
| qemu-cpu | `cortex-a7` (el core de la R16; con el modelo por defecto la GUI muere por SIGBUS) |
| Sesión | `dirclean` + rebuild **una vez** (`RECONSTRUIR=si`); las otras cinco capturas reusan el binario (`RECONSTRUIR=no`) |

El md5 del binario es **idéntico** en las seis capturas: el único cambio de `src/` entre
el build y el tip (`62f5b33`) son **comentarios** (`src/dialog_settings.c`, `src/theme.h`),
que no entran al código objeto — por eso el guard del banco avisa que el hash de fuentes
cambió pero el binario es el mismo.

## Cómo se cambiaron los temas (gap declarado)

**El socket de control no permite cambiar el tema en runtime.** `src/remote_server.cpp`
acepta `get`/`sub`/`freq`/`mode`/`app`/`page`/... pero **no** un comando de tema: el tema se
lee de `params.theme_name` **al arrancar** (`theme_init()`, `src/main.c`) y cambiarlo reinicia
la GUI (`dialog_settings.c`, `main_screen.c` → `params_flush()` + `exit(0)`). Así que el
cambio de tema por socket es un **gap** de esta revisión (un comando `theme` es trabajo
aparte, fuera del alcance del #73).

Las seis capturas se hicieron por **`params.db` inyectado** (la vía que el #73 previó para
este caso), sin rebuild: se escribe `theme_name` en `/mnt/params.db` del chroot, se reinicia
la GUI y se captura. Los JSON de los seis temas se dejaron en `/mnt/themes/` del chroot (el
directorio DATA gana sobre la ROM, la misma prioridad que en la radio).

```bash
CH=/home/pablo/desa/x6200/.vfix3                       # chroot del banco
BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot

# 1) UNA sesión de build + la primera captura (Sunlit):
CHROOT=$CH BUILDROOT=$BUILDROOT RECONSTRUIR=si tools/captura-banco.sh Sunlit.png

# 2) las otras cinco, reusando el binario (params.db + reinicio de la GUI):
for t in Redshift Sprint Cathode Trail Aurora; do
    sqlite3 "$CH/mnt/params.db" "INSERT OR REPLACE INTO params(name,val) VALUES('theme_name','$t');"
    docker rm -f banco-apps
    CHROOT=$CH BUILDROOT=$BUILDROOT RECONSTRUIR=no tools/captura-banco.sh "$t.png"
done
```

## Las capturas

| Tema | PNG | sha256 (16) | Bytes |
|---|---|---|---|
| Sunlit | `Sunlit.png` | `92362854edb259a6` | 26124 |
| Redshift | `Redshift.png` | `f7570235ac935c58` | 22210 |
| Sprint | `Sprint.png` | `9753ae68261530ee` | 23727 |
| Cathode | `Cathode.png` | `81f3ec7a6be5d4dd` | 25966 |
| Trail | `Trail.png` | `7a932f8b102a4380` | 27117 |
| Aurora | `Aurora.png` | `b7f5620d80944a65` | 25321 |

`banco-seis-temas.png` es la hoja con las seis, para verlas juntas.

Cada captura es la pantalla principal (RX) del binario real con ese tema: la identidad de
color de cada uno se ve en la barra superior, la fila de frecuencia, el arco del S-meter y
los botones. El "8 fantasma" de la fila de frecuencia es el defecto conocido UI-017 (la
sombra de la etiqueta, `src/main_screen.c`), igual en todos los temas.

## Lo que NO se midió (honesto)

- **Brillo al sol / adaptación nocturna**: son metas de diseño de Sunlit/Redshift, no
  garantías medidas. QEMU no tiene pantalla.
- **Energía**: el banco no mide consumo; los temas no se asocian a ahorro del LCD.
- **El selector six-only en la pantalla real**: el socket no navega Ajustes hasta el grupo
  Apariencia, así que la oferta de seis se valida con el test nativo
  (`tests/native/test_theme.c`) y el render del arnés (`settings-perfil`), no con una captura
  del banco.
