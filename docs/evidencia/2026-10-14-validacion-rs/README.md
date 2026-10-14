# Validacion en la radio del fix de contraste del medidor (RS) — 2026-10-14

Se publico el commit `26dc5ec` ("temas: WCAG compliance 9/9 medido (texto 81 pares + no-texto 1.4.11
por pixeles) — fix RS hallado y aplicado"), que cambia **solo** en el JSON del tema:

```
rootfs/usr/share/x6200/themes/RS/theme.json:  "meter_over": "#ff4040"  ->  "#ff5252"
```

## Lo medido en la radio (tema RS)

`01-rs.png` es la captura del equipo con el tema RS y el `theme.json` **ya sincronizado** (md5
`b045d46481fc`, igual al del repo) y la GUI reiniciada. Muestreando los pixeles del arco del medidor
(franja y=15..60, x=380..560):

```
pixeles rojos: 343 del color (255, 64, 64) = #ff4040   <- el color VIEJO
contraste contra el fondo medido (62, 78, 92): 2.42:1  <- por debajo de 3:1 (WCAG 1.4.11)
```

O sea: en la radio el color **no** cambio (sigue `#ff4040`) y el contraste real es **2.42:1**.

## Por que no cambia (codigo)

- RS es un **tema de ROM**: el comentario de `src/theme.c:31` dice *"Los temas que vienen con la ROM
  (Holo, Material 3 y RS, con sus imagenes)"* y el valor sale de la tabla compilada
  (`src/theme.h:129`, macro `THEME_COLORS`): `X(meter_over, 0xffffff, 0xffffff, 0x000000, 0xff4040)`.
- Los `theme.json` se leen de **`/mnt/themes/`** para temas **custom** (`src/theme.c:4`: *"built-in
  tables and JSON files in /mnt/themes/"*), no de `/usr/share/x6200/themes/` para los de ROM.

Conclusion: para que el fix llegue al equipo hay que tocar la tabla compilada (o mover RS a JSON y
servirlo desde donde el firmware lo lee).

## Y el valor nuevo tampoco alcanza 3:1 con el fondo real

Contrastes calculados con la formula de WCAG:

| Color | vs fondo real medido (62,78,92) | vs `colors.panel` del JSON (#000000) |
|---|---|---|
| `#ff4040` (viejo) | **2,47:1** NO pasa | 5,3:1 pasa |
| `#ff5252` (nuevo) | **2,69:1** NO pasa | 5,8:1 pasa |

El chequeo del arnes da 9/9 porque mide contra el `panel` declarado en el JSON (`#000000`), pero en
la pantalla el medidor se dibuja sobre otro fondo (barra/imagen). En el equipo, con el valor nuevo, el
contraste seguiria por debajo de 3:1.

## Entorno

Rama `lu2dsx/x6200-rom` (commit `7f2c94ecb2c7` en el equipo), `x6200_gui` md5 `7f2c94ecb2c7`, kernel
`Linux X6200 6.1.82 #2 SMP PREEMPT`, sin transmision.
