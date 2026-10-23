# Issue #292 — LSB: la señal audible aparece a la derecha de la raya

Diagnóstico **fundado** (2026-10-23). Corrida `factory/diag-lsb-292` sobre
`origin/main` (`b7660ed0`).

## Veredicto (una línea)

**El GUI es coherente consigo mismo: NO es un defecto del render.** La posición
de la señal y la de la barra del passband usan la **misma** escala px↔Hz; lo
único que cambia por modo es la barra. Por lo tanto la anomalía ("se oye a la
derecha con la barra a la izquierda") está **aguas arriba (BASE/demodulación o
centro de la PSD)**, y **no** hay que tocar el waterfall ni el espectro. La
evidencia y el protocolo para fundar la causa en la radio van abajo.

Lo prohibido (espejar la PSD) seguiría siendo incorrecto: el fixture demuestra
que el render ya es espejo-coherente y que espejarlo **rompería el sentido de
sintonía** (freq↑ ⇒ señales←).

---

## Cómo se fundó (medido, no hipótesis)

### 1. La evidencia del owner SÍ está en disco (no hace falta suponerla)

`/home/pablo/desa/x6200/assessment-194/scratch/cw244/` (scratch local del owner,
fuera del repo). Se usaron: las capturas `captura-usuario-2252.png`, `fb-AM.png`,
`fbm-LSB.png`, `fbm-USB.png`, `fbx-LSB.png`, `fbx-USB.png`, los framebuffer
crudos `fA.raw` / `fB.raw` (800×480×4 BGRA rotado) y el audio `audio4.raw`,
`ftAudio.raw`, `ff-LSB.raw`, `ff-USB.raw`, `calAudio.raw`, `fAud.raw`. Los
scripts `measure_captures.py` y `measure_audio.py` los vuelven a medir; su
salida está en `capture_measurements.txt` y `audio_measurements.txt`.

### 2. La barra del passband del owner se reproduce EXACTA con span=48 kHz

Medido en `captura-usuario-2252.png` (LSB, 7.074.000): la barra está en
**x ∈ [352, 397]** (izquierda de la raya). Con el eje de la captura
(span 48 kHz, centro x=400, 60 Hz/px; la barra "Normal 2,7k" = **45 px**):

- `spectrum_filter_band(-2850, -150, 48000, 800)` → **[352, 397]** ✓

Es decir: `flo=150`, `fhi=2850`, y el ancho medido (45 px = 2700 Hz) fija el eje
en **60 Hz/px (span 48 kHz)**. La barra dibujada es exactamente la que el código
calcula para LSB. (USB da [403, 448] a la derecha; AM [352, 448] simétrico —
`capture_measurements.txt`.)

### 3. El render es **independiente del modo** en la posición de la señal

Con el `spectrum.c` **real** y los stubs de `tests/native` (fixture
`lsb_axis_fixture.c`, `sh run.sh`), alimentando un portador sintético en un bin
conocido y midiendo el x del pico y el rectángulo de la barra:

```
-- portador a +1000 Hz (a la DERECHA del centro) --
AM   | pico x=417 | banda [352..448] (-2880..+2880 Hz)
LSB  | pico x=417 | banda [352..397] (-2880.. -180 Hz)
USB  | pico x=417 | banda [403..448] ( +180..+2880 Hz)
-- portador a -1000 Hz (a la IZQUIERDA del centro) --
AM   | pico x=383 | banda [352..448]
LSB  | pico x=383 | banda [352..397]
USB  | pico x=383 | banda [403..448]
```

El **pico cae en el mismo x en AM/LSB/USB** (417 para +1000 Hz, 383 para
−1000 Hz). Sólo se mueve la barra. Y la barra de LSB [352..397] coincide con la
medida en la captura del owner. La coherencia de escala queda probada: un
portador en el borde del passband de LSB (−150 Hz) cae en el borde de la barra
(x=398 vs 397, ±1 px por el redondeo documentado en `spectrum_filter_band`).

### 4. El eje está calibrado y el sentido de sintonía es correcto (capturas del owner)

Par de calibración del owner `fA` (LSB **7.074.000**) y `fB` (LSB **7.076.000**,
+2 kHz): la correlación del waterfall (enmascarando la raya) da **lag = −33 px**,
o sea **+1980 Hz ≈ +2 kHz** → **60 Hz/px** y, al subir la frecuencia, las señales
se mueven a la **izquierda**.

Y el par `fbx-LSB` vs `fbx-USB` (3 s de diferencia, mismo 7.074.000) da
**lag = 0 px**: cambiar de banda lateral **no mueve** la señal en pantalla —
confirma con datos del owner lo que dice el fixture (el render no espeja por
modo).

### 5. El write de modo/filtros al BASE es el de la referencia y el del default OEM

- `src/radio.c` → para SSB (LSB y USB) manda **los mismos valores positivos**:
  `x6200_control_rx_filter_set_low(+150)` y `..._set_high(+2850)`
  (`radio.c:451-492`). El modo va aparte (`x6200_control_vfo_mode_set(A, 0)` con
  `x6200_mode_lsb=0`, `radio.c:401-412`).
- El empaquetado `(low<<16)|high` **reproduce exacto el default OEM** del
  registro SSB de la lib `aether-x6200-control`:
  `x6200_filter_ssb = 0x00960b22` → `low=0x0096=150`, `high=0x0b22=2850`
  (`src/low/control.c`, bloque "Copy from captures OEM"). La otra captura OEM
  del mismo archivo (`0xb80b2c01`, comentada "3000, 300") es también una **banda
  de audio positiva** (≈300..3000 Hz), con un quirk de byte-order por campo. O
  sea: el registro SSB guarda una **banda de audio positiva, sin signo de modo**;
  el lado lo decide el registro de modo, que el GUI escribe bien.
- La lib del X6100 usa el mismo criterio: `filter1_low=50`, `filter1_high=2950`
  positivos (`/home/pablo/desa/x6200/.x6100_ctl/src/low/control.c`).
- La app de referencia X6100 hace lo mismo: manda `low`/`high` positivos para
  LSB y USB (`/home/pablo/desa/x6200/.x6100_ref/src/radio.c:676-700`).

Conclusión: **no hay evidencia de una inversión del write para LSB**. El write
del GUI es el de la referencia y el del default OEM.

### 6. Por qué la pantalla es coherente por construcción (código)

- La señal se dibuja por **índice de bin**, sin conocer el modo:
  `x = i*w/SPECTRUM_SIZE`, `v = buf[i]` (`src/spectrum.c:193-236`); el waterfall
  igual (`src/waterfall.c:511-554`, mapeo monótono, sin espejo por modo); la
  interpolación 512→800 es monótona y el centro (bin 256 → salida 400) es el
  centro (`src/dsp.cpp:43-108`).
- La barra es el **único** elemento que cambia por modo, y sale
  **exclusivamente** de `cfg_cur.filter.real.from/to` con la **misma** escala:
  `xa = w/2 + round(from*w/span)` (`src/spectrum.h:63-93`,
  `src/cfg/mode.c:550-586` con LSB → `[-high, -low]`).
- La raya central se dibuja en `w/2` (`src/spectrum.c:372-388`).

⇒ Un portador a **+X Hz** del centro y el **borde de la barra a +X Hz** caen en
el **mismo x**. No existe en el GUI un camino que los separe.

### 7. El audio del owner (orden de magnitud, no decisivo)

`measure_audio.py` mide los RAW de audio del owner (mono int16 @ 48 kHz):

```
audio4.raw     1.37s | ~2302 Hz
ftAudio.raw    1.37s | 1538 Hz (+ 1833, 1913, 2053 Hz)
ff-LSB.raw     1.37s | ~1633..1642 Hz
ff-USB.raw     1.37s | ~1633..1660 Hz
calAudio.raw   1.37s | 1461 Hz (+ 1737 Hz)
fAud.raw       1.37s | 1460 Hz (+ 1909 Hz)
```

Es decir, tonos audibles de **~1,5..2,3 kHz** — el orden de magnitud del
`~1854 Hz` del issue. **No** alcanza para decidir H1/H2/H3: falta el modo y la
estación de cada grabación y su sincronía con la pantalla. El mismo tono
(~1,6 kHz) en `ff-LSB` y `ff-USB` es lo esperable para un batido de portadora
(el pitch es `|offset|`, igual en cualquier banda lateral), así que tampoco
discrimina: la discriminación es la del protocolo (paso 5, prueba de oído).

---

## Estado de las 3 hipótesis del issue

**H1 — lado de demodulación invertido en LSB (el BASE demodula la banda de
arriba).**
*Estado: SOPORTADA como explicación líder; NO fundada (falta la radio).*
Explica las tres observaciones juntas: pantalla coherente (el render no espeja),
la señal audible a la derecha (el BASE demodula el lado de arriba) y el sentido
de perilla correcto (el write de frecuencia es independiente). No se puede
confirmar sin radio: el write del GUI (modo y filtros) es el correcto
(§5), así que, de ser H1, es el **firmware del BASE**.

**H2 — centro del espectro/PSD desplazado.**
*Estado: REFUTADA como defecto del render; NO excluida como offset del lado BASE.*
El render usa **un solo** centro (x=400) para la señal y para la barra (fixture
§3 + calibración §4). Además, un corrimiento **grande** (p. ej. la PSD con el DC
en el bin 0) queda refutado por las capturas: en LSB a 7.074 los picos más
fuertes están a **+900/+1620 Hz** de la raya, o sea cerca del centro —
imposible si el centro de la PSD fuera el extremo. Pero si el BASE entrega la
PSD centrada en `tuned + D` con un `D` **chico** (p. ej. un LO/IF shift que el
GUI no modela: `if_shift` es sólo plumbing y `cfg_cur.lo_offset=0`, ver
HALLAZGOS), el GUI dibujaría todo corrido en D — y no hay forma de medir D sin
una portadora de frecuencia conocida. Queda **PENDING** (protocolo abajo).

**H3 — la frecuencia sintonizada es el borde del passband (no la portadora).**
*Estado: NO refutada; por sí sola NO explica el síntoma.*
Si el borde fuera el límite superior del passband de LSB, el audio quedaría
por debajo del VFO (izquierda) y la pantalla seguiría coherente — no explica
"se oye a la derecha". Queda como variante a descartar en la radio.

> Medido vs hipótesis: §1-§7 son **mediciones** (fixture, capturas del owner,
> audio y fuentes). Las 3 hipótesis son **interpretaciones** de lo medido; su
> veredicto final necesita la radio.

---

## Protocolo para el owner (lo que no se puede hacer sin radio)

**PENDING — hardware.** Requiere la radio. Span 48 kHz (60 Hz/px, centro x=400,
la raya). Portadoras conocidas: **CHU 7.850.000** y **WWV 10.000.000**.

1. **AM, portadora conocida.** Sintonizá exacto 7.850.000 en AM. Medí el x del
   pico/portadora. `dA = (x_carrier − 400) · 60 Hz`. Debería ser ≈ 0.
2. **Calibración ±2 kHz.** Subí a 7.852.000: la portadora debe correr a
   `x = 400 − 33 px`. (Ya verificado con `fA`/`fB`; repetilo con portadora
   conocida.)
3. **LSB, portadora audible.** En LSB, movete hasta **oír** la portadora
   (queda un tono de `f_audio` Hz). Medí su x: `p_seen = (x_carrier − 400) · 60 Hz`
   (positivo = derecha).
4. **USB, portadora audible.** Igual que (3) en USB.
5. **Prueba de oído.** Una estación de **voz** en LSB y en USB: ¿es inteligible
   en las dos, o en LSB suena "invertida/donkey" y en USB normal?

### Tabla de decisión

En LSB, con la portadora audible de pitch `f_audio` (Hz):

| AM: `dA` (portadora) | LSB: `p_seen` | Oído | Veredicto |
|---|---|---|---|
| ≈ 0 | `p_seen ≈ −f_audio` (portadora a la **izquierda**) | normal | **coherente** — la observación original fue de una señal **fuera** del passband |
| ≈ 0 | `p_seen ≈ +f_audio` (portadora a la **derecha**, como USB) | voz LSB invertida | **H1** — el BASE demodula la banda de arriba en LSB |
| `dA = D ≠ 0` | `p_seen ≈ −f_audio − D` (todo corrido D) | normal | **H2** — centro de la PSD/display desplazado en D (¿LO/IF del BASE?) |
| ≈ 0 | portadora en el **borde** de la barra, no dentro | normal | **H3** — la frecuencia sintonizada es el borde del passband |

Criterio único y falsable: **el pitch audible `f_audio` debe coincidir con la
magnitud del offset en px (×60 Hz/px) de la portadora dibujada, con signo
opuesto en LSB** (`p_seen = −f_audio`) y **el mismo signo en USB**
(`p_seen = +f_audio`). Cualquier discrepancia sistemática es D (H2); el signo
cruzado en LSB es H1.

---

## Qué se entrega / qué NO

- **No hay fix del GUI**: el render es coherente (evidencia §3-§4) y el write de
  modo/filtros es el correcto (§5). Cualquier parche al render repetiría el
  error ya revertido (`fix/waterfall-lsb-mirror`) o rompería el sentido de
  sintonía.
- Evidencia reproducible en esta carpeta:
  - `lsb_axis_fixture.c` + `run.sh` + `output.txt` — el fixture sobre el
    `spectrum.c` real (mide pico vs barra en AM/LSB/USB).
  - `measure_captures.py` + `capture_measurements.txt` — la medición de las
    capturas del owner (raya, picos, barra y corrimientos).
  - `measure_audio.py` + `audio_measurements.txt` — los tonos dominantes del
    audio del owner (orden de magnitud, no decisivo).
- **PENDING (hardware)**: confirmar H1/H2/H3 con el protocolo de arriba.
- **BLOCKED**: la semántica del lado SSB del firmware del X6200 (si el registro
  SSB es "banda de audio positiva + modo", como sugiere el default OEM, o si
  además necesita signo por modo). No hay fuente en el repo ni en la lib que lo
  fije; requiere la radio.
