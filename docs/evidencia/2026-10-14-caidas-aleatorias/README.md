# Caidas aleatorias de la GUI en la radio (2026-10-14)

Evidencia del issue "[radio] Caidas aleatorias de la GUI (SIGABRT)".

## Que se observo

Con el barrido de las 23 apps (`tools/prueba-apps-radio.sh`, 14-15 s por app) la GUI **se cae**
entre 6 y 8 veces por corrida, y **las apps que caen cambian en cada corrida** (incluidas apps que
en la corrida anterior habian pasado). O sea: no es de una app, es endemico.

| Corrida | Caidas | Apps |
|---|---|---|
| `prueba-23-final.log` | 6 | FT4, WSPR, CwTrainer, Eq, Status, WiFi |
| `prueba-23-final2.log` | 8 | WSPR, Field, Settings, About, CwTrainer, QTH, Callsign, Recorder |

El equipo **no se reinicia**: el supervisor (`/usr/sbin/x6200_daemon`, un `while true`) relanza la
GUI en ~1 s y el uptime sigue subiendo. Lo que se pierde es la sesion de la app.

## Que dice el handler de crashes (incluido en el build)

Los ultimos 5, todos **SIGABRT (sig=6)**, con los **mismos bits bajos** de PC y LR (misma funcion
de libreria) y fallos **chicos y crecientes**:

```
CRASHLOG sig=6 fallo=0x6007 pc=0xb5d46dd4 lr=0xb5d46dc8 sp=0xb46fe838 base=0x4b9804
CRASHLOG sig=6 fallo=0x6047 pc=0xb5d26dd4 lr=0xb5d26dc8 sp=0xb46fe838 base=0x419804
CRASHLOG sig=6 fallo=0x6087 pc=0xb5dc6dd4 lr=0xb5dc6dc8 sp=0xb46fe838 base=0x469804
CRASHLOG sig=6 fallo=0x60cd pc=0xb5d36dd4 lr=0xb5d36dc8 sp=0xb46fe838 base=0x509804
CRASHLOG sig=6 fallo=0x6106 pc=0xb5dd6dd4 lr=0xb5dd6dc8 sp=0xb46fe838 base=0x499804
```

Y el volcado de pila del mismo crash esta **lleno de `000b000b` repetido**:

```
CRASHLOG-STACK 000b000b 000b000b 00000006 b46fe918 00001000 000000af 00000001 b5d7b618
                b46ff340 b5d6207c 00000020 00000000 000b000b 000b000b 000b000b ...
```

(archivo completo: `crashlogs.txt`)

## Como reproducir

```sh
cd x6200_gui
RADIO_IP=<ip-de-la-radio> PERMANENCIA=15 LOG=$PWD/barrido.log ./tools/prueba-apps-radio.sh
```

El script abre cada app, espera, y detecta la caida por PID de la GUI y por log nuevo (el uptime
solo no alcanza). Al final imprime `SE CAYO Y VOLVIO LA GUI` por cada caida.

## Entorno

- Rama: `lu2dsx/x6200-rom`, commit `b029907` (incluye los fixes de Status/JS8 y el handler de crashes).
- Radio: `x6200_gui` md5 `8ed2834a2d09` en la radio, `8ed2834a2d09` en la imagen; `liblvgl.so` md5
  `a1181ba6368d` en la radio, `a1181ba6368d` en la imagen.
- Kernel: `Linux X6200 6.1.82 #2 SMP PREEMPT`.
- Sin transmision: el barrido solo abre y cierra apps.

## Que ya se descarto

- **`LV_USE_ASSERT_OBJ=1`**: se probo y **empeora** (el barrido paso de 2 a 6 apps caidas): convierte
  en abortes casos que el equipo tolera. Es herramienta de diagnostico, no fix.
- **Memoria**: `oom_kill` en 0 y la GUI usa 20 MB.
- **Las librerias de instrumentacion del banco**: no hay `ld.so.preload` ni estan en la radio.
- Varios punteros colgados reales ya arreglados (spot_marks, band_plan, cw_tune_ui, pannel, los
  botones sin handler, `buttons_load_page` con `items[0]` NULL, `wifi_get_ipaddr` con punteros NULL,
  el timer del CW Trainer y el callback de audio de JS8). Despues de esos arreglos el barrido sigue
  mostrando caidas aleatorias.

---

## Actualizacion: causa raiz encontrada (misma firma, ahora con nombre)

Barrido nuevo con el build que ya trae el handler de crashes (`prueba-23-postfix.log`): **5 caidas**
(CwTrainer, Eq, Status, Callsign, Recorder) y 18 apps OK. Otra vez caidas **distintas** a las
corridas anteriores: sigue siendo endemico y ajeno a la app.

Lo nuevo: en los logs de esos crashes aparece el mensaje de glibc **antes** del `CRASHLOG`:

```
*** buffer overflow detected ***: terminated
CRASHLOG sig=6 fallo=0x669b pc=0xb5dc6dd4 lr=0xb5dc6dc8 sp=0xb46fe838 base=0x4c9804
```

Es el chequeo **fortify de glibc** (`*** buffer overflow detected ***`), que termina en `abort()`
dentro de **libc** (los `pc` caen en el rango de `libc.so.6`, `b5cb0000-b5e2c000`, medido en
`/proc/<pid>/maps` de la GUI viva): de ahi el `sig=6`.

Resolviendo las **pilas** de esos crashes por direccion de enlace (invariante al ASLR,
`link = link(main) + (pc - base)`) salen solo **10 direcciones distintas**, todas en un mismo
cluster (0x2E000-0x33000) y todas en un solo camino:

```
audio.c: on_state_change / read_callback / monitor_cb
      -> cw_put_audio_samples   (src/cw.cpp:212)
      -> cw_decoder_signal      (src/dsp.cpp:252 llama a cw_put_audio_samples)
```

Detalle de las 40 pilas en `pilas-resueltas.txt`.

Dato que cierra: las "pilas" estaban llenas de pares tipo `000b000b`, `000c000d`, `000a000a`: eso no
es una pila, es **audio** (pares de 16 bits). El desborde ocurre sobre datos de audio.

**Hipotesis fundada del mecanismo** (a confirmar por quien arregle): `cw_put_audio_samples` usa el
buffer circular de liquid-dsp sin mirar el contrato de `cbuffercf_read` — el read devuelve en `n` la
cantidad **contigua** disponible (puede ser menor que `desired_num` cuando el buffer da la vuelta), y
el codigo usa `desired_num` para el DSP y para el `release`. Cuando el buffer circular envuelve, se
lee/escribe mas alla de lo contiguo: es un desborde, y da la casualidad (depende del timing del audio)
que explica por que caen apps distintas en cada corrida.
