# Evidencia de contraste — la línea central del waterfall (issue #140, orden del owner 2026-10-18)

Orden del owner, textual (2026-10-18): *"no veo la frecuencia que estoy
sintonizando; no se ve la línea"*.

La línea existe (issue #140) pero, dibujada como estaba, no se leía:

| | antes (HEAD `9872b614`) | después (este fix) |
| --- | --- | --- |
| color | `TC(panel_border)` del tema | `TC(text)` del tema |
| opacidad | `LV_OPA_60` (60 %) | `LV_OPA_COVER` (100 %) |
| blending | `LV_BLEND_MODE_ADDITIVE` | normal (el color del tema) |
| ancho | `waterfall_signal_width`, default **1** px | `waterfall_signal_width`, default **3** px (clamp 1..8) |

## Qué prueba esta carpeta

1. **La línea se ve, de punta a punta, en los seis temas** — ver los crops ×4 de
   la columna central `linea-aurora-antes.png` / `linea-aurora-despues.png` (el
   tema del owner) y `linea-sunlit-antes.png` / `linea-sunlit-despues.png` (el
   tema claro, donde el gate detecta la regresión), más las capturas del **banco
   QEMU con el binario ARM real** (`banco/`, tres temas con la línea encendida y
   su control apagado) y su antes/después en los crops ×4 (`banco/linea-banco-*-x4.png`).
2. **El contraste subió en los seis temas**, medido con el *mismo* algoritmo del
   gate (`x6200-ab themes` → `nontext_checks.measure_nontext`: percentil 90 de la
   tinta contra percentil 10 del piso de ruido del waterfall), y también en la
   medida local fila por fila (Tabla 1).
3. **El objeto declarado en el gate es sensible**: con los renders *viejos*
   `waterfall_center_line on main-tema-sunlit-agua` **falla** (2.40:1 < 3:1) y con
   los nuevos pasa (14.51:1). Salida real:

   ```text
   $ uv run x6200-ab themes --repo ../.. --renders-dir /tmp/wf140/antes/on
   themes FAIL: ... 0 failure(s)
       OK   waterfall_center_line on main-espectro-agua = 7.12:1 (min 3.0:1) [object lighter; #71b4cd vs #23193e]
       FAIL waterfall_center_line on main-tema-sunlit-agua = 2.40:1 (min 3.0:1) [object darker; #51a9df vs #f1f7fb]
   ```

4. **El cambio está confinado a la línea**: los 7 renders con el parámetro
   `waterfall_center_line` apagado (`off-*.png`) son **byte-idénticos** antes y
   después (mismo md5 en las dos pasadas; ver `renders.md5`). Lo único que se
   mueve son los 3 px de la línea (`x=398..400`, antes 1 px en `x=400`) y la
   banda de la misma línea que cae en la barra de botones.
5. **Por qué `TC(text)` y no `TC(panel_border)`** (Tabla 2): con `LV_OPA_COVER`,
   `panel_border` da 3.13:1..9.21:1 en la convención del gate y **0 % de las filas
   llegan a 3:1 en cinco de los seis temas**; `TC(text)` da 11.93:1..16.55:1 y
   sube todos los temas.
6. **Por qué se cae `LV_BLEND_MODE_ADDITIVE`** (Tabla 2): con
   `COVER + ADDITIVE` cinco temas mejoran, pero **Sunlit se lava** (1.79:1 en el
   gate, 1.12:1 local, 0 % de las filas ≥ 3:1): la suma satura a blanco sobre un
   piso claro (medido: tinta `#ffda66` sobre `#f1f7fb`).
7. **El assert del arnés rechaza la regresión**: `tools/ui_harness/src/screens.c`
   (`settle_waterfall_center_line`) exige ancho == `waterfall_signal_width`,
   `LV_OPA_COVER` y `TC(text)`. Salida real del binario con la variante
   `panel_border` (el build del harness con esa variante):

   ```text
   $ ./ui_harness --scenario ../fixtures/main-tema-aurora-agua.scn --png out/var-aurora.png
   main-tema-aurora-agua: la linea central no usa el rol text del tema (#ff8a7ba8 vs #fff8eeff): no alcanza el 3:1 contra el fondo del waterfall (src/waterfall.c)
   ```

   (log completo: `assert-panel-border.log`)

## Método

### 1. Renders (harness host, Docker `x6200-uiharness`)

Un build por pasada, sobre 7 fixtures *con* la línea (`main-espectro-agua` es
Aurora) y las mismas 7 con el parámetro apagado (el fondo del waterfall). Los
`.scn` "off" se generan del propio fixture cambiando `waterfall_center_line true`
→ `false` (no viven en `tools/ui_harness/fixtures/`).

```sh
WT=/home/pablo/desa/asf/state/worktrees/x6200/wf-line-140-coder
./renders.sh "$WT" /tmp/wf140/antes   antes      # el src/ de HEAD (sin el fix), via stash temporal
./renders.sh "$WT" /tmp/wf140/despues despues    # el src/ del worktree (con el fix)
./renders.sh "$WT" /tmp/wf140/var-panel-border variante-panel-border
./renders.sh "$WT" /tmp/wf140/var-additive      variante-additive
```

`renders.sh` es el script de esta carpeta: hace el build dentro de Docker con el
repo montado en `/src` (correr el binario desde el host hace caer todos los temas
a Aurora) y deja los PNG + el `.json` de cada render en `<salida>/{on,off}/`. Las
pasadas que tocan el `src/` (`antes`, `variante-*`) lo restauran al salir, incluso
si algo falla, y recompilan el harness con el `src/` restaurado.

### 2. Medición

`medir-linea.py` (PIL + numpy, importa el medidor del gate para no tener dos
definiciones de contraste) da dos números por render:

* **gate**: el algoritmo del gate sobre el objeto declarado en
  `tools/ab_testing/quality/nontext.yaml` — objeto `[398, 255, 3, 150]` (el
  trazo de la línea), fondo `[220, 255, 40, 150]` (el piso de ruido a su
  izquierda), tinta percentil 90, fondo percentil 10. Es la medida que exige
  ≥ 3:1, y su valor para `main-espectro-agua` es el que imprime el gate (14.58:1).
* **peor fila**: la medida local, fila por fila (`y=255..404`, las filas del
  objeto declarado), contra la mediana de 8 px a cada lado de la línea; la
  máscara sale del diff contra el render con el parámetro apagado. Es el peor
  caso: mide la línea contra el píxel vecino, sin elegir el percentil que la
  favorece.

```sh
cd tools/ab_testing
uv run python ../../docs/evidencia/2026-10-18-wf140-contraste/medir-linea.py \
    --antes-on /tmp/wf140/antes/on --antes-off /tmp/wf140/antes/off \
    --despues-on /tmp/wf140/despues/on --despues-off /tmp/wf140/despues/off
# --json para la salida completa; medicion.txt y medicion-variante-*.txt son las salidas reales
# (las variantes se miden pasando su carpeta como --despues-on y la de despues como --antes-on)
```

## Tabla 1 — antes vs después (renders reales, seis temas)

| Tema | fixture | tinta antes → después | ancho | gate antes | gate después | peor fila antes | peor fila después |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Aurora | `main-tema-aurora-agua` | `#71b4cd` → `#f8eeff` | 1 → 3 px | 7.12:1 | **14.58:1** | 1.90:1 (min 1.77; 0 % ≥ 3) | **2.76:1** (min 2.47; 13 % ≥ 3) |
| Cathode | `main-tema-cathode-agua` | `#c08439` → `#f9d9aa` | 1 → 3 px | 6.07:1 | **14.29:1** | 2.61:1 (min 2.15; 3 % ≥ 3) | **4.17:1** (min 3.71; 100 % ≥ 3) |
| Redshift | `main-tema-redshift-agua` | `#d16a5a` → `#ffb3a0` | 1 → 3 px | 3.89:1 | **11.93:1** | 3.16:1 (min 2.63; 81 % ≥ 3) | **7.41:1** (min 7.00; 100 % ≥ 3) |
| Sprint | `main-tema-sprint-agua` | `#60c4b8` → `#f5f7f9` | 1 → 3 px | 8.54:1 | **16.55:1** | 1.86:1 (min 1.59; 0 % ≥ 3) | **2.89:1** (min 2.59; 31 % ≥ 3) |
| Sunlit | `main-tema-sunlit-agua` | `#51a9df` → `#162436` | 3 → 3 px | 2.40:1 | **14.51:1** | 1.23:1 (min 1.07; 0 % ≥ 3) | **5.97:1** (min 5.48; 100 % ≥ 3) |
| Trail | `main-tema-trail-agua` | `#dee1a2` → `#f2eed8` | 3 → 3 px | 11.66:1 | **13.63:1** | 2.37:1 (min 2.09; 0 % ≥ 3) | **2.69:1** (min 2.43; 11 % ≥ 3) |

Notas:

* Notas leerlas con la convención de cada columna: la columna **gate** es la que
  exige el gate y compara tinta contra el **piso de ruido** (el fondo oscuro del
  waterfall); la columna **peor fila** compara la línea contra el **píxel de al
  lado**, y en los fixtures del arnés ese vecino es el *carrier*, que el generador
  sintético planta exactamente en el centro (`tools/ui_harness/src/signal_gen.c`,
  `c2 = n/2`). O sea: la "peor fila" mide el caso más adverso posible —la línea
  sobre la parte más brillante de la señal—, y aun así sube en todos los temas.
* El "antes" de Sunlit y Trail ya era de 3 px: son los dos temas que fijan
  `waterfall_signal_width: 3` desde el #92; lo que no se leía era el color (60 %
  + `ADDITIVE` de `panel_border`), no el ancho.
* El ancho de 1 px cae en `x=400`; con 3 px la línea ocupa `x=398..400`.
* Debajo de `y≈418` la línea pasa **por debajo** de la barra de botones
  translúcida y ahí su contraste local es ~1.1:1 en los dos estados (no es
  waterfall: es la barra). Fuera de la banda declarada, igual que el objeto del
  gate, que termina en `y=404`.

## Tabla 2 — variantes de estilo descartadas (renders reales, seis temas)

Las tres variantes salen del mismo código con el estilo cambiado; los renders se
hicieron con el `screens.c` de HEAD (el assert nuevo *rechaza* la variante
`panel_border`: ver punto 7 arriba).

| Tema | `panel_border` + COVER + normal | `panel_border` + COVER + ADDITIVE | **elegida: `text` + COVER + normal** |
| --- | --- | --- | --- |
| Aurora | gate 4.28:1 · peor fila 1.26:1 (0 % ≥ 3) | gate 16.17:1 · peor fila 3.10:1 (68 % ≥ 3) | gate **14.58:1** · peor fila **2.76:1** (13 % ≥ 3) |
| Cathode | gate 3.13:1 · peor fila 1.12:1 (0 % ≥ 3) | gate 14.23:1 · peor fila 4.84:1 (100 % ≥ 3) | gate **14.29:1** · peor fila **4.17:1** (100 % ≥ 3) |
| Redshift | gate 3.86:1 · peor fila 2.40:1 (0 % ≥ 3) | gate 8.84:1 · peor fila 6.00:1 (100 % ≥ 3) | gate **11.93:1** · peor fila **7.41:1** (100 % ≥ 3) |
| Sprint | gate 3.35:1 · peor fila 1.71:1 (0 % ≥ 3) | gate 15.62:1 · peor fila 3.02:1 (52 % ≥ 3) | gate **16.55:1** · peor fila **2.89:1** (31 % ≥ 3) |
| Sunlit | gate 9.21:1 · peor fila 3.79:1 (100 % ≥ 3) | gate **1.79:1** · peor fila **1.12:1 (0 % ≥ 3)** ← se lava | gate **14.51:1** · peor fila **5.97:1** (100 % ≥ 3) |
| Trail | gate 3.33:1 · peor fila 1.52:1 (0 % ≥ 3) | gate 15.42:1 · peor fila 3.05:1 (65 % ≥ 3) | gate **13.63:1** · peor fila **2.69:1** (11 % ≥ 3) |

Lectura:

* `panel_border` con COVER no alcanza: la tinta es el borde del panel (un gris
  medio: `#8a7ba8` Aurora, `#87562d` Cathode, `#9a5a50` Redshift, `#4a6e96`
  Sprint, `#14456f` Sunlit, `#5e7a55` Trail) y **0 % de las filas** llegan a 3:1
  en cinco temas. Es exactamente el síntoma que reportó el owner, un paso más
  atrás.
* `ADDITIVE` con COVER arregla cinco temas y rompe el sexto: sobre el piso claro
  de Sunlit la suma satura a blanco (`#ffda66` sobre `#f1f7fb` = 1.79:1).
  Es un canje, no un fix.
* `text` + COVER es el único que deja los seis temas por encima del umbral del
  gate (11.93:1..16.55:1) y multiplica la mediana local por 1.1×..4.9×.

## Banco QEMU — la línea en el firmware ARM real

El arnés de arriba dibuja el waterfall con un generador sintético y corre en el
host. El banco QEMU (`cortex-a7`, framebuffer falso, **sin radio ni FFT**) corre
el **binario ARM real** y muestra la línea sobre el fondo que el producto tiene
**antes** de que llegue el primer frame. Se capturaron tres temas con la línea
encendida y los mismos tres con `waterfall_center_line=0` (control):

```sh
export CHROOT=/tmp/opencode/vfix3-wf140
BINARIO=/tmp/wf140/banco/binario-after          # md5 e39cbe000d6dcb677d9478128e2135ce
./captura-banco.sh banco/aurora-on.png  Aurora on  "$BINARIO"
./captura-banco.sh banco/aurora-off.png Aurora off "$BINARIO"   # control
# ... y lo mismo con Sunlit y Redshift
```

Cada corrida del script escribe el tema y el estado de la línea en `params.db`,
copia el binario al chroot, corre el **gate fail-closed #86**
(`tools/verifica-firmware-port.py`), arranca la GUI en el contenedor
`banco-apps` y espera **hasta-condición con techo** (`TECHO=30`, sin `sleep` a
ciegas): el socket de control, la respuesta a `{"cmd":"get"}`, el repintado
después de un cambio de frecuencia, la **salida de pantalla del aviso de
arranque** (que tapa el waterfall: sin esperarlo, la medición baja de 673 a 651
px) y, por último, la **tinta medida en la columna central** (`x=398..400`,
`y=240..478`). Con la línea apagada la misma espera exige **cero** px de tinta:
si aparece, el script falla. Los seis logs (`banco/*.log`) quedaron con el techo
y el tiempo real de cada espera.

Las seis capturas son del **mismo binario**, `e39cbe000d6dcb677d9478128e2135ce`
(13.773.040 B), y en las seis el gate #86 dio `OK ... 3/3 marcas`:

```text
OK: /tmp/opencode/vfix3-wf140/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13773040 B, 3/3 marcas)
```

`docker ps` antes de la primera captura no tenía ningún `banco-apps`
(`banco/docker-ps-antes.txt`: cinco contenedores `x6200-buildroot` de otras
corridas y nada más), y al cerrar el script queda **0** contenedores
`banco-apps` vivos (última línea de cada log).

### Tabla 3 — la línea medida en el banco (banda `y=240..478`)

| Caso (banco QEMU, ARM real) | color | ancho | filas con la línea | ratio vs vecino | ratio vs piso (gate) |
| --- | --- | --- | --- | --- | --- |
| Aurora (después, banco) | `#f8eeff` | 3 px | 227/239 (95,0 %) | **18.67:1** (min 12.76) | **18.67:1** |
| Redshift (después, banco) | `#ffb3a0` | 3 px | 232/239 (97,1 %) | **12.21:1** (min 2.96) | **12.21:1** |
| Sunlit (después, banco) | `#162436` | 3 px | 226/239 (94,6 %) | 1.34:1 (min 1.02) | 1.34:1 |
| Sunlit (antes, trunk) | `#0b2942` | 3 px | 226/239 (94,6 %) | 1.41:1 (min 1.04) | 1.41:1 |
| Redshift (antes, **radio**) | `#733b36` | 1 px | 45/239 (18,8 %)¹ | 1.98:1 (min 1.92) | 2.27:1 (`#6c3934` vs `#000000`) |

¹ La fila de la radio es la única **sin control** (el parámetro no se apaga a
mano en el equipo): su tinta se mezcla con el ruido y toma `#5b352f`..`#733b36`,
así que el hex exacto aparece en 45 filas. La **geometría** de esa captura la
mide `medir-linea.py` sobre su propia banda (`y=240..479`): la columna central
difiere de sus vecinas en `217/217` filas medibles
(`medicion-banco-radio-antes.txt`). El "color" de la tabla es la tinta de la
fila `y=300`.

Salida completa (una fila por caso, más el histograma de anchos y el px del
control de cada uno): [`medicion-banco.txt`](medicion-banco.txt). En las cuatro
filas del banco el ratio "vs piso" coincide con el "vs vecino" porque el piso y
los vecinos son negro; en la radio el piso (percentil 10 a la izquierda) y el
vecino inmediato no son lo mismo.

Los tres controles `off` dan **0 px** del color esperado en `x=398..400`: la
misma pantalla, el mismo binario, el mismo tema, sin la línea.

### El piso negro del banco es un artefacto, no el piso del producto

La tinta oscura de Sunlit mide 1.34:1 en el banco y, sin embargo, es la que
**más** sube en el arnés (14.51:1 de gate). No es una contradicción: es el piso
del banco.

- En el banco no hay FFT, así que el waterfall nunca recibe un frame. El buffer
  del widget nace en cero: `lv_waterfall_set_size()` y
  `lv_waterfall_clear_data()` hacen `memset(dsc->data, 0, ...)`
  (`código: src/widgets/lv_waterfall.c:69-70` y `:81-88`; medido: 97 % de negro
  en la banda). Ese negro es **el mismo en los seis temas** y no depende de la
  paleta: es el buffer recién asignado.
- Cuando el waterfall corre, cada fila se pinta con `palette[id]`, con
  `id = v*255` y `v = (d - min)/(max - min)` (`código:
  src/widgets/lv_waterfall.c:118-121`): el valor mínimo cae en `palette[0]`. Con
  `waterfall_steps = 0`, `make_palette()` deja `palette[0] = waterfall[0]`
  (`código: src/theme.c:441-454`). O sea: el piso del producto **sin señal** es
  el extremo bajo de la paleta del tema, no negro.
- **Cross-check medido**: el piso (percentil 10) de los seis fixtures del arnés
  cae en el extremo bajo de cada tema — Sunlit `#f1f7fb` (con
  `waterfall[0] = #FFFFFF`), Redshift `#0a0203` (`#000000`), Aurora `#23193e`
  (`#140D22`), Cathode `#150c06` (`#090605`), Sprint `#0a182d` (`#030A17`), Trail
  `#142618` (`#0A150D`)
  ([`medicion-variante-panel-border.txt`](medicion-variante-panel-border.txt)).
  Y la captura de la radio real (Redshift, `07-restaurado.png`) tiene piso
  `#090202` con `waterfall[0] = #000000`.

### Tabla 4 — la línea sobre el piso real (derivada de los temas del repo)

`derivar-piso.py` lee los seis temas del repo (el JSON es la fuente, no hay
colores escritos a mano), toma `waterfall[0]` como piso y compone la tinta del
**trunk** —`panel_border` al 60 % con `LV_BLEND_MODE_ADDITIVE`, que LVGL arma
como `piso + (canal * 153) >> 8` por canal, saturado a 255— contra la tinta del
**fix** —`text` a opacidad plena—:

| Tema | `waterfall[0]` (piso) | antes (trunk): `panel_border` × 153/256 + piso | ratio antes | después: `text` a opacidad plena | ratio después |
| --- | --- | --- | --- | --- | --- |
| Aurora | `#140d22` | `#665686` | 2.92:1 | `#f8eeff` | **16.81:1** |
| Cathode | `#090605` | `#59391f` | 1.95:1 | `#f9d9aa` | **14.94:1** |
| Redshift | `#000000` | `#5c352f` | 2.01:1 | `#ffb3a0` | **12.21:1** |
| Sprint | `#030a17` | `#2f4b70` | 2.23:1 | `#f5f7f9` | **18.45:1** |
| Sunlit | `#ffffff` | `#ffffff` — **se satura a blanco** | **1.00:1** | `#162436` | **15.67:1** |
| Trail | `#0a150d` | `#425d3f` | 2.55:1 | `#f2eed8` | **15.98:1** |

Salida real: [`medicion-piso-derivado.txt`](medicion-piso-derivado.txt). Es
contraste directo WCAG 1.4.11 entre tintas planas (no la convención de
percentiles del gate: esa es la Tabla 1, sobre píxeles de renders).

La composición queda **cruzada contra lo medido**, no sólo derivada: la tinta
que la fórmula da para Sunlit sobre negro, `#0b2942`, es el `RGB(11,41,66)` que
mide el banco con el trunk (hallazgo 54), y la de Redshift sobre el piso del
tema, `#5c352f`, es el `#5b352f` de la captura de la radio (el color de tinta
más repetido, 45 filas; 1/256 de diferencia en rojo).

Las dos convenciones coinciden en lo que importa: **en el trunk, en el tema
claro, la línea no existía a la vista** — sobre el piso blanco de Sunlit el
60 % aditivo del borde del panel satura a `#ffffff`, es decir el mismo color que
el fondo. Eso, más el 60 % + `ADDITIVE` de los cinco temas oscuros
(1.95:1..2.92:1), es lo que el owner no veía. La fila Redshift de la derivada
(12.21:1) coincide con la de la columna Redshift de la Tabla 3: el piso del
banco y el `waterfall[0]` de Redshift son los dos `#000000`.

### Antes y después, a ojo (crops ×4)

Rectángulo `x=360..440, y=236..480` (la columna central con contexto), ×4 = 320
× 976, con `recortes.py`:

| Crop | Qué es |
| --- | --- |
| `banco/linea-banco-aurora-x4.png` | Aurora, banco, **después** (3 px, `#f8eeff`) |
| `banco/linea-banco-sunlit-x4.png` | Sunlit, banco, **después** (3 px, `#162436`) |
| `banco/linea-banco-redshift-x4.png` | Redshift, banco, **después** (3 px, `#ffb3a0`) |
| `banco/linea-banco-sunlit-antes-x4.png` | Sunlit, banco, **antes** (trunk `2a0be37fdb29`, `#0b2942` al 60 %) |
| `banco/linea-radio-redshift-antes-x4.png` | Redshift, **radio real**, **antes** (1 px, `#5b352f`..`#733b36`) |

El "antes" del banco sale de `../2026-10-18-wf140-banco/red-trunk.png` (trunk
`2a0be37fdb29`, Sunlit, con su control `param-off.png`) y el de la radio de
`../2026-10-17-qa-radio-swr125/07-restaurado.png`. En los tres crops del banco
la línea se ve de punta a punta; en el de la radio se ve, pero apagada, que es
el síntoma reportado.

### md5 de las capturas y los logs del banco

```text
8249804c107db7a68136ec8110104d13  banco/aurora-on.png
7b1898c6f7577d93cd72141468c98afd  banco/aurora-off.png
62de32fbebe5ed16362acd06f5ee5788  banco/sunlit-on.png
d80b7de3cbade5cf91471468e7638bde  banco/sunlit-off.png
3fba04be911f6624b78fa33a4d34ade8  banco/redshift-on.png
516cc2e74e178d4096ca3c74b3ef9106  banco/redshift-off.png
5aeb9e8c6a15114657f151ef617cf97e  banco/aurora-on.log
87fcfabfe162a23b330a5a8f087e6eec  banco/aurora-off.log
c78f3e18809a3ad1f5be7542fc48adcd  banco/sunlit-on.log
326fbbda3c2ce1f82d8c2164be09ad88  banco/sunlit-off.log
730bfad97d739b9569aee03d67d9ba58  banco/redshift-on.log
b45d610da1aef07e1cc188ac36f5c7d7  banco/redshift-off.log
```

Los `on` y `off` **no** son byte-idénticos entre sí: la captura incluye el reloj
de la barra de estado, que cambia de minuto entre corridas. La comparación que
vale es la del control por columna (0 px de tinta esperada), no el md5.

### Matriz de gates

`scripts/apps/x6200/check-matrix.sh 9872b614 HEAD <worktree>` — **exit 0**:
tests nativos, los 8 chequeos estáticos, `check_wcag`, ruff/format/pytest del
framework A/B y el gate de determinismo del arnés, todo `ok`
([`matrix.log`](matrix.log)). El bloque del banco QEMU reporta **BLOCKED** (no
FAILED): el preflight de `tools/captura-banco.sh --check` busca el chroot en
`<worktree>/../.vfix3` y falla **antes de tocar nada** (`FALTA: falta el chroot`,
`3 cosa(s) faltan`). El banco sí se corrió en esta corrida, con un chroot privado
(`CHROOT=/tmp/opencode/vfix3-wf140`) y un volumen privado, y la evidencia es la
de arriba (`banco/`): mismo binario, gate #86 `3/3 marcas`, 6 capturas.

Encima corren los gates del arnés A/B que el `check-matrix` no incluye (los corre
el CI), más `ty` (que también corre el CI y no la matriz). La salida real está en
[`gates-ab.log`](gates-ab.log), generada sobre esta revisión de código y
agregada en el commit siguiente de la rama (que sólo toca `docs/evidencia/**`).
Resumen:

| Gate | Resultado |
| --- | --- |
| `x6200-ab themes` | PASS — 19 objetos no-texto, 0 fallas (`waterfall_center_line` 14.58:1 en Aurora y 14.51:1 en Sunlit) |
| `x6200-ab contracts --revision HEAD` | PASS — 873 checks, 0 fallas, 0 defectos de baseline |
| `x6200-ab audit --revision HEAD` | 0 reproducciones, 0 hallazgos estructurales (9462 objetos, 85 escenarios) |
| `x6200-ab compare --a 9872b614 --b HEAD` | **PASS** — Functional / Visual Regression / Quality Contract / Performance en PASS; Hardware y Energy BLOCKED (advisory: no hay equipo) |
| `ty check` (todos los `.py` de `src`/`tests`) | All checks passed! |

El **compare** A vs B es el que cubre la superficie compartida: 85 escenarios
capturados en las dos revisiones, **sin regresión visual no explicada** — o sea
el cambio de render queda dentro de lo que declara el contrato `AB-WF140` de
`quality/contracts.yaml`.

## Archivos

| Archivo | Qué es |
| --- | --- |
| `renders.sh` | build + renders del harness por pasada (antes / después / variantes) |
| `medir-linea.py` | la medición (importa el medidor del gate, no lo reimplementa) |
| `medicion.txt` | salida real de `medir-linea.py` para antes/después |
| `medicion-variante-panel-border.txt`, `medicion-variante-additive.txt` | salida real de las variantes |
| `assert-panel-border.log` | el binario del harness rechazando la variante `panel_border` |
| `renders.md5` | md5 de los 28 renders usados (dos pasadas) |
| `linea-{aurora,sunlit}-{antes,despues}.png` | crop ×4 de la columna central (la prueba visual) |
| `captura-banco.sh` | captura una pantalla del banco con un binario exacto, tema y estado de la línea (esperas hasta-condición) |
| `medir-banco.py` | la línea en una captura del banco o de la radio, por caso, con o sin control `off` |
| `medicion-banco.txt` | salida real de `medir-banco.py` (Tabla 3: banco + radio) |
| `medicion-banco-radio-antes.txt` | `medir-linea.py` sobre la captura de la radio (geometría: 217/217 filas) |
| `derivar-piso.py` | el piso real por tema (`waterfall[0]`) y la tinta antes/después sobre él |
| `medicion-piso-derivado.txt` | salida real de `derivar-piso.py` (Tabla 4) |
| `recortes.py` | crop ×4 de la columna central de una captura |
| `banco/` | las 6 capturas del banco (3 temas × on/off) con sus 6 logs, `docker-ps-antes.txt` y los crops ×4 |
| `matrix.log` | salida real de `scripts/apps/x6200/check-matrix.sh 9872b614 HEAD <worktree>` |
| `gates-ab.log` | salida real de `themes`, `contracts`, `audit`, `compare` y `ty check` |

## Límites de esta evidencia

* **No hay outline/halo.** La orden es "que se vea la línea"; agregar un borde o
  un halo alrededor del trazo es producto distinto y no se hizo. El número que
  queda flojo es la "peor fila" de Aurora/Sprint/Trail (2.69:1..2.89:1), que es
  la línea justo sobre el carrier más brillante del fixture — no sobre el piso
  del waterfall, donde los tres temas miden 13.63:1..16.55:1.
* **Los fixtures son sintéticos**: el waterfall lo llena `signal_gen.c` con un
  carrier plantado en el centro, y esa es la "peor fila" de la Tabla 1. La
  captura de la radio real está en `banco/linea-radio-redshift-antes-x4.png`
  (Redshift, 1 px, `#5b352f`..`#733b36`, ~1.98:1 local, 2.27:1 en el par del
  gate): es el "antes" del equipo, medido.
* **El piso del banco es negro y el del producto no** (ver arriba): en el banco
  el waterfall no recibe ningún frame y el buffer queda en cero. Medir el
  contraste de Sunlit *en el banco* da 1.34:1 y en el piso real del tema da
  15.67:1. El número del banco no se puede usar como el contraste del producto;
  por eso la Tabla 4 deriva el piso de los temas del repo.
* **Una sola captura de la radio, y de otra corrida**: la radio no estuvo
  accesible en esta corrida; `07-restaurado.png` es evidencia del `2026-10-17` y
  no se puede repetir acá. Se usa como el "antes" medido, no como verificación
  del "después" en el equipo.
* **La barra de botones** (≤ 50 px de alto) no se mide: ahí la línea pasa por
  debajo de los botones y no es una marca sobre el waterfall.
