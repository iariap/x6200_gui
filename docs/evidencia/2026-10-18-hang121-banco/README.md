# Evidencia — #121: la radio se cuelga entera (última respiración persistente)

**Fecha**: 2026-10-18 · Rama `factory/hang-121` · base `07d73541` (trunk)
**HEAD de código**: `befcf155` — *Hang #121: ultima respiracion persistente y aviso de bateria baja*
**Binario ARM de la evidencia**: `md5 2a0be37fdb293383b605316a9881c932` (13.773.024 B, ELF32 ARM)
**Radio**: inalcanzable durante toda la corrida (sin red ni SSH). **No hubo prueba en hardware**:
todo lo de abajo es nativo (host) + banco QEMU (`cortex-a7`) con el binario real de la imagen.

## Qué se implementó

### A. Registro persistente de “última respiración” (`src/lastbreath.c/h`)

Cada **10 s** (configurable) la GUI escribe de forma **atómica** (`<path>.tmp` + `fdatasync` +
`rename`) una línea de ~180 B en `/mnt/lastbreath` con:

| campo | significado |
|---|---|
| `up_s`, `up_ms` | uptime de `/proc/uptime` |
| `clean=0/1` | 1 sólo en la salida limpia (atexit / apagado por software) |
| `boot=` | primeros 8 hex de `/proc/sys/kernel/random/boot_id` |
| `radio=`, `tx=` | `RADIO_RX/TX/ATU_*/SWRSCAN/POWEROFF/OFF` y TX sí/no |
| `app=` | app abierta (`home`, `SWRScan`, `FT8`, …) |
| `bat_mv=`, `ext_mv=`, `cap=`, `chg=` | voltaje de batería/fuente, capacidad y carga; `bat=na` si el flow todavía no publicó la muestra |
| `ev=pending/peak/dropped` | cola de eventos (#111) |
| `last=`, `msg=` | último tipo de evento procesado y último mensaje mostrado por la GUI |
| `wus=` | duración **auto-medida** del write anterior, en µs |

Al arrancar, `lastbreath_init_from_env()` lee la anterior **antes** de pisarla. Si existe y no
tiene `clean=1`, deja **una línea** en el log de arranque:

```
LASTBREATH lastbreath v=1 up_s=487256 up_ms=487256330 clean=0 boot=12ad7621 radio=RX tx=0
           app=home bat=na ev=0/4/0 last=msg-update wus=2711 msg=X6200 de R1CBU v0.1.2-lu2dsx
```

(En el log real es una sola línea; acá se parte para el README.)

Detalles de conservación:

- **sin heap en el camino de escritura**: la línea se formatea en un buffer de pila y se escribe
  con `open/write/fdatasync/close/rename` (no hay `malloc` en `lastbreath_format()` ni en
  `lastbreath_write_atomic()`);
- **sin SD / sin `/mnt` degrada**: si el path no existe, `lastbreath_write_atomic()` devuelve -1,
  se avisa **una vez** (`LASTBREATH sin escritura…`) y la GUI sigue; no crea directorios;
- **configurable sin tocar params**: `X6200_LASTBREATH_SECS` (1..3600, default 10) y
  `X6200_LASTBREATH_PATH` (default `/mnt/lastbreath`), útiles para el banco y para bajar la
  frecuencia si el owner prefiere menos desgaste;
- **salida limpia**: `atexit()` y `radio_poweroff()` llaman a `lastbreath_mark_clean_now()`, que
  escribe `clean=1` y apaga el latido para que un tick tardío no lo vuelva a `clean=0`. Un
  SIGKILL/SIGSEGV/corte de energía **no** pasa por ahí: la próxima corrida lo reporta.

Integración (sin tocar RF/DSP): `src/main.c` (init, tick en el loop, atexit), `src/clock.c/h`
(snapshot del pack), `src/events.c/h` (último evento procesado), `src/msg.c/h` (último mensaje),
`src/main_screen.c/h` (app abierta), `src/radio.c` (apagado limpio). `src/CMakeLists.txt`.

### B. Aviso de batería baja (sólo visible)

**Lo que ya existía** (grep `battery|volt|batt|pwr|vbat`):

- `radio.c` ya publica `pack->vbat`, `pack->vext`, `pack->batcap` y `flag.charging` a
  `clock_update_power()`; el reloj ya muestra el voltaje y el símbolo de batería.
- `visual_policy.c/h` ya tiene **umbrales de pack** con hysteresis: `VP_BALANCED_ENTER_MV 12300`,
  `VP_ECO_ENTER_MV 11400`, `VP_CRITICAL_ENTER_MV 10800`, `VP_CRITICAL_EXIT_MV 11300`. No había
  **ningún aviso textual** de batería baja (sólo el perfil visual y la voz “Battery voltage”).

**Lo agregado**: `visual_policy_low_battery_warn()` usa **esos umbrales existentes** (entra en
10.8 V, sale en 11.3 V) y `radio.c` muestra un mensaje (`Battery low: X.X V`) + una línea
`BATTERY-LOW` en el log en la transición. **No bloquea TX, no apaga nada, no toca RF/DSP ni el
presupuesto de TX.** Con `charging` o lectura inválida no avisa. El umbral es el de la política,
no un número nuevo; **la calibración contra el pack real del owner queda pendiente en radio**
(el issue midió 8.6 V, que cae dentro del aviso).

## Costo medido

- Línea de ejemplo con batería: **180 B**; período 10 s ⇒ **6 escrituras/min = 1.080 B/min**
  (~1,5 MB/día). En el banco (sin radio, `bat=na`) la línea fue de **162 B** ⇒ 972 B/min.
- Host x86-64 (ext4, `fdatasync`): **0,32 µs** por formateo y **19,1 µs** por escritura atómica
  (200 ops) — `costo-host.txt`.
- Banco QEMU `cortex-a7` (tmpfs/ext4 del chroot): `wus=` medido por el propio registro:
  **1.021–3.197 µs** por escritura (incluye `fdatasync`), o sea ~0,01–0,03 % de CPU con período
  de 10 s. En la SD de la radio el `fdatasync` puede costar más; por eso el período es
  configurable.

## TDD nativo (RED → GREEN)

- **RED** (`tdd-red.log`): `tests/native/run_native_tests.sh` →
  `cc1: fatal error: .../src/lastbreath.c: No such file or directory` y
  `lastbreath.h: No such file or directory`.
- **GREEN** (`tdd-green.log`): `tests/native/test_lastbreath.c` fija formato determinista exacto,
  `bat=na`, sanitización de mensajes con `\n`, buffer corto, **rename atómico** (no queda `.tmp`),
  detección `clean=0` / `clean=1` / sin marca, línea `LASTBREATH`, `mark_clean` y degradación sin
  filesystem/path largo. **48 comprobaciones, 0 fallos**.
- El bloque de batería se agregó a `tests/native/test_visual_policy.c` (entrada/salida con
  hysteresis, carga e inválidos): **461 comprobaciones, 0 fallos** en ese binario.
- Suite nativa completa con `-fanalyzer`: **0 fallos** (ver `gates.txt`).

## Gates

Ver [`gates.txt`](gates.txt). Resumen real:

- nativos completos + `-fanalyzer`: OK; estáticos: **8/8** (config, actions, settings_focus,
  apps_menu, app_handlers, manual_apps, wcag, cw_tune_feed) + `audita-pruebas` + curva ROE;
- determinismo: `PASS: deterministic (identical PNGs for every fixture)`;
- **A/B `AB-HANG121` (A `07d73541`, B `befcf155`)**: **PASS**, 85 escenarios, **capturas
  pixel-idénticas**. Se declara esperado: el cambio no toca el render (sólo log/estado y un
  mensaje de batería que no aparece en los fixtures);
- contracts **873/873**, audit **0 hallazgos estructurales**, themes **OK**;
- ruff / format / ty / pytest **227** + e2e **13**;
- cross-compile ARM: `md5 2a0be37fdb29…`, guard del port **3/3 marcas**;
- banco QEMU: ver abajo.

## Banco QEMU (sin radio)

El banco no tiene radio: el flow nunca publica el pack, así que el registro muestra `bat=na`
(la degradación declarada). Para que `uptime` y `boot_id` existan se montó `/proc` en el chroot;
el resto es el binario ARM real con `libfakefb`/`libcrash`/`libx6200trace`, `QEMU_CPU=cortex-a7`.

1. Se borró `/mnt/lastbreath`, se arrancó la GUI y se esperó > 30 s.
   Archivo crudo: [`lastbreath-antes-del-corte.txt`](lastbreath-antes-del-corte.txt)

   ```
   lastbreath v=1 up_s=487256 up_ms=487256330 clean=0 boot=12ad7621 radio=RX tx=0 app=home
   bat=na ev=0/4/0 last=msg-update wus=2711 msg=X6200 de R1CBU v0.1.2-lu2dsx
   ```

2. Se mató el contenedor con `docker rm -f` (**SIGKILL**, sin `atexit`) y se arrancó de nuevo.
   El arranque reportó la anterior como `LASTBREATH`:
   [`out-banco-lastbreath.txt`](out-banco-lastbreath.txt) (línea 2)

   ```
   [Warn] (457665.554, +1) lastbreath_init_from_env: LASTBREATH lastbreath v=1 up_s=487256
   up_ms=487256330 clean=0 boot=12ad7621 radio=RX tx=0 app=home bat=na ev=0/4/0
   last=msg-update wus=2711 msg=X6200 de R1CBU v0.1.2-lu2dsx (in main.c line #270)
   ```

   (en el archivo es una sola línea; acá se parte para el README). El registro nuevo de esa
   corrida, con uptime avanzando y `wus` distinto, quedó en
   [`lastbreath-despues-del-reinicio.txt`](lastbreath-despues-del-reinicio.txt).

3. Captura de la GUI viva: [`banco-lastbreath.png`](banco-lastbreath.png) (20.093 B, 264 colores).
   Binario/qemu: [`banco-binario.txt`](banco-binario.txt), [`banco.log`](banco.log).

## Archivos de esta evidencia

| Archivo | Qué es |
|---|---|
| `tdd-red.log`, `tdd-green.log` | RED (sin `lastbreath.c`) → GREEN (48 comprobaciones) |
| `costo-host.txt` | formateo y escritura atómica medidos en host |
| `cross-build.log`, `cross-build-info.txt` | build ARM y md5/tamaño/guard |
| `ab.log`, `contracts.log`, `audit.log`, `themes.log` | gates A/B y calidad |
| `determinism.log`, `pytest.log`, `pytest-e2e.log`, `py-gates.log`, `estaticos.log` | resto de gates |
| `lastbreath-antes-del-corte.txt` | registro crudo antes del SIGKILL |
| `out-banco-lastbreath.txt` | log de arranque con la línea `LASTBREATH` |
| `lastbreath-despues-del-reinicio.txt` | registro de la corrida siguiente |
| `banco-lastbreath.png`, `banco.log`, `banco-binario.txt` | captura y datos del banco |
| `gates.txt` | salidas reales resumidas |

## Qué queda sin fundar

**La causa de las cuatro caídas.** Este trabajo **no** la establece y no inventa un fix. Sigue sin
evidencia del kernel (no hay `pstore`/ramoops ni consola serie), y la radio no estuvo accesible
para medir tensión, corriente, temperatura, PA ni el estado del watchdog. La hipótesis del
**undervoltage** (8,6 V medidos; ~2,15 V/celda si es 4S LiFePO4 o ~2,87 V/celda si es 3S Li-ion)
sigue siendo **no fundada**: este cambio sólo hará que la próxima muerte deje el estado de la GUI y
la tensión que había en ese momento.

Tampoco se funda si el equipo se recupera solo: el registro distingue reinicio de kernel
(`boot=` distinto) de reinicio sólo de la GUI (`boot=` igual) **en la próxima caída**, no hacia
atrás.

## Procedimiento exacto para el próximo triage (cuando la radio vuelva)

1. **Lo primero, sin reiniciar**: copiar el archivo crudo y el log vivo:

   ```sh
   ssh root@<radio> 'cat /mnt/lastbreath; echo ---; cat /proc/uptime; \
                     cut -c1-8 /proc/sys/kernel/random/boot_id; \
                     ls -l /mnt/lastbreath; tail -40 /tmp/x6200_log.txt'
   ```

   Si la radio no responde, leer `/mnt/lastbreath` por SD (partición DATA) antes de flashear nada.

2. Reportar, textualmente:
   - el contenido crudo de `/mnt/lastbreath` (`up_s`, `clean`, `boot`, `radio`, `tx`, `app`,
     `bat_mv`/`ext_mv`, `ev`, `last`, `msg`);
   - la línea `LASTBREATH …` de `/mnt/app_logs/boot.log` o de `/tmp/x6200_log.txt` (si la GUI
     volvió a arrancar);
   - `uptime` actual y `boot_id` actual (primeros 8) para comparar con `boot=` del registro;
   - si hubo que dar ciclo de energía, si la pantalla/LED quedaron encendidos, y la tensión
     **medida con tester** (no la del display) en el momento;
   - si aparece `BATTERY-LOW` en el log, con el voltaje.

3. Interpretación rápida:
   - `clean=0` + `boot=` **igual** al actual ⇒ la GUI murió y volvió a arrancar **sin reboot del
     kernel** (o el registro es de esta misma corrida antes de la muerte);
   - `clean=0` + `boot=` **distinto** ⇒ hubo reboot del kernel (watchdog, panic o ciclo de
     energía); el `up_s` es lo último que llegó a escribir la GUI;
   - `clean=1` ⇒ salida ordenada; no debería haber `LASTBREATH`;
   - `bat_mv` muy bajo (< 10.800 mV) ⇒ el aviso de batería baja ya debería estar en el log, y la
     hipótesis de alimentación gana fuerza (pero no se confirma sin medir con tester).

4. Si `/mnt/lastbreath` no existe: verificar que `/mnt` esté montado y escribible; el registro se
   degrada solo y no rompe la GUI (ver `LASTBREATH sin escritura…` en el log).

**Lo que este cambio no hace**: no habilita `pstore`/ramoops (sigue pendiente para tener el
kernel del boot anterior), no cambia RF/TX/DSP, no bloquea TX por batería baja y no apaga nada.
