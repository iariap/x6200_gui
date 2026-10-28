# Guía de prueba en el equipo (X6200)

Checklist para probar la ROM en la radio. Está ordenada para que lo primero sea lo visual y rápido,
y al final lo que transmite. Cada punto dice **cómo se llega**, **qué debería pasar** y **qué mirar**
si algo falla.

Todo lo que se puede probar **sin** la radio ya está probado y está en
[`INTEGRATION_AUDIT.md`](INTEGRATION_AUDIT.md) §3; lo que queda acá es lo que sólo se valida con el
equipo, que es el §3 de ese documento.

---

## 0. Antes de empezar

- **Imagen**: la del release en borrador (`x6200_20261012_013612.img`, sha256 `8d9868da…`).
  Verificá con el `SHA256SUMS` que subiste.
- **Dos caminos** (detalle en el [README](../README.md)):
  - **Actualización en el lugar**: copiá `x6200_r1cbu_v0.1.2-lu2dsx.tgz` a `/mnt` y reiniciá. No
    toca tus datos (configuración, logs, temas, grabaciones, mensajes de voz quedan).
  - **Imagen completa** en la microSD. ⚠️ **Respaldá `/mnt` antes** (grabar la imagen borra la
    partición DATA).
- **Volver atrás**: la release anterior (`lu2dsx-2026-10-11`) sigue publicada; regrabá esa imagen.

### Cómo reportar algo

1. **Qué feature y en qué pantalla**, y si se colgó o volvió solo.
2. **El log de la GUI**: el daemon guarda uno por arranque en
   `/mnt/app_logs/x6200_log_<fecha>.txt` (y el actual en `/tmp/x6200_log.txt`).
3. Si es Bluetooth: `dmesg | tail -30` y `/tmp/btcat.log`.
4. La app **`APP Status`** (Ajustes → acciones de pulsación larga) muestra de un vistazo:
   adaptador Bluetooth y **su transporte** (USB/UART), nivel de TX, espacio en `/mnt`, wfview, la
   dirección de la web y el estado del aviso de versión. Una foto de esa pantalla ya responde
   varias de mis dudas.

> Las apps se abren desde **Ajustes → acciones de pulsación larga**: elegís la acción (`APP WSPR`,
> `APP JS8`, `Theme`, …) y la asignás a un botón; después mantenés ese botón.

---

## 1. Rápido y visual

| Qué | Cómo | Qué mirar |
|---|---|---|
| **Temas** | acción `Theme`, mantenerla | El aviso dice *restarting* y la GUI **se reinicia sola** con el tema nuevo. Debería cambiar **toda** la UI: barra, botones, espectro, waterfall, S-meter y los diálogos de apps |
| **Aguja de S-meter** | Ajustes → `S-meter needle` | El dial analógico: que quede bien encuadrado, que las marcas y el valor en dB se lean, y que se mueva con la señal |
| **Band plan IARU** | Ajustes → `Band plan` | La franja pegada al borde inferior del espectro, alineada con la fila de frecuencia y moviéndose con el dial |
| **Columna de spots** | Ajustes → `Cluster column` (necesita internet y el cluster) | La columna a la derecha sobre el waterfall, con frecuencia/indicativo/modo/antigüedad. **Tocar una fila sintoniza ese spot** |
| **Fuente** | Ajustes → `Font` | Aplica **en el próximo arranque** (dice "from the next start"), como en el X6100 |
| **Grabación** | acción `Screen rec` | Graba en `/mnt/screenrec/*.avi`; el archivo debería reproducirse (video + audio de RX) |
| **Aviso de versión** | al arrancar, con red | Avisa si hay una release más nueva. Su estado se ve en `APP Status` |

---

## 1b. CB 27 MHz (issue #146) — dial por canal, 4 grids y TX con interlock

> **Advertencia legal (Argentina).** El SBC está en **26,960–27,410 MHz** y exige autorización
> por estación y equipo homologado/registrado (Res. SC 47/1982; CABFRA 2025 de ENACOM). El
> X6200 **no es un equipo CB homologado** y transmitir en 27 MHz con él queda fuera del SBC y
> fuera del servicio de aficionados. Los grids A, B y D no son SBC argentino. **F0 (RF real,
> potencia, armónicos y PTT del micrófono) la hace el owner con autorización y carga
> fantasma**; el módulo no declara el TX validado.

Lo que se prueba acá es el módulo CB: el dial por canal, la celda `CH nn`/`CH --` y las
protecciones de TX. Se llega con `UP`/`DOWN` **largos** hasta `CB A`…`CB D` (la app Bands no las
lista).

| # | Qué | Cómo | Qué mirar |
|---|---|---|---|
| 1 | RX en canal | Ir a 27,185 (o `{"cmd":"freq","val":27185000}`) en AM | Celda **`CH 19`**, audio, S-meter, filtro y squelch normales |
| 2 | Dial | Girar la perilla en 27,185 | **1 detent = 1 canal**, sin aceleración; en 1 y 40 no da la vuelta; voz `Channel 19` |
| 3 | Hueco / fuera de canal | Ir a 27,000 o 27,195 | Celda **`CH --`**; el primer giro engancha 4 / 20 según la dirección (no el más cercano) |
| 4 | Bandas | `UP`/`DOWN` largos A→B→C→D | Arriba dice `CB A`…`CB D`; **AM** en las cuatro; el modo no queda en USB |
| 5 | Entrada directa / CAT | Entrar `27.185` o mandar CAT a 27,185 | **`CH 19`**; en 27,195 **`CH --`** |
| 6 | Split | Entrar a CB con split prendido | El split **se apaga**; al salir vuelve como estaba |
| 7 | Tope de potencia | En CB, subir PWR desde el panel | No pasa de **`cb_pwr_max`** (4,0 W); mensaje + voz al intentar pasarlo; al salir vuelve el valor anterior |
| 8 | XIT | Intentar mover XIT en CB | Queda en 0; el **RIT** sí funciona |
| 9 | Interlock | Con `cb_tx_scope=0`, PTT de software en 26,515 / 27,185 / 28,074 | Bloqueado con mensaje y voz / permitido / permitido (10 m intacto) |
| 10 | PTT del micrófono | PTT físico en 27,185 y en 26,515 | **Documentar qué hace**: el MCU puede no respetar el interlock de software |
| 11 | Migración | Radio con `/mnt/params.db` viejo | Los 4 rangos quedan canónicos (`sqlite3`: 201 26,065–26,505 … 204 27,415–27,855) y las frecuencias guardadas válidas se conservan |
| 12 | Consumo | RX y TX en CB contra 10 m | Sin timers nuevos: el consumo no debe cambiar de forma medible |

**F0 (aparte, con autorización del owner):** TX en 27,185 con carga fantasma, potencia real,
armónicos y PTT del micrófono. Sin esa medición, el estado es **`requires-radio`**.

---

## 1c. CB 27 MHz (issue #169) — canal en la fila y barra CB en el plan de banda

Mejora de presentación (solo UI; sin TX). Se prueba que el canal se vea en grande en la fila de
frecuencia y que la franja del plan de banda se pinte de CB. **Sin transmisión.**

| # | Qué | Cómo | Qué mirar |
|---|---|---|---|
| 1 | Fila en CB C | Ir a 27,185 (AM) | Fila **`27.185` · `CH 19` · `CB C`**; barra **violeta `CB`** arriba; **sin** «8» fantasma |
| 2 | Dial | Girar la perilla en 27,185 | El centro cambia `CH 20`, `CH 21`…; la barra sigue; la voz dice `Channel N` (sin cambios) |
| 3 | Grids A/B/D | Ir a 26,065 / 26,515 / 27,855 | A la derecha **`CB A` / `CB B` / `CB D`**; `CH 1` en el borde y `CH 40` en el otro; la barra se recorta al cruzar el borde |
| 4 | Off-channel | Ir a 27,000 | **`27.000` · `CH --` · `CB C`**; el primer giro engancha |
| 5 | Orden 23/24 | Pasar del 23 al 24 | **`CH 24`** y a la izquierda **`27.235`** |
| 6 | Temas | Aurora, Cathode, Redshift, Sprint, Sunlit, Trail | En Cathode/Sprint `CH 19` y `CB C` con la familia **normal** y **sin** «8» fantasma; en los seis la barra violeta `CB` |
| 7 | Ajuste Band plan | Apagar/prender *Display → Band plan* | Apagado: no hay barra (ni en CB ni en aficionados). Prendido: vuelve |
| 8 | Salir de CB | Ir a 40 m | Vuelve la frecuencia grande + bordes del span + fantasma (si el tema lo tiene); la franja IARU igual que antes |
| 9 | Celda VFO | En CB | Sigue mostrando **`CH nn`** (no se tocó `info.c`) |
| 10 | Consumo | RX en CB contra 10 m | Sin timers nuevos ni wakeups extra |

---

## 2. Apps nuevas (que no se cuelguen y que vuelvan bien)

Para cada una: abrir, usar lo básico, cerrar. Al cerrar, la pantalla debe volver al waterfall con
la **frecuencia y el modo restaurados** (eso se arregló en este port).

| App | Qué probar | Necesita |
|---|---|---|
| **WSPR** | Que decodifique señales y que la lista se llene; el % de TX y la subida a WSPRnet | Señal real, internet |
| **NavTex** | Que decodifique un mensaje y lo guarde | Señal NavTex |
| **SSTV** | Que decodifique una imagen (VIS automático) y que la galería la muestre | Señal SSTV |
| **WeFax** | Que decodifique un fax; alineado e inclinación con los **botones** `Align`/`Tilt`/`Fix` | Señal de fax |
| **JS8** | Que decodifique, el mapa de estaciones y el inbox | Señal JS8 |
| **Cluster** | Servidores, filtros, alertas; el orden por frecuencia/hora | Internet |
| **Field Ops** | Referencias, log, RBN; **`Rec CQ`** graba y **`CQ`** lo emite en SSB (página 2) | Internet; TX |
| **CW Keyboard** | Escribir y que la radio telegrafíe; los mensajes CW guardados | Un **teclado** vinculado |
| **CW Trainer** | Que emita grupos de prefijos y que el texto aparezca decodificado; `Replay` | — |
| **Bluetooth** | Ver la lista, `Scan`, tocar una fila para vincular/conectar; el **passkey** de un teclado | Adaptador BT |
| **Status** | Que muestre los datos (es lo que necesito de tu lado) | — |

---

## 3. Radios y red

- **wfview**: Ajustes → interruptor; conectá desde la PC (credenciales en `/mnt/wfview.conf`,
  puerto **50001**). La radio debería aparecer como un IC-705.
- **Web configurator**: `http://<ip>/` (15 páginas) y `https://<ip>/` (certificado autofirmado).
  **Lo que más me interesa: la página `/remote`**, que ahora debería mostrar la radio **en vivo**
  (banda, modo, frecuencia, espectro) y permitir mandar comandos.
- **Bluetooth CAT**: vincular el teléfono y elegir el SPP de la radio (para FT8CN, por ejemplo).
  `APP Status` dice qué transporte usa el adaptador.
- **Auriculares BT**: en la app de Bluetooth, vincular y conectar; después `RX to HP` y probar el
  audio (y `Speaker` para el silencio del parlante).
- **Teclado BT**: que escriba en el diálogo de mensajes o en CW Keyboard y que no se pierdan
  teclas al escribir rápido (*rollover*).

---

## 4. Transmisión (con cuidado)

Empezá con **potencia baja**. El nivel de excitación de los modos digitales quedó en un valor
conservador porque el X6200 no expone la revisión de su base; `APP Status` muestra ese valor.

- **Modos digitales**: WSPR (TX), SSTV (TX de una imagen), JS8, FT8. Mirá que la potencia de salida
  sea la esperada y que no haya ALC raro.
- **CQ de voz** (Field Ops): grabá el CQ y emitilo en SSB; escuchá el audio emitido con otra radio
  o un SDR.
- **CW Keyboard / Trainer**: que la llave cierre y que la velocidad sea la configurada.
- **Grabación de pantalla**: que el audio de RX quede en el AVI.

---

## 5. Opcionales

- **DRM/KMS** (motor de video): creá `/mnt/use_drm` y reiniciá. El log dice `Display: DRM/KMS` o
  `Display: fbdev`. Si la pantalla queda negra, **borrá `/mnt/use_drm`** (por SSH) y reiniciá: no
  hace falta regrabar nada. Ver
  [`NEW_FEATURES.md`](NEW_FEATURES.md) §32.
- **Actualización en el lugar**: es el camino que va a usar la gente que ya tiene la ROM; probarlo
  una vez confirma que `S02update` hace lo que esperamos.
- **Cambio de tema**: además de lo visual, que la GUI **vuelva sola** (la reinicia `x6200_daemon`)
  y que la configuración quede guardada.

---

## 6. Lo que ya está verificado sin radio

Por si algo te resulta raro: el arnés nativo (469 comprobaciones), WSPR (158), SSTV (7 modos),
WeFax (300/300 líneas), NavTex, **la suite completa de JS8 del X6100 (1994 aserciones en 88
casos)** y las 19 páginas del web configurator corren y pasan en verde
([`TESTING.md`](TESTING.md)). El inventario de lo que lleva la imagen está en
[`IMAGE_INVENTORY.md`](IMAGE_INVENTORY.md).

---

## 7. Actualizar sin regrabar (recomendado para probar arreglos)

La ROM sabe actualizarse sola: `S02update` corre **al principio del arranque** (S02), antes de
que se lance la GUI, y aplica cualquier `x6200_r1cbu_*.tgz` que encuentre en la partición DATA
(`/mnt`).

1. Sacá la tarjeta de la radio y ponela en la PC.
2. Abrí la partición **DATA** y copiá `x6200_r1cbu_v0.1.2-lu2dsx.tgz` a su raíz.
3. Ponele la tarjeta a la radio y encendé.

En ese arranque el firmware hace un backup de lo que reemplaza en `/mnt/backup_*.tgz` y
descomprime el tgz sobre `/`. Como corre **antes** de la GUI, sirve incluso si la GUI de la
versión anterior no arrancaba: el arreglo queda aplicado y **la GUI siguiente ya es la nueva**.

Qué conserva y qué no:

- **Conserva** `/mnt` completo: configuración, memorias, logs, temas, grabaciones y mensajes de
  voz.
- **No** toca el kernel ni u-boot (van en la partición BOOT); los módulos del kernel sí.
- Deja **~150 MB** libres en DATA para el backup.
- El tgz trae además un `S02update` corregido, así que a partir de esa actualización el backup
  queda completo y legible.

Para volver atrás: la imagen de la release anterior, por la vía de la imagen completa.
