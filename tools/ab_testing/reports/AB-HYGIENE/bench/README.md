# Banco (sin radio): evidencia de `{"cmd":"app","text":"off"}` sobre RTTY

Banco del repo (`tools/captura-banco.sh`): el `x6200_gui` ARM de verdad, con
`qemu-arm-static` en un chroot del rootfs y framebuffer falso, comandado por el
socket de control. Sin radio física: es un banco de GUI.

Binarios medidos (md5 del `x6200_gui`, impreso por el script):

| medición | binario | fuente |
|---|---|---|
| **antes** | `a6865609cf13` | `main_screen.c` de `baf6164` (resto del árbol = HEAD) |
| **después** | `67b41837e0f4` | HEAD `c589411` (el fix) |

Secuencia (la misma en las dos): abrir RTTY por el socket, pedir `{"cmd":"get"}`,
mandar `{"cmd":"app","text":"off"}`, volver a pedir `{"cmd":"get"}`.

## antes (bug)

```
$ {"cmd":"app","text":"RTTY"}      -> {"t":"ok"}
$ {"cmd":"get"}                    -> app="RTTY"
$ {"cmd":"app","text":"off"}       -> {"t":"ok"}
$ {"cmd":"get"}                    -> app="RTTY"    <-- "off" NO cerro RTTY
```

Capturas: `base-rtty-abierta.png` (barra `(RTTY 1:1)`) y `base-tras-off.png`
(la barra **sigue** en `(RTTY 1:1)`, la página de la app pegada).

## después (fix)

```
$ {"cmd":"app","text":"RTTY"}      -> {"t":"ok"}
$ {"cmd":"get"}                    -> app="RTTY"
$ {"cmd":"app","text":"off"}       -> {"t":"ok"}
$ {"cmd":"get"}                    -> app=""        <-- RTTY cerrada de verdad
```

Capturas: `head-rtty-abierta.png` (barra `(RTTY 1:1)`) y `head-tras-off.png`
(la barra vuelve a la página previa a la apertura: `(VOL 1:3)`, el home).

## cierre desde un diálogo (decisión 6)

```
después:
$ {"cmd":"app","text":"RTTY"}      -> {"t":"ok"}
$ {"cmd":"app","text":"Settings"}  -> {"t":"ok"}    (un diálogo encima de RTTY)
$ {"cmd":"get"}                    -> app="Settings"
$ {"cmd":"app","text":"off"}       -> {"t":"ok"}
$ {"cmd":"get"}                    -> app=""
```

La prueba **por píxeles** de que la barra queda en el estado pre-apertura y no en
la página de RTTY es el fixture `app-close-home` del arnés
(`tools/ui_harness/fixtures/`): su PNG final es byte-idéntico (mismo sha256) a
`home-usb-20m`. El banco no puede afirmar eso: su espectro es vivo, así que dos
capturas de la misma pantalla nunca son byte-iguales.

Nota de robustez del banco: el `x6200_gui` bajo qemu muere solo después de un
rato; dos capturas del banco en invocaciones separadas obligan al script a
reiniciar la GUI (lo imprime: "el banco no contesta o la GUI no esta viva"). Por
eso las mediciones de estado de arriba se tomaron en una sola vida de la GUI.
