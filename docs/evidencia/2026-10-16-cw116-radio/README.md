# Evidencia de radio — #116: SIGSEGV (fallo=NULL) en el código de CW

El crash medido en la radio, resuelto con los **artefactos exactos** que corre el
equipo (no un build parecido). Cierra el punto del issue que el QA dejó abierto:
*"el binario del volumen no coincide con el de la radio, así que no resolví la
pila"*. La radio **sí** es reproducible: el binario que corre es el build del
trunk `23926241`, reconstruido acá con `md5 3604cece0e43`
(ver [`../2026-10-16-cw116-banco/`](../2026-10-16-cw116-banco/)).

## Qué corre la radio (md5 leídos del equipo)

```
$ md5sum /usr/sbin/x6200_gui /lib/libc.so.6 /usr/lib/liblvgl.so
3604cece0e4382229a2de328d9baa614  /usr/sbin/x6200_gui     (13 742 964 B, no stripped)
412df301b11eeecfaaa77847fe63372b  /lib/libc.so.6          (1 573 500 B)
cd30ca6c58ad42f06caad930b65f0ee2  /usr/lib/liblvgl.so     (690 444 B)
```

Los tres coinciden con los artefactos usados para resolver (el `x6200_gui` sale
del rebuild aislado de `23926241`; la `libc` y la `liblvgl` son las del rootfs de
esa ROM).

## El CRASHLOG (crudo)

Del log `/mnt/app_logs/x6200_log_2026-10-16_22-01-44.txt` (1445 líneas, la última
es la pila): [`crashlog-116.txt`](crashlog-116.txt).

```
CRASHLOG sig=11 fallo=(nil) pc=0xb5de0064 lr=0xb5ddf714 sp=0xbeb89ae0 base=0x44ea74
CRASHLOG-STACK 00000001 00000000 00000001 0124fbcc beb89afc 0047dacc b5ece000 b5dd58ac ...
```

La línea inmediatamente anterior es el último `CWDBG show o=0x1cd4db0 on=1`
(el `refresh_ui()` del **CW Trainer**, que estaba abierto y refrescándose con el
timer). Es contexto, no la causa.

## La pila resuelta

[`resolucion.txt`](resolucion.txt) (reproducible con
[`resuelve-pila.py`](resuelve-pila.py) sobre los artefactos):

```
main link=0x1ea74  base(runtime)=0x44ea74  base de carga del ELF=0x430000
libc base (fijada por pc/lr) = 0xb5d40000

pc=0xb5de0064 -> libc+0xa0064 : strlen@@GLIBC_2.4+0x23
lr=0xb5ddf714 -> libc+0x9f714 : __strdup@@GLIBC_2.4+0xc
0x0047dacc -> enlace 0x4dacc : cw_encoder_send+0x68
0x004773cc -> enlace 0x473cc : dialog_msg_cw_beacon_cb+0x50
```

La base de libc se fija con el propio par pc/lr: `lr = pc - 0x950` y la única base
de libc `0x10000`-alineada en que el pc cae en `strlen` y el lr en `__strdup` es
`0xb5d40000`. Es un par llamador/llamado (`__strdup` llama a `strlen`), no una
coincidencia: los otros 19 offsets posibles dan funciones sin relación.

## El desensamblado que lo cierra

[`desensamblado.txt`](desensamblado.txt):

```
__strdup (libc 0x9f708):
   9f70c: mov  r5, r0                              ; r5 = s (el argumento)
   9f710: blx  a0040 <strlen>                       ; strlen(s)
   9f714: add  r4, r0, #1                           ; <- lr = 0x9f714

strlen (libc, Thumb 0xa0040):
   a0060: mvn.w r0, #7                              ; r1 = r0 & ~7
   a0064: ldrd  r2, r3, [r1]                         ; <- pc = 0xa0064: lee [r1]

cw_encoder_send (x6200_gui 0x4da64):
   4dac8: bl   1cefc <strdup@plt>                    ; strdup(text)
   4dacc: add  r5, pc, r5                            ; <- sp[5] = 0x4dacc

dialog_msg_cw_beacon_cb (x6200_gui 0x4737c):
   473c4: mov  r1, #1                                ; beacon = true
   473c8: bl   4da64 <cw_encoder_send>               ; cw_encoder_send(msg, true)
   473cc: bl   4f440 <buttons_unload_page>           ; <- sp[21] = 0x473cc
```

Cadena, de arriba hacia abajo:

`dialog_msg_cw_beacon_cb` → `cw_encoder_send(msg, true)` con **`msg == NULL`** →
`strdup(NULL)` → `strlen(NULL)`. En `strlen`, `r0 = NULL`, `r1 = r0 & ~7 = 0`, y el
`ldrd [r1]` lee la dirección **0** → `SIGSEGV` con `si_addr = 0` (`fallo=(nil)`), pc
justo en ese `ldrd`. La pila encaja con los marcos: `strdup` empuja 4 registros
(`push {r4,r5,r6,lr}`) y `strlen` empuja 8 bytes (`strd r4,r5,[sp,#-8]!`), así que el
lr guardado por `strdup` (el retorno a `cw_encoder_send`) cae en `sp[5]` — y ahí está
`0x0047dacc`.

## Por qué `msg` era NULL

`get_msg()` (`src/dialog_msg_cw.c:256`) devuelve NULL cuando la tabla de mensajes de
CW está **vacía** (`table_rows == 0`):

```c
static const char* get_msg() {
    if (table_rows == 0) {
        return NULL;
    }
    ...
}
```

`table_rows` sólo crece en `dialog_msg_cw_append()`, que llama `params_msg_cw_load()`
al leer la tabla `msg_cw` del `params.db`. Esa tabla **nace vacía** en el esquema de
la ROM (`sql/params.sql` la crea sin `INSERT`), así que una radio sin mensajes de CW
guardados tiene `table_rows == 0` y `get_msg()` devuelve NULL. El callback de
**Beacon** (`dialog_msg_cw_beacon_cb`) le pasaba ese NULL al keyer sin mirarlo.

(El otro chequeo de `get_msg()`, `row == LV_TABLE_CELL_NONE`, está muerto:
`LV_TABLE_CELL_NONE` es `0xFFFF` y `row` es `int16_t`, así que la comparación nunca
es cierta y el compilador la borra; con fila inválida `lv_table_get_cell_value()`
devuelve `""`, no NULL. Por eso el único camino NULL es `table_rows == 0`, coherente
con el desensamblado, que sólo tiene el chequeo de `table_rows`.)

## El contexto del owner

El log muestra el **CW Trainer** abierto (las líneas `CWDBG show` son su
`refresh_ui()` por el timer) mientras el owner movía el VFO. El crash es **otro**
diálogo: la ventana de mensajes de CW, que se abre con la tecla `MSG` en modo CW
(`src/main_screen.c:783`, `KEYPAD_MSG`), al tocar **Beacon** con la tabla vacía. Los
`CWDBG` son ruido de diagnóstico (escriben crudo, sin pasar por el logger), no la
causa: la correlación que anotó el issue es real pero no causal.

## Alcance

Sólo resolución de pila + fundación del NULL. El crash real bajo radio
(el orden exacto de teclas del owner) es `requires-radio`; el mecanismo queda fundado
por el desensamblado y por el test nativo (`tests/native/test_cw_encoder.c`).
