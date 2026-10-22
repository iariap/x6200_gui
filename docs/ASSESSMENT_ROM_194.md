# Assessment pre-release ROM X6200 — documento de síntesis (#194 / #199)

> **Ubicación:** este es el documento de síntesis de la campaña #194 (ruta pedida por #199: `docs/ASSESSMENT_ROM_194.md`).
> La evidencia completa vive en [`evidencia/assessment-rom-194/`](evidencia/assessment-rom-194/README.md).

## 1. Alcance y SHAs

- **Campaña:** `iariap/x6200_gui#194`, evidencia pre-release de la ROM completa, ejecución
  secuencial #195 → #196 → #197 → #198 → #199.
- **Candidato:** GUI `c888308eede71205605c06383432ee7e165f202e` (rama `lu2dsx/x6200-rom`,
  merge del PR #193), Buildroot `d084e2870a00b64d9c57474e7eceeef80f43f25b`
  (`iariap/AetherX6200Buildroot`), LVGL `74d0a816440eea53e030c4f1af842a94f7ce3d3` (v8.3.11).
- **Artefacto:** `sdcard.img` sha256 `d0ff4435…` (571.473.920 B), `rootfs.tar` `7c26c76b…`
  (242.790.400 B), `rootfs.ext4` `9365b52f…`, `boot.vfat` `914f07eb…`; GUI en rootfs
  13.407.624 B, md5 `1f998c46…`, sha256 `7e2e9331…`; `fuente-md5` `964193ae…`; gate #86 PASS.
- **Integración:** `assessment/rom-194` = `05020942` (#201/#207/#203/#208); el diff
  candidato→integración no tiene archivos de producción (solo `docs/`).
- **Fase A:** consolidación documental, **sin radio**. Fase B (hardware) **BLOCKED /
  PENDING_HARDWARE** con causa exacta, ver [§8](#8-protocolo-físico-pendiente).

## 2. Qué ROM se midió (y qué no)

Se midió el **par congelado** y el artefacto de #195, no el ROM del orquestador ni el tip:

| Elemento | Medido | No medido |
|---|---|---|
| GUI | `c888308e` (fuente md5 `964193ae`) | tip `d4fb57f8` (#200/#202/#206) |
| Buildroot | `d084e287` | tip `f23d14f` (PR #2/#3) |
| Artefacto | hashes de #195, revalidados | ROM del orquestador (GUI `3b22a03a…`) |
| Capas | HOST, QEMU_USER | HARDWARE / instrumentos |

**STALE:** si se adopta el tip GUI hay **25 archivos de producción** fuera de `docs/` que obligan
a repetir build/identidad, layout/DATA, init, boot, estrés, update, UI, performance y toda la fase
física. Si se adopta el Buildroot `f23d14f`, además cambian kernel y config de audio. Detalle y
lista de repeticiones en [`stale.md`](stale.md).

## 3. Resumen por dimensión

| Dimensión | Qué se midió | Resultado | Evidencia |
|---|---|---|---|
| Identidad/cobertura (#195) | 334 ítems, 199 fixtures, 114 pruebas, artefacto y gate #86 | PASS de baseline; 17 OK/0 fallos/5 SKIP; CTest ft8/qth BLOCKED; webserver SKIP | [01-baseline](../01-baseline/README.md) |
| ROM/confiabilidad (#196) | layout/rootfs/init/DATA/update/100 ciclos/1 h/negativas | FAIL en tarjeta nueva, update y DB; PASS en layout, prune, protocolo, estrés | [02-rom](../02-rom/README.md) |
| UI/UX (#197) | 199 capturas, 26.455 objetos, 2056 checks, 10 paletas/90 pares WCAG | 0 FAIL; 236 PASS, 15 no-PASS; 22 diálogos inertes; touch/encoders pendientes | [03-ui-ux](../03-ui-ux/README.md) |
| Performance (#198) | 3.980 frames, A/A, natural/wakeups, QEMU, audio null-sink | baseline INCONCLUSIVE por ruido; CPU/CW/timers como REVIEW; FFT/mixer BLOCKED | [04-performance](../04-performance/README.md) |
| Hardware (#199) | no ejecutado | **BLOCKED/PENDING_HARDWARE** | [`hardware-plan.csv`](hardware-plan.csv) |

## 4. Evidencia principal (enlaces durables)

- Baseline, inventario y artefacto: [`01-baseline/README.md`](../01-baseline/README.md),
  [`coverage.csv`](../01-baseline/coverage.csv), [`artifact-manifest.json`](../01-baseline/artifact-manifest.json).
- ROM: [`02-rom/README.md`](../02-rom/README.md), [`results.csv`](../02-rom/results.csv),
  [`reliability.csv`](../02-rom/reliability.csv).
- UI/UX: [`03-ui-ux/README.md`](../03-ui-ux/README.md), [`atlas.md`](../03-ui-ux/atlas.md),
  [`operator-tasks.csv`](../03-ui-ux/operator-tasks.csv), [`ui-coverage.csv`](../03-ui-ux/ui-coverage.csv).
- Performance: [`04-performance/README.md`](../04-performance/README.md), [`results.csv`](../04-performance/results.csv),
  [`bench/`](../04-performance/bench/), [`bench-aa/`](../04-performance/bench-aa/), [`audio/`](../04-performance/audio/).
- Dossier: [`coverage-final.csv`](coverage-final.csv), [`findings.csv`](findings.csv),
  [`hardware-plan.csv`](hardware-plan.csv), [`provenance-audit.md`](provenance-audit.md).

Cada carpeta tiene su `SHA256SUMS` verificado (80/102/815/272 entradas). No se copiaron logs: se
enlazan por ruta y hash.

## 5. Negativos y fallos (sin abrir tickets)

- **DATA en tarjeta nueva (F-01):** `S01create_data` crea p3 vfat; `mount` resuelve ext4 y falla;
  el primer boot no monta `/mnt`; el segundo monta vfat. Contradice el contrato DATA ext4.
- **DB corrupta/RO/faltante (F-02):** SIGSEGV determinista (PC `0x42e12c`, exit 139) y lazo del
  daemon (34 inicios/19 CRASHLOG en 10 s).
- **Update (F-03/F-04):** empaquetador exit 1 por setuid 3 vs 5; `S02update` owner `1000:1000`
  (uid `pulse`); primera update con backup 706/5466; sin checksum/rollback; el tgz se borra aunque
  `tar` falle.
- **Watchdog (F-05):** 0 servicios; wrapper `while true` sin backoff.
- **Performance (F-06/F-07/F-08):** 41,8 wakeups/s por timer WiFi incondicional; CW decoder
  ~9 % de un core sin señal; `radio_thread` ~100 % sin BASE. Los dos últimos son QEMU, no
  hardware; el impacto físico es hipótesis.
- **Render/A-A (F-16):** INCONCLUSIVE por ruido del host; no se declara mejora/regresión.
- **Huecos:** webserver SKIP, CTest ft8/qth BLOCKED, 3 barridos de apps BLOCKED, 22 diálogos
  inertes, sin CI del SHA candidato.

Los **P1** son F-01..F-05. No se repara nada; los issues existentes se enlazan y los nuevos
candidatos quedan documentados para decisión posterior.

## 6. Restricciones de los arneses

- **HOST** mide lógica/render sintético; no mide Cortex-A7, LCD, RF, audio analógico ni batería.
- **QEMU_USER** ejecuta el binario ARM con mocks; **no** arranca kernel/init real, no mide tiempos
  reales ni periféricos.
- **HARDWARE** no se ejecutó en esta fase; todo caso físico es PENDING_HARDWARE/BLOCKED.
- Un PASS de gate/CI o la ausencia de crash en QEMU **no** autoriza a flashear ni demuestra
  ausencia de defectos. Un SKIP/exit 77 nunca es PASS. Un resultado de otro SHA es HISTORICAL.
- #197 se ejecutó antes del merge de #196: su `DEP-196` está superado, pero los 22 diálogos
  inertes siguen sin captura.

## 7. DATA nueva vs existente y update

| Caso | Resultado | Evidencia |
|---|---|---|
| DATA ext4 existente (tarjeta del owner) | PASS: `mount -a` monta ext4; GUI usa `/mnt/params.db` | [02-rom §5.2](../02-rom/README.md#52-lo-que-realmente-pasa-evidencia-ejecutada) |
| DATA vfat existente | PASS: rama 1 monta con el tipo real | idem |
| Tarjeta nueva (sin p3) | **FAIL**: crea vfat y el primer boot no monta; segundo boot vfat | idem |
| Persistencia entre procesos (QEMU) | PASS: segundo proceso restaura freq/modo/banda/filtro | [02-rom §5.3](../02-rom/README.md#53-base-de-datos-por-defecto) |
| Update primera vez | **FAIL**: backup 706/5466; sin integridad/rollback | [02-rom §6](../02-rom/README.md#6-rom-04--update-y-rollback) |
| Update posterior (overlay corregido) | PASS: backup 5466/5466, GUI restaurada | idem |
| Corte de energía | **PENDING_HARDWARE** (HW-06), solo con autorización y medio de prueba | [`hardware-plan.csv`](hardware-plan.csv) |

## 8. Protocolo físico pendiente

No se ejecutó. Estado **BLOCKED** para HR-01..HR-06 y **PENDING_HARDWARE** para HW-01..HW-08.

**Causa exacta:** fase A documental por directiva del orquestador; la radio no se contactó (sin
SSH/socket/RF) y no hay sesión exclusiva verificada, identidad del binario instalado ni snapshot.
La fase B la coordina el owner. No es un fallo del candidato ni una omisión.

**Dato externo:** la orquestación registra que la radio tenía el build del tip (`3b22a03a…`), no
el candidato (`1f998c46…`). Si sigue así, la fase B medirá el software instalado; el candidato
seguirá `PENDING_HARDWARE` salvo SD/prueba autorizada. No se autoriza deploy/flash desde este PR.

**Prerrequisitos:** autorización escrita, ventana exclusiva, backup/restore de DATA, SD(s) de
prueba, instrumentos (V/I/P, audio, cámara; TX solo con autorización específica). Secuencia y
comandos en [`hardware-plan.md`](hardware-plan.md).

## 9. Métricas: unidades, n y dispersión

| Métrica | Valor | n | Unidad / nota |
|---|---:|---:|---|
| Render p50 (mediana entre fixtures) | 4,2946 | 199 | ms; frame forzado x86 384.000 px |
| Render p95 (mediana) | 5,3839 | 199 | ms; p95 de la distribución 10,5582 ms; peor 11,7862 ms |
| Render p99 (mediana) | 5,5980 | 199 | ms; máximo 12,3133 ms |
| FPS derivado (1000/p50) | 232,85 | 199 | **no** es FPS del LCD ni cadencia natural |
| Dispersión MAD-CV (mediana) | 7,579 | 199 | %; 198/199 ≤25 %; 1 escenario 26,898 % |
| A/A p95 delta | −33,02 / −1,645 / +54,39 | 199 | % (mín/mediana/máx); 194 PASS, 5 INCONCLUSIVE, 0 FAIL |
| CW decoder sin señal | 9,17 / 9,03 / 7,29 | 3 | % de un core; mediana 9,03; QEMU |
| `radio_thread` sin BASE | 99,797 | 1 | % de un core; QEMU; no es consumo de la GUI |
| Wakeups estáticos | 41,8 | 1 | /s; timers permanentes; sin medidor de energía |
| Audio voz RMS / pico | −26,53 / −3,70 | 3 | dBFS; null-sink; 24,5 % muestras ≠0 |
| Audio control | 0 | 3 | muestras ≠0; silencio digital exacto |
| Boot real / energía / térmica / LCD / input | null | 0 | PENDING_HARDWARE, no 0 |

## 10. Conclusión con alcance temporal

**CONDITIONAL para el alcance software; NO_GO de release en el estado actual.** La recolección
documental está completa y es auditable, pero: (1) la fase física no se ejecutó; (2) hay P1
candidatos abiertos (DATA nueva, DB, update, watchdog); (3) el tip GUI/Buildroot es STALE; (4) no
hay CI del SHA candidato. El cierre de este dossier **no aprueba la ROM ni autoriza deployment**.
Para GO se requiere fase B con identidad exacta, disposición de los P1, decisión sobre el tip y
revisión/merge del PR final por el maintainer.

*Válido para `c888308e` + `d084e287` + artefacto con los hashes de este dossier, a la fecha
2026-10-22 UTC. Revalidar tips antes de usarlo.*
