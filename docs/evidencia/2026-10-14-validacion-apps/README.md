# Validacion en la radio de los commits del 14/10 (Bands, About, RS)

Publicado en la radio el commit `7f2c94ecb2c7` (rama `lu2dsx/x6200-rom`), con 3 commits nuevos:

- `0684861` *main: Bands abre como el resto de apps (fix del defecto reportado en la radio)*
- `26dc5ec` *temas: WCAG compliance 9/9 medido ... fix RS hallado y aplicado*
- `cefbfc1` *main, tests: About abre desde el menu + checker scopeado y contador real*

## Validado

| Captura | Que se verifico | Resultado |
|---|---|---|
| `Bands.png` | la app **Bands** abre como las demas (titulo, lista de bandas con barras de propagacion, pie `(BANDS 1:1)` / `Test beacon` / `Use band`) | **anda** ✓ |
| `About.png` | **About** abre desde el menu y muestra `Version v0.1.2-lu2dsx` y los creditos | **anda** ✓ |

En las dos: el control remoto responde `{"t":"ok"}` y la GUI sigue viva (mismo PID).

## No validado (no llega al equipo)

El fix de contraste del tema **RS** del `26dc5ec` cambia **solo**
`rootfs/usr/share/x6200/themes/RS/theme.json`, pero RS es un **tema de ROM** (tabla compilada en
`src/theme.h:129`, `meter_over = 0xff4040`) y los `theme.json` son para temas custom
(`/mnt/themes/`). Medido en la radio con el tema RS: los pixeles del medidor siguen en `#ff4040` y el
contraste real es **2,42:1** (y 2,69:1 con el color nuevo), por debajo de 3:1.

Reportado como **issue #44** con la medicion, las lineas de codigo y la reproduccion.

## Entorno

Rama `lu2dsx/x6200-rom` (commit `f5c7c5d`), `x6200_gui` md5 `7f2c94ecb2c7` en el equipo, kernel
`Linux X6200 6.1.82 #2 SMP PREEMPT`, sin transmision. Tema de la prueba: `Classic` (el del usuario).
