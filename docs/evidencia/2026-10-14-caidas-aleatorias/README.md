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
