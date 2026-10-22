# Hallazgos fundados e hipótesis — detalle

Este documento separa lo que el dossier **puede sostener** de lo que **no**. El owner exige
"si no se puede fundar no es un hallazgo": las hipótesis no se declaran defectos ni se usan para
recomendar un fix. **No se abrió ningún ticket**; cuando existe issue previo se enlaza.

## 1. Criterio de "fundado"

Un hallazgo entra en `findings.csv` como fundado si cumple:

1. **Síntoma observable** o camino de código determinista citado (archivo:línea).
2. **Repro** mínima, con precondiciones y capa (HOST/QEMU_USER/HARDWARE).
3. **Expected vs observed** contra un contrato o requisito vigente.
4. **Evidencia** en 01..04, con ruta y hash; no se copian logs.
5. **Impacto** para el operador/release y **certeza** declarada (`measured`, `code`, `measured+code`).

Si falta 2/3/4, va a **hipótesis** (`H-*`, `certainty=hypothesis`, `status=HYPOTHESIS`). Que un
PASS de CI o la ausencia de crash en QEMU no convierta una hipótesis en hecho.

**Prioridades:** P0 daño/TX insegura/pérdida de datos demostrada; P1 crash/bloqueo/tarea frecuente
inutilizable; P2 degradación medible/feedback erróneo; P3 consistencia/estética.

## 2. Hallazgos P1 (fundados)

### F-01 — Tarjeta nueva: DATA nace vfat y el primer boot no monta `/mnt`

- **Requisito:** DATA ext4 montada rw en `/mnt` desde el primer arranque (`docs/PRUEBAS.md:43`,
  fstab de la imagen y contrato de persistencia).
- **Evidencia:** [`02-rom §5.2`](../02-rom/README.md#52-lo-que-realmente-pasa-evidencia-ejecutada),
  [`logs/rom03/fstab-resolve.log`](../02-rom/logs/rom03/fstab-resolve.log) sha256 `abed2d62…`,
  [`mkfs-vfat.log`](../02-rom/logs/rom03/mkfs-vfat.log) sha256 `c7427c1c…`,
  [`target-branch.log`](../02-rom/logs/rom03/target-branch.log) sha256 `03e08585…`.
- **Repro:** imagen del candidato en SD nueva → boot 1 → `blkid -L DATA`, `findmnt /mnt`; boot 2 →
  repetir. En el análisis se usó una copia agrandada + `parted`/`mkfs.fat`/bash del target bajo
  QEMU (no se creó el dispositivo).
- **Expected:** p3 ext4, montada rw en el primer boot.
- **Observed:** `S01create_data` crea p3 `fat32` con `mkfs -t vfat`; `mount /dev/mmcblk0p3`
  resuelve `ext4` por fstab y falla; el primer boot escribe en la rootfs; el segundo boot monta
  vfat. El guard `grep fat32` es frágil: si boot fuera FAT32, sale sin crear DATA.
- **Impacto:** params/logs del primer boot no persisten; contrato DATA ext4 roto; update/backup
  sobre una base vfat. **Certeza:** measured+code. **Issue:** #188 (cerrado), Buildroot PR #1.
- **Límite:** no se ejecutó en hardware; el procedimiento externo de flasheo no está en el repo.

### F-02 — DB corrupta/read-only/faltante: SIGSEGV determinista y lazo de crash

- **Requisito:** arranque robusto con DB inválida (defaults/error visible), sin crash (#188).
- **Evidencia:** [`02-rom §8.2`](../02-rom/README.md#82-db-corrupta--read-only--faltante-fail-crash-determinista);
  logs `db-corrupt-gui.log` `d4b50571…`, `db-readonly-gui.log` `7aa14b63…`,
  `db-missing-gui.log` `388acd0a…`, `crashloop-summary.log` `163fbbbb…`.
- **Repro:** copia privada del rootfs; corromper/RO/borrar `params.db`; ejecutar `x6200_gui`; con
  DB corrupta ejecutar `x6200_daemon` 10 s.
- **Expected:** GUI arranca con defaults o error controlado; daemon con backoff.
- **Observed:** exit 139, PC `0x42e12c` idéntico en los 3 casos; wrapper real 34 inicios de GUI,
  19 CRASHLOG y 20 logs en 10 s. Camino: `params_init` → `cb_hooks_init` con `cfg_cur.band=NULL`.
- **Impacto:** boot-loop, pérdida de acceso a la UI, logs que crecen. **Certeza:** measured
  (QEMU/SYNTHETIC). **Issues:** #121, #188 (cerrados).
- **Límite:** no confirmado en la radio; DB llena BLOCKED.

### F-03 — Empaquetador de update y owner del overlay

- **Requisito:** paquete de update íntegro, script root:root, aserción de setuid correcta.
- **Evidencia:** [`02-rom §6`](../02-rom/README.md#6-rom-04--update-y-rollback);
  `make-update-container.log` `2c4e6c3d…`, `overlay-owner-target.log` `42fc8f31…`.
- **Repro:** `make_update_tgz.sh` como root → extraer con BusyBox tar del target → `stat`.
- **Expected:** exit 0; S02update 0:0 755.
- **Observed:** exit 1 `setuid: 3 (tienen que ser 5)`; S02update `1000:1000` (uid 1000 = `pulse`).
- **Impacto:** el release falla en su propia aserción; si se aplica, un script que corre como root
  queda propiedad de `pulse` (integridad/seguridad). **Certeza:** measured. **Issue:** ninguno.

### F-04 — Update sin integridad/rollback y backup incompleto en la primera update

- **Requisito:** update atómico/recuperable, checksum, backup completo, no borrar el tgz si falla.
- **Evidencia:** [`02-rom §6`](../02-rom/README.md#6-rom-04--update-y-rollback);
  `first-update-exec.log` `db2afe6b…`, `backup-first-update.log` `80fac748…`.
- **Repro:** target con rootfs de la imagen → `S02update` stock con tgz en `/mnt` → contar backup.
- **Expected:** 5466/5466, checksum, rollback.
- **Observed:** 706/5466 legibles (xargs BusyBox); sin checksum/firma; `rm` del tgz aunque tar
  falle; backup fallido no detiene; sin A/B/journal.
- **Impacto:** corte de energía/tgz corrupto deja rootfs a medias y sin backup completo.
  **Certeza:** measured. **Issue:** #188 (contexto), sin ticket específico de update.

### F-05 — Sin watchdog y sin backoff

- **Requisito:** supervisión con backoff/watchdog; no amplificar fallos (#121).
- **Evidencia:** [`02-rom §9`](../02-rom/README.md#9-hallazgos-sin-abrir-tickets);
  `rootfs/usr/sbin/x6200_daemon:14-16`; `crashloop-summary.log` `163fbbbb…`.
- **Repro:** ejecutar el wrapper 10 s con DB corrupta.
- **Expected:** espera/backoff, no 3,4 vueltas/s.
- **Observed:** 0 servicios watchdog; `while true` sin espera; 34 inicios/19 CRASHLOG en 10 s.
- **Impacto:** amplifica F-02; boot-loop y logs crecientes. **Certeza:** measured+code.
  **Issue:** #121.

## 3. Hallazgos P2/P3 (fundados)

- **F-06** timers WiFi 30 ms incondicional (41,8 wakeups/s; `src/wifi.cpp:84,371-373`) — código;
  impacto energético **no** medido (H-04). Evidencia: [04-performance §8](../04-performance/README.md#8-perf-06--perfil-estatico-de-costes-y-riesgos-host-codigo).
- **F-07** CW decoder sin señal ~9 % core en QEMU (3 ventanas; label con markup cada 100 ms) —
  medido QEMU; no Cortex-A7. Evidencia: [04-performance §9](../04-performance/README.md#9-hallazgos-principales-sin-abrir-tickets),
  `resources-summary.csv` `0923b4df…`.
- **F-08** `radio_thread` ~100 %/630k read/s sin BASE — medido QEMU; hardware = H-03.
  Evidencia: [04-performance §9](../04-performance/README.md#9-hallazgos-principales-sin-abrir-tickets),
  `threads.txt` `408564bb…`.
- **F-09** 22 diálogos inertes en harness — hueco de cobertura, no defecto de firmware.
  Evidencia: [03-ui-ux §2](../03-ui-ux/README.md#2-dependencia-no-satisfecha--196-blocked-explicito).
- **F-10** mensajes ES/EN mezclados — REVIEW editorial, no bug.
  Evidencia: [03-ui-ux §8](../03-ui-ux/README.md#8-ux-03--apps-errores-y-alcance-real).
- **F-11** defconfig congelado sin LIBTOOL/LIBSNDFILE/PULSEAUDIO — **resuelto en el repo** por
  Buildroot PR #2 (`f23d14f`, 2026-10-21T21:51:25Z); el artefacto medido se construyó con la
  desviación local declarada y no se reconstruyó. Evidencia: [02-rom §1.2](../02-rom/README.md#12-reproducibilidad-del-par-congelado-defconfig),
  [01-baseline §8](../01-baseline/README.md#8-paso-6--artefactos-armrom-en-sandbox-privado).
- **F-12** migración 2 no idempotente si se pierde `version` (multiplica `atu.freq`) — código.
- **F-13** sin corrida CI del SHA `c888308e` (solo PR #193 head) — medido.
- **F-14** documentación con conteos inconsistentes/About/DFL — medido.
- **F-15** `/bin/sh`→bash; ash falla S01/S04 — medido+code.
- **F-16** render/A-A INCONCLUSIVE por ruido del host — medido.
- **F-17** webserver/CTest/barridos/suites omitidos — BLOCKED con causa.
- **F-18** imagen sin p3; DATA depende de procedimiento externo no versionado — código.

## 4. Hipótesis (NO son hallazgos)

| ID | Hipótesis | Por qué no se funda | Qué la convertiría en hallazgo |
|---|---|---|---|
| H-01 | El crash de DB ocurre igual en la radio | Solo QEMU + fixture | HW-06 con copia de DATA en medio de prueba |
| H-02 | Sin watchdog, un crash deja boot-loop en hardware | El lazo se midió en QEMU, no en radio | HW-06/HW-01 con crash inducido |
| H-03 | `radio_thread` al 100 % en hardware si falla la BASE | QEMU es modo degradado | HW-08 con BASE desconectada autorizada |
| H-04 | 41,8 wakeups/s impactan la batería | No hay medidor | HW-05 con V/I/P calibrado |
| H-05 | El flasheo del candidato puede dejar p3 corta | El caso es del tip 50bf8ab0 | HW-06/flash en SD de prueba |
| H-06 | Auto Tune puede hacer QSY con FFT no confiable | No hay FFT/radio | HW-03 con señal débil/QRM |
| H-07 | La mezcla ES/EN induce error de operador | No hay estudio de operador | HW-02 con usuarios |
| H-08 | El tip #206 cambia boot/consumo | No se midió | HW-01 con el tip |
| H-09 | Mixer/audio analógico falla en hardware | QEMU no pudo preparar mixer | HW-03 con loopback/analizador |
| H-10 | Datos del primer boot se pierden al actualizar | Mecanismo F-01, no medido | HW-06: boot 1, update, verificar |

## 5. Lo que el dossier NO afirma

- No afirma que el candidato esté aprobado para release.
- No afirma que los FAIL de QEMU/host ocurran en la radio (son fundados en su capa; H-01/H-02).
- No convierte un PASS de gate/CI en ausencia de defectos.
- No abre tickets ni propone fixes; solo enlaza issues existentes y deja candidatos documentados.
