# #91 — "tema guardado `Outdoor`, que ya no existe": hallazgo fundado

**Corrida**: `factory/theme-fallback-91` · **Fecha**: 2026-10-15 · **Base**: trunk `ad2bd88`
**Issue**: #91 (la radio queda lavada y come ~1,2 núcleos tras el reflash con el fix del #89).

## Resumen (una línea)

**`Outdoor` SÍ existe** — es un tema **incorporado** (`src/theme.c`, `B_OUTDOOR`), se ofrece en
Ajustes (posición 1 de 13) y **es el que la radio está dibujando**: lo "lavado" es su paleta clara
por diseño. No hay tema roto, no hay migración y el fallback del engine ya es completo.

## 1. `Outdoor` existe: es un builtin, no un archivo

`src/theme.c`:

```c
enum { B_IU1WXY, B_LEGACY, B_OUTDOOR, B_CLASSIC, B_OUTDOOR_NEG, B_VISION, B_COUNT };
static const char *const builtin_names[B_COUNT] = {"IU1WXY", "Legacy", "Outdoor", "Classic",
                                                   "Outdoor-Negative", "VISION"};
```

`theme_list()` (la lista de *Ajustes → Display → INTERFACE → Theme*) recorre los builtins menos
`Legacy` y después suma los JSON de `/mnt/themes` + `/usr/share/x6200/themes`. Medido con el
resolver real (`theme_list()`, `THEMES_DIR_SHIPPED=rootfs/usr/share/x6200/themes`):

```
theme_list n=13
  [0] IU1WXY   [1] Outdoor   [2] Classic   [3] Outdoor-Negative   [4] VISION
  [5] Aurora   [6] Holo      [7] Material 3 [8] RS   [9] Redshift  [10] Sprint  [11] Sunlit  [12] Trail
Outdoor offered: YES
```

`theme_init("Outdoor")` entra por `builtin_index()` (match `strcasecmp`, `theme.c:125`) → `b >= 0` →
`use_builtin(B_OUTDOOR)`: **carga el tema completo**, sin pasar por el fallback. Por eso el `grep`
del log del QA por `theme|image|not found` da 0: nunca se disparó el `"Theme %s not found"`.

### Por qué la radio (y el QA) lo dan por "inexistente"

`Outdoor` es **compilado** (tabla en `src/theme.h`, columna `b`), **no es un JSON**. La lista que
reportó el QA —`Aurora, Holo, Material 3, Redshift, RS, Sprint, Sunlit, Trail` (8)— es exactamente
el contenido de **`/mnt/themes`** (los archivos), donde `Outdoor` no está porque no es un archivo.
De ahí la conclusión "ya no existe". En el desplegable de Ajustes sí está (posición 1).

## 2. La radio dibuja `Outdoor`, no `Sunlit` (medido en la captura)

Colores de la captura `docs/evidencia/2026-10-15-validacion-arreglos/13-tras-fix89.png` (800x480),
contados por píxel:

| zona / rol | píxeles | color medido | quién lo declara |
|---|---:|---|---|
| barra superior (`bar`) | 7705 | `#F2F2F2` | **`Outdoor`** (`theme.h` col. `b`, `bar`) |
| waterfall parada 0 | 46725 | `#FAFAFA` | **`Outdoor`** (`theme_shape.waterfall[0]`) |
| fondo/panel | 125486 | `#FFFFFF` | **`Outdoor`** (`bg`/`panel` col. `b`) |
| `Sunlit.json` `bg` | **0** | `#F7F5EB` | `Sunlit.json` — **no aparece** |
| `Sunlit.json` `bar` | **0** | `#E7E4D8` | `Sunlit.json` — **no aparece** |

La barra `#F2F2F2` es **`Outdoor`** (su `bar` compilado); `Sunlit.json` declara `bar` `#E7E4D8`, que
**no aparece** en la captura. El comentario del issue (y el commit `ad2bd88`) fundaron "el tema
activo es `Sunlit`" matcheando el pie `#FFFFFF` contra el `panel` de `Sunlit.json` — pero `Outdoor`
también tiene `panel` `#FFFFFF`, y la barra es el discriminante: `#F2F2F2` = `Outdoor`, no `Sunlit`.
**Lo lavado es `Outdoor` por diseño** (es el tema "claro, para el sol": `theme.c:95`,
`docs/THEMES.md`).

## 3. No hay migración de temas

`src/params/migrations.c` sólo tiene 3 migraciones (`_0` init, `_1` tabla `digital_modes`, `_2` freq
de ATU). No hay ninguna que reescriba `theme_name`. `grep -rn "Sunlit\|migra" src/` no devuelve
ninguna migración de temas. El mapeo `Outdoor -> Sunlit` que supone el issue **no existe en el
código**.

## 4. El fallback del engine ya es determinista y completo

`theme_init()` (`src/theme.c:479`): si el nombre no es un builtin y no hay archivo legible, cae a
`use_builtin(B_IU1WXY)` — **un tema completo**, no un estado mitad-y-mitad: `use_builtin()` copia
los **122 roles** (`memcpy(colors, builtin_colors[src], sizeof(colors))`) y el `theme_shape` entero,
y `theme_init` limpia `images[]`/caché antes. Verificado con el resolver real:

```
TC_COUNT=122 mismatched roles=0 name='IU1WXY'   (nombre inexistente)
second pass mismatched=0                         (idempotente)
```

Cubierto por tests nuevos en `tests/native/test_theme.c`
(`test_unknown_theme_is_complete_default`, `test_broken_theme_file_falls_back`): tema que no existe
y archivo de tema que existe pero no parsea → los 122 roles del default, nombre `IU1WXY`, y pedirlo
dos veces da lo mismo. **No hizo falta cambiar `src/`**: la garantía ya estaba; el test la fija.

## 4b. Banco: el repro, con captura

`tools/captura-banco.sh` (chroot ARM + `qemu-arm-static`, `RECONSTRUIR=no`), inyectando
`theme_name` en `mnt/params.db` (tabla `params`). Evidencia en
`docs/evidencia/2026-10-15-fallback91/`:

| `theme_name` | captura | resultado |
|---|---|---|
| ausente (default `IU1WXY`) | `banco-default-iu1wxy.png` | `bg #27313A` (oscuro) |
| `Outdoor` | `banco-tema-outdoor.png` | `bg #FFFFFF` + `bar #F2F2F2` = **Outdoor (claro)**, reproduce la captura de la radio |
| `no-such-theme-91` | `banco-tema-invalido.png` | log `Theme no-such-theme-91 not found, using IU1WXY`; PNG **byte-idéntico** al default |

Es decir: el banco reproduce el "lavado" con `Outdoor` y confirma que un params inválido deja el
default **completo** (idéntico al arranque normal), sin mitad-y-mitad ni redibujo raro.

## 5. Lo que queda abierto (no es el tema)

El **~1,2 núcleos** y el redibujo permanente: el framebuffer se dibuja con un buffer de pantalla
completa (`src/main.c:55`, `DISP_BUF_SIZE (800*480*4)`, un solo buffer) + `sw_rotate`/`ROT_90`
(`main.c:233`), y el S-meter/spectrum/waterfall se invalidan solos. **No es del tema**: el mismo
camino corre para cualquier tema.

Se intentó medir en el banco como proxy relativo (`docker stats` del contenedor, 3 muestras de 3 s):
`Outdoor` ~100,6 % y `IU1WXY` ~100,7 % — **QEMU satura el core del host en los dos**, así que no
separa (el A33 real no se modela). **Declarado**: el ~1,2 núcleos queda para la radio (ajuste del
owner: cambiar de tema y volver a medir para separar causa), como ya pide el issue.
