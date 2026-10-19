# TX cualitativa en CB (27 MHz) en el X6200 del owner — teclado CW, potencia e interlock

**Fecha**: 2026-10-19 (hora host 12:26–12:36 -03; hora radio 15:26–15:36 UTC)
**Issue**: [#146 — Módulo CB 27 MHz](https://github.com/iariap/x6200_gui/issues/146) §7.2 (interlock de TX)
**Radio**: X6200 del owner, `192.168.0.236` (la `192.168.18.67` no respondía)
**Build que corría** (medido): GUI md5 **`67387bfc4391cf9b1d6c4c1aa3b0d56f`**, 13.849.516 B
(el build del **#154 Propagation**, commit `02090454`; **lo desplegó otra sesión**, no esta QA)
**Autorización**: el owner pidió esta prueba y autorizó TX de prueba con su antena (sin carga fantasma).

> Esta corrida **no** es la F0 con instrumentos (#156, cerrada como parcial): acá se busca
> evidencia **cualitativa** de que el equipo transmite en un canal CB y de si el **interlock**
> bloquea fuera de la ventana. **No hay wattmeter ni analizador.**

---

## 0. Veredicto

| Paso | Pregunta | Resultado | Fundamento |
|---|---|---|---|
| 1 | TX en **27.185 MHz** (CB C, CH 19) por el teclado CW a la potencia configurada | **✓ (medido)** | `{"cmd":"cw_send"}` → `tx:true` con el ritmo del CW; medidor del equipo en **PWR/SWR** (no S-meter) y `ON AIR`; captura [`02-durante-tx-cb19.png`](02-durante-tx-cb19.png) |
| 2 | Interlock bloquea fuera de la ventana: **26.515 MHz** (CB B, CH 1, fuera del SBC) | **✗ (medido)** | `cw_send` → **`tx:true`**, el equipo **transmitió**; **NO** apareció `TX blocked: not a CB channel`; captura [`10-interlock-26515-fb.png`](10-interlock-26515-fb.png) |
| 3 | Control en **27.185 MHz** (debe transmitir) | **✓ (medido)** | mismo `cw_send` → `tx:true` (corrida 1) |
| 4 | No-regresión en **28.074 MHz** (10 m): el interlock no interviene | **✓ (medido)** | `cw_send` → `tx:true`; sin `CH` (no es CB); captura [`12-10m-28074-fb.png`](12-10m-28074-fb.png) |
| 5 | Potencia real de salida | **NO MEDIDA** | sin wattmeter; el readback `state.pwr` sigue en `50` (#157); el medidor del equipo marcó **SWR ~3,5** y una barra **PWR baja** (≈1–2 W), con batería a **8,6 V** (baja) |

**Conclusión**: el equipo **transmite** en CB (27.185) y en 10 m (28.074) con el teclado CW, pero
**el interlock de CB NO bloquea el teclado CW**: en 26.515 el `cw_send` transmite igual. La causa
está en el código (§3): el interlock vive en `radio_set_ptt()` y el keyer CW usa
`radio_set_morse_key()` (un GPIO), que **no** pasa por `radio_set_ptt()`.

> El paso 2 **no** es un fallo del keying (el paso 3 lo descarta): es que el interlock **no cubre
> ese camino**. El TX **no** se declara validado (#146 §11-F0 sigue pendiente de instrumentos).

---

## 1. Método

- **Control por socket** de la GUI (`/tmp/x6200-remote`, mismo protocolo que la página `/remote`).
  El socket **no tiene PTT**; el único camino de TX accionable por software es el **teclado CW**:
  `{"cmd":"cw_send","text":"PARIS PARIS"}` / `{"cmd":"cw_stop"}` con la app **CW** abierta
  (`{"cmd":"app","text":"CW"}`).
  - **Ojo (corrección del encargo)**: el comando usa el campo **`text`**, **no** `val`
    (`src/remote_server.cpp:853` exige `cmd->text`; con `val` devuelve `Invalid text`).
- **Potencia**: `{"cmd":"pwr","val":40}` (escala 1..100; el SDK hace `p == val*10`, o sea ≈4 W,
  tope de CB `cb_pwr_max`).
- **Captura**: `tools/captura-radio.sh` (framebuffer) y, **durante el keying**, un `dd` del
  framebuffer **en la propia radio** (el PNG se armó después) para no perder el instante de TX.
- **Transcripción cruda** de todos los `get`/respuestas: [`transcript-corrida1.log`](transcript-corrida1.log)
  y [`transcript-corrida2.log`](transcript-corrida2.log).

### 1.1 Condiciones

- **Antena del owner** (no hay carga fantasma). **ATU1** visible; **sin XPA125B**.
- Radio en **RX** antes de empezar; se transmitió **breve** (CW `PARIS`/`PARIS PARIS`, ~1–3 s por
  vez). Potencia configurada por socket al **tope de CB (≈4 W)**.
- **Batería 8,6 V** (`ext 14,2 V`): por debajo del umbral de 10,8 V del proyecto → posible límite
  de potencia del equipo.

---

## 2. Resultado por paso (salida cruda)

### Paso 1 — 27.185 MHz (CB C, CH 19), CW, `pwr 40`

```text
$ /tmp/x6200-remote '{"cmd":"freq","val":27185000}'   -> {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"app","text":"CW"}'        -> {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"pwr","val":40}'           -> {"t":"ok"}
$ /tmp/x6200-remote '{"cmd":"get"}'
{"t":"state","vfo":"A","freq":27185000,"freq_bg":27185000,"mode":"CW","band":"CB C",
 "filter":2820,"flo":190,"fhi":3010,...,"pwr":50,...,"tx":false,"app":"CW",
 "ax":{"cw":1,"wpm":15,...}}
```

`band":"CB C"` y la celda VFO **`CH 19`** en la captura [`01-cb19-antes-tx.png`](01-cb19-antes-tx.png).
El `pwr` del `get` sigue en **50** (readback roto, #157): **no** sirve para confirmar el setpoint.

### Paso 2 — TX en 27.185 (`cw_send "PARIS PARIS"`)

```text
$ /tmp/x6200-remote '{"cmd":"cw_send","text":"PARIS PARIS"}'   -> {"t":"ok"}
t=1 tx=true   t=2 tx=true   t=3 tx=false  t=4 tx=true   t=5 tx=true   t=6 tx=true
t=7 tx=false  t=8 tx=false  t=9 tx=true   t=10 tx=true  t=11 tx=true  t=12 tx=false
t=13 tx=true  t=14 tx=true
$ /tmp/x6200-remote '{"cmd":"cw_stop"}'   -> {"t":"ok"}
```

`tx` **sigue el ritmo del CW** (puntos/rayas): el teclado CW **sí** acciona el TX.
Captura **durante** el keying [`02-durante-tx-cb19.png`](02-durante-tx-cb19.png) (zoom del medidor
en [`02-metro-tx-cb19-zoom.png`](02-metro-tx-cb19-zoom.png)): celda **`CH 19`**, botón **`ON AIR`**,
y el medidor pasa de **S-meter** (RX, [`01-metro-rx-zoom.png`](01-metro-rx-zoom.png)) a **PWR/SWR**
(TX). El puente del equipo marca **SWR ≈3,5** → hay **RF real** (antena no resonante en 27 MHz).

### Paso 3 — Interlock: 26.515 MHz (CB B, CH 1, fuera del SBC)

```text
$ /tmp/x6200-remote '{"cmd":"app","text":"off"}'
$ /tmp/x6200-remote '{"cmd":"freq","val":26515000}'
$ /tmp/x6200-remote '{"cmd":"app","text":"CW"}'      # la banda CB B fuerza AM; la app CW lo pone CW
$ /tmp/x6200-remote '{"cmd":"get"}'
{"t":"state",...,"freq":26515000,"mode":"CW","band":"CB B",...,"pwr":50,...,"tx":false,
 "app":"CW","ax":{"cw":1,...}}
$ /tmp/x6200-remote '{"cmd":"cw_send","text":"PARIS"}'   -> {"t":"ok"}
t=1 tx=true  t=2 tx=true  t=3 tx=true  t=4 tx=true  t=5 tx=true  t=6..10 tx=false
```

**`tx:true`**: el equipo **transmitió en 26.515 MHz**. **NO** apareció el mensaje
`TX blocked: not a CB channel`. Captura durante el keying
[`10-interlock-26515-fb.png`](10-interlock-26515-fb.png) (zoom
[`10-metro-interlock-zoom.png`](10-metro-interlock-zoom.png)): celda **`CH 1`**, **`ON AIR`**,
medidor PWR/SWR, **sin** mensaje de bloqueo.

### Paso 3b — Control en 27.185 (corrida 1)

Mismo `cw_send` en 27.185 → `tx:true` (ver [`transcript-corrida1.log`](transcript-corrida1.log)).
Es decir: **el keying funciona en las dos frecuencias**; la diferencia **no** es un fallo del
keying, es que **no hay interlock en este camino**.

### Paso 4 — 28.074 MHz (10 m): el interlock no interviene

```text
$ /tmp/x6200-remote '{"cmd":"freq","val":28074000}'
$ /tmp/x6200-remote '{"cmd":"app","text":"CW"}'
$ /tmp/x6200-remote '{"cmd":"get"}'
{"t":"state",...,"freq":28074000,"mode":"CW","band":"10m",...,"tx":false,"app":"CW","ax":{"cw":1,...}}
$ /tmp/x6200-remote '{"cmd":"cw_send","text":"PARIS"}'   -> {"t":"ok"}
t=1..6 tx=true   t=7..8 tx=false
```

Transmite en 10 m; la celda VFO **no** muestra `CH` (no es CB). Captura
[`12-10m-28074-fb.png`](12-10m-28074-fb.png).

---

## 3. Causa (código): el interlock no cubre el teclado CW

- El interlock vive en **`radio_set_ptt()`** (`src/radio.c:1068`), que es el único punto que llama
  a `cb_tx_allowed()` y muestra `TX blocked: not a CB channel` (`src/radio.c:1073`).
- El teclado CW **no** pasa por ahí: `cw_send` → `cw_encoder_send()` → `key_down()` →
  **`radio_set_morse_key()`** (`src/cw_encoder.c:62`), que es sólo un GPIO
  (`src/radio.c:1161`: `gpio_safe_set(x6200_pin_morse_key, …)`). **Nunca** llama a
  `radio_set_ptt()`.
- Los caminos que **sí** pasan por el interlock son el **PTT físico** (`src/main_screen.c:906`),
  **FT8** (`src/dialog_ft8.c:1241`), **mensajes de voz** (`src/dialog_msg_voice.c:357`) y **CAT**
  (`src/cat.cpp:790`). El keyer CW queda **afuera**.

El build **tiene** el interlock (strings `TX blocked`, `cb_tx_scope` presentes en el binario);
el problema es que **ese camino de TX no lo consulta**.

---

## 4. Qué quedó medido y qué NO

| Pregunta | Resultado | Fundamento |
|---|---|---|
| ¿Transmite en 27.185 (CH 19) con el teclado CW? | **SÍ** | `tx:true` + medidor PWR/SWR + `ON AIR` |
| ¿El interlock bloquea el teclado CW en 26.515? | **NO** | `tx:true` sin `TX blocked` |
| ¿Transmite en 10 m (28.074) sin que el interlock intervenga? | **SÍ** | `tx:true` |
| ¿Potencia real ≈4 W? | **NO MEDIDA** | sin wattmeter; readback roto (#157); PWR del equipo bajo (batería 8,6 V) |
| ¿Se escucha en otro receptor? | **NO VERIFICADO** | no hay SDR/receptor disponible en el host; no se inventa |
| ¿PTT del micrófono / AM-FM a potencia? | **NO MEDIDO** | sigue pendiente de la F0 con instrumentos (#156) |

---

## 5. Incidencias observadas durante la corrida (no del CB)

1. **La GUI se cayó 2 veces** durante los cambios de frecuencia/modo: `CRASHLOG sig=11
   pc=0xb6186ae8 lr=0xb618d4f0` (misma firma del **#131**, la conocida de LVGL). Se ve en
   `x6200_log_2026-10-19_15-30-31.txt` y `…15-31-10.txt` de la radio. Eso invalidó el primer
   intento de los pasos 3/4 (el `freq` dio `Connection refused`); se repitieron (corrida 2).
2. **La radio quedó inalcanzable** ("No route to host", 0 hosts SSH en 192.168.0.0/24 y
   192.168.18.0/24) al terminar el paso 4, **antes** de poder ejecutar el control positivo por FT8.
   Es la firma del **#121** (cuelgue). Volvió **sin reinicio** (uptime continuo, mismo PID 18416) a
   los ~10 min; entonces **se restauró** frecuencia/modo/app (14.285 MHz, CW, 20m SSB, app off) y la
   potencia a **1,5 W** (último valor conocido, del arranque de las 15:18), con `tx:false`.
   Captura: [`99-restaurado.png`](99-restaurado.png) (abajo, `TX power: 1.5 W`).

---

## 6. Legal (Argentina) — obligatorio

- El **Servicio de Banda Ciudadana (SBC)** está en **26.960–27.410 MHz** y exige **autorización por
  estación** y **equipo homologado/registrado** (Res. SC 47/1982; CABFRA 2025 de ENACOM).
- El **X6200 no es un equipo CB homologado**; transmitir en 27 MHz con él queda fuera del régimen
  del SBC y fuera del servicio de aficionados (10 m empieza en 28,000 MHz). **Modificar el firmware
  no cambia la homologación.**
- Esta corrida transmitió en **27.185** (canal 19 CEPT, dentro del SBC) y **fuera** del SBC en
  **26.515** (grid B, el intento del interlock) y **28.074** (10 m, aficionados), en CW, ~1–3 s por
  vez, con **autorización explícita del owner y su equipo**. **No habilita a operar** en CB.
- Los grids A/B/D (26,065–26,955 y 27,415–27,855) **no** son SBC argentino: el default
  `cb_tx_scope = 0` los deja en RX **para los caminos con interlock**; este hallazgo muestra que el
  **teclado CW los esquiva**.

---

## 7. Evidencia

| Archivo | Qué contiene |
|---|---|
| [`transcript-corrida1.log`](transcript-corrida1.log) | Transcripción cruda corrida 1 (27.185 + intentos 26.515/10 m) |
| [`transcript-corrida2.log`](transcript-corrida2.log) | Transcripción cruda corrida 2 (26.515 interlock + control + 10 m, con el modo CW correcto) |
| [`00-antes.png`](00-antes.png) | Estado antes de la corrida |
| [`01-cb19-antes-tx.png`](01-cb19-antes-tx.png) | 27.185, CH 19, CW, RX antes de TX |
| [`02-durante-tx-cb19.png`](02-durante-tx-cb19.png) · [`02-metro-tx-cb19-zoom.png`](02-metro-tx-cb19-zoom.png) | TX en 27.185: `ON AIR`, medidor PWR/SWR (SWR ≈3,5) |
| [`01-metro-rx-zoom.png`](01-metro-rx-zoom.png) | El medidor en RX (S-meter) para contrastar |
| [`10-interlock-26515-fb.png`](10-interlock-26515-fb.png) · [`10-metro-interlock-zoom.png`](10-metro-interlock-zoom.png) | **26.515, CH 1, TX sin bloqueo** (clave del hallazgo) |
| [`12-10m-28074-fb.png`](12-10m-28074-fb.png) | 28.074 (10 m), TX, sin celda `CH` |
| [`99-restaurado.png`](99-restaurado.png) | Estado final **restaurado** (14.285 MHz, CW, `TX power: 1.5 W`, app cerrada) |

- **Build**: GUI `67387bfc4391cf9b1d6c4c1aa3b0d56f` (13.849.516 B). **No lo desplegó esta QA**:
  lo desplegó otra sesión a las 15:18 (hora radio), después de que esta corrida empezara sobre
  `9a094c421b0e` (que **no** tenía el módulo CB). Esta QA **no flasheó ni desplegó** nada.
- **Estado de la radio al terminar**: **restaurada** al estado previo
  `14.285 MHz / CW / 20m SSB / app off / pwr 1,5 W / tx:false` (verificado por socket y por la
  pantalla, [`99-restaurado.png`](99-restaurado.png)); volvió sola tras el corte de red (§5).

## 8. Pendientes

1. **Decidir el fix del interlock del keyer**: que `radio_set_morse_key()` (o el keyer) consulte
   `cb_tx_allowed()` como `radio_set_ptt()`. Issue nuevo cargado (#160).
2. **Control positivo del interlock por un camino que sí lo use** (FT8/PTT físico/CAT) en 26.515:
   no se pudo correr (la radio quedó inalcanzable antes). Confirmaría que el interlock funciona y que
   el hueco es sólo del keyer.
3. **F0 con instrumentos** (#156) para potencia real y armónicos; y confirmar el setpoint `pwr`
   cuando #157 esté desplegado.
