# Evidencia de build — #116: ELF exacto de la radio + verificadores ARM

## 1. El ELF que corre la radio, reconstruido (md5 verificado)

La radio corre el build del trunk **`23926241`** ("Ajustes: el campo de año con la
misma caja que Day/Month (issue #110) (#112)"). Se reconstruyó en un **directorio
propio** (volumen docker privado `cw116-build-output`, no el compartido
`x6200-build-output`) desde un checkout propio del commit
(`git archive 23926241` + el submódulo `lvgl` pineado `74d0a816`), con el toolchain
`x6200-buildroot`:

```sh
docker run --rm --entrypoint "" \
    -v .../AetherX6200Buildroot:/workspace \
    -v cw116-build-output:/workspace/build \
    -v <checkout 23926241>:/workspace_gui:ro \
    -e FORCE_UNSAFE_CONFIGURE=1 -w /workspace/build x6200-buildroot \
    sh -c 'make x6200-gui-dirclean; make x6200-gui-rebuild -j$(nproc)'
```

```
-rwxr-xr-x  13 742 964 B  /workspace/build/target/usr/sbin/x6200_gui
3604cece0e4382229a2de328d9baa614  x6200_gui        <- == md5 que corre la radio
```

El md5 **coincide** con el del binario de la radio (`md5sum /usr/sbin/x6200_gui` →
`3604cece0e4382229a2de328d9baa614`), así que la pila se resolvió contra el ELF
correcto (ver [`../2026-10-16-cw116-radio/`](../2026-10-16-cw116-radio/)). El
binario no está stripped (16 629 símbolos), con `main` en `0x1ea74`.

Salida cruda: [`rebuild.txt`](rebuild.txt).

## 2. El fix compila para ARM (y el firmware entero se reconstruye)

Verificador mínimo (patrón de los flujos de hoy), `src/cw_encoder.c` con el fix,
toolchain de buildroot, `-Wall -Wextra`:

```
$ docker run --rm --entrypoint "" -v cw116-build-output:/b:ro -v <stage>:/s -w /s x6200-buildroot \
    sh -c '/b/host/bin/arm-buildroot-linux-gnueabihf-gcc -std=gnu11 -O2 -Wall -Wextra \
           -Wno-unused-parameter -pthread -I/s -c /s/cw_encoder.c -o /s/cw_encoder.o'
cw_encoder.c: compila para ARM ok
/s/cw_encoder.o: ELF 32-bit LSB relocatable, ARM, EABI5 version 1 (SYSV), not stripped
```

Y el firmware entero (el `x6200_gui` real, con el fix, en el volumen privado):

```
-rwxr-xr-x  13 743 188 B  /workspace/build/target/usr/sbin/x6200_gui
6d23c0906c808f8f9b046ce359b31a0e  x6200_gui
```

El único warning del build es de `lvgl/widgets/lv_waterfall.c` (`discards 'const'
qualifier`), del submódulo LVGL pineado, pre-existente y ajeno al cambio.

## 3. Banco QEMU: no alcanzable en aislamiento (con control)

El banco **no se pudo correr** en aislamiento, y se midió por qué:

1. El camino del crash (la ventana de **mensajes de CW**) **no es alcanzable** por el
   control remoto: se abre con la tecla `MSG` (`src/main_screen.c:783`, `KEYPAD_MSG`),
   y el socket no tiene comando de teclado — sólo `app` (22 apps del menú, y la ventana
   de mensajes de CW no está ahí), `btn` (botón de la página actual) y `page`.
2. El chroot compartido `.vfix3` y el volumen `x6200-build-output` están **fuera de
   alcance** por la regla de aislamiento del issue. Con un chroot propio (copia de
   `target/` del build privado) la GUI **no arranca**: crashea temprano, antes del
   socket, en `subject_get_int` (inicialización de params/subjects; el banco sin
   `.vfix3` no tiene el `params.db` preparado ni la tarjeta de audio —
   `amixer: Mixer attach default error: No such file or directory`).

**Control (para no atribuirle el crash al fix)**: el binario **sin** el fix
(`3604cece`) crashea **igual** en el mismo chroot:

```
sin fix (3604cece):  CRASHLOG sig=11 fallo=0x10 pc=0x419b8400 lr=0x4fa63c base=0x41ea74
con fix (6d23c090):  CRASHLOG sig=11 fallo=0x10 pc=0x419b8400 lr=0x4fa9ac base=0x41ea74
```

Mismo `pc`/`fallo`/`base` (el `lr` difiere en `0x370` porque el fix corrió las
direcciones), o sea: es del entorno del chroot propio, no del cambio.

**PENDIENTE honesto**: el crash real bajo radio (la secuencia exacta de teclas del
owner sobre la ventana de mensajes de CW) es `requires-radio`. Lo que queda fundado
sin radio es el mecanismo (desensamblado + test nativo).
