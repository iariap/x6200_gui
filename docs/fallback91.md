# #91 — "tema guardado `Outdoor`, que ya no existe": hallazgo fundado

**Corrida**: `factory/theme-fallback-91` · **Fecha**: 2026-10-15 · **Rebase**: 2026-10-16
**Base**: trunk `7032d3bb` (ya incluye el #103 = cierre del epic THEMES v2 #66/#74)
**Issue**: #91 (la radio queda lavada y come ~1,2 núcleos tras el reflash con el fix del #89).

## Resumen (una línea)

En el trunk actual **`Outdoor` ya no es un tema**: el #74 retiró los legados, el default es
**`Aurora`** y un `theme_name=Outdoor` guardado **migra a `Aurora`** al arrancar
(`_3_migrate_theme_name`); aun sin migrar, `theme_init()` **cae al default completo**. Lo "lavado"
de la captura del QA era la paleta clara de `Outdoor` **por diseño**, y ese síntoma ya no puede
volver. Este PR fija el fallback con dos tests nativos y deja la evidencia.

> **Nota de rebase.** La investigación original (base `5e252dfb`) fundó que "`Outdoor` **sí**
> existía —era un built-in— y lo lavado era su paleta clara por diseño". Eso era correcto **antes**
> del #74: el #103 retiró los built-ins legados, puso el default en Aurora y reescribió
> `tests/native/test_theme.c`. Al rebasar sobre el trunk, la parte que importa sobrevive intacta
> —**el fallback es determinista y completo**— y es la que este PR fija con tests; los números de
> abajo están re-medidos contra el trunk rebasado.

## 1. El estado del trunk: los legados ya no son temas

`src/theme.c` ya no tiene `builtin_names[]`/`use_builtin()`: hay **cuatro paletas base internas**
(`IU1WXY`, `Legacy`, `Outdoor`, `Classic`) que **no se ofrecen ni se cargan por nombre** y sólo
sirven de plantilla al `base` de un archivo JSON (`base_index()`/`use_base()`). La colección son los
**seis archivos** de `rootfs/usr/share/x6200/themes/` (Sunlit, Redshift, Sprint, Cathode, Trail,
Aurora) y el **default es `Aurora`** (`THEME_DEFAULT`, `src/theme.h:211`).

- `theme_list()` ofrece exactamente esos seis (`src/theme.c`, `offered_names[]`).
- Un `params.theme_name` guardado que no sea uno de los seis **migra a Aurora** al arrancar
  (`src/params/migrations.c`, `_3_migrate_theme_name`); no borra ningún archivo del usuario.
- `theme_init()` sólo carga un archivo: nombre vacío, desconocido, retirado o archivo ilegible →
  `load_file(THEME_DEFAULT)`.

**Medido sobre el binario real (evidencia del trunk, #74)**: con `version=2` y
`theme_name='Outdoor'` en el `params.db`, el boot aplicó la migración y el `params.db` quedó en
`theme_name|Aurora`, con el render de Aurora. Ver
`docs/evidencia/2026-10-15-themes-removal-74/gates.txt` y `banco-migracion-outdoor-a-aurora.png`.

## 2. El fallback: determinista y completo (medido)

`theme_init()` (`src/theme.c:456`) limpia `images[]` y la caché de PNG, intenta `load_file(name)` y,
si falla, carga `THEME_DEFAULT`; sólo si **ese** archivo tampoco se puede leer usa la plantilla base
`IU1WXY` como último recurso, para no dejar la GUI sin tema. No hay estado mitad-y-mitad: el
`load_file()` que gana aplica la plantilla base **entera** (`memcpy(colors, base_colors[b],
sizeof(colors))`) y después los roles que el archivo declara.

Medición con el resolver real (host, sin qemu; instrumento y salida completa en
`docs/evidencia/2026-10-15-fallback91/resolver.txt`):

```text
TC_COUNT=122 default=Aurora
theme_name=no-such-theme-91 -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=IU1WXY           -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=Outdoor          -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=Classic          -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=Holo             -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=Material 3       -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=RS               -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=                 -> name='Aurora' roles_distintos_del_default=0 waterfall=si
theme_name=(NULL)           -> name='Aurora' roles_distintos_del_default=0 waterfall=si
idempotente: roles_distintos=0 name_a='Aurora' name_b='Aurora'
```

Es decir: los 122 roles, el nombre y la forma salen del default, y pedirlo dos veces da lo mismo.

## 3. Los tests que lo fijan

`tests/native/test_theme.c`:

- `test_unknown_theme_is_complete_default()` — un nombre irresoluble cae al default **completo**:
  los 122 roles, el nombre y la forma de `THEME_DEFAULT`; idempotente; y **sin mezclar** con el tema
  que hubiera cargado antes (carga Sunlit y después el inexistente: el estado queda idéntico al
  default, no una mitad de cada uno).
- `test_broken_theme_file_falls_back()` — un archivo de tema que **existe** pero no parsea (JSON
  roto, DATA a medio copiar) cae igual al default completo: la resolución no puede depender de que
  el archivo sea válido.

El test **muerde**: rompiendo el fallback a mano (dejar el tema anterior en pantalla en vez de
cargar `THEME_DEFAULT`) la suite pasa de `1512 comprobaciones, 0 fallos` a `1512 comprobaciones,
124 fallos` (los 122 roles + el nombre + el radio). La mutación no se commitea: **este PR no toca
código de producción** — el fix lo trajo el #74 y acá sólo se fija el invariante.

## 4. Evidencia del repro (banco, base pre-#74)

`tools/captura-banco.sh` (chroot ARM + `qemu-arm-static`, `RECONSTRUIR=no`), inyectando
`theme_name` en `mnt/params.db`. Las capturas se tomaron sobre la base `5e252dfb`, **antes** del #74
—cuando `Outdoor` todavía era un built-in y el default era `IU1WXY`— y se conservan como el registro
del repro:

| `theme_name` | captura | resultado |
|---|---|---|
| ausente (default `IU1WXY` de esa base) | `banco-default-iu1wxy.png` | `bg #27313A` (oscuro) |
| `Outdoor` | `banco-tema-outdoor.png` | `bg #FFFFFF` + `bar #F2F2F2` = **Outdoor (claro)**: reproduce el "lavado" de la radio |
| `no-such-theme-91` | `banco-tema-invalido.png` | log `Theme no-such-theme-91 not found, using IU1WXY`; PNG **byte-idéntico** al default |

En el trunk rebasado ese `theme_name=Outdoor` **ya no puede** dar la pantalla lavada: migra a Aurora
(§1) y, sin migración, el fallback cae al default completo (§2). No se re-corrió el banco en esta
pasada: el volumen del build (`x6200-build-output`) está compartido con otros contenedores del host
(`banco-apps`, …) y un `dirclean`+rebuild los afectaría; el banco del #74 (§1) ya documenta la
migración sobre el binario real, que es el mismo camino que acá se mide en el resolver.

## 5. Lo que queda abierto (no es el tema)

El **~1,2 núcleos** y el redibujo permanente: el framebuffer se dibuja con un buffer de pantalla
completa (`src/main.c:55`, `DISP_BUF_SIZE (800*480*4)`, un solo buffer) + `sw_rotate`/`ROT_90`
(`src/main.c:233`), y el S-meter/spectrum/waterfall se invalidan solos. **No es del tema**: el mismo
camino corre para cualquier tema, y el QA del trunk lo acotó al hilo principal y a user-space
(`docs/PRUEBAS.md`, 15/10 ~23:35).

Se intentó medir en el banco como proxy relativo (`docker stats` del contenedor, 3 muestras de 3 s):
`Outdoor` ~100,6 % y `IU1WXY` ~100,7 % — **QEMU satura el core del host en los dos**, así que no
separa (el A33 real no se modela). **Declarado**: el ~1,2 núcleos queda para la radio (ajuste del
owner: cambiar de tema y volver a medir para separar causa), como ya pide el issue.
