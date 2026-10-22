# Registro para `docs/PRUEBAS.md` — propuesta lista para pegar

> **No aplicado en este PR.** El issue #199 pide agregar una fila/sección en `docs/PRUEBAS.md`;
> por instrucción de alcance del orquestador este PR commitea solo `05-summary/`. El contenido de
> abajo está listo para que el PR de integración final (o una tarea autorizada) lo pegue en
> `docs/PRUEBAS.md`. Los enlaces están escritos **como deben quedar en `docs/PRUEBAS.md`**
> (`evidencia/assessment-rom-194/...`), no relativos a este archivo.

## Fila para la tabla "Resumen"

```markdown
| 22/10 (UTC) | (sin radio; candidato GUI **`c888308eede712`**, 13.407.624 B; Buildroot `d084e287`; fuente `964193ae…`) | `c888308e` (#193) | **no ejecutado** (fase A sin radio) | **no ejecutado** (BLOCKED por banco ocupado) | — | **ASSESSMENT #194: CONDITIONAL / NO_GO de release** — evidencia software 01..04 integrada y auditada (334 ítems, 1.025 métricas, SHA256SUMS OK, gate #86 PASS); **hardware BLOCKED/PENDING_HARDWARE** con causa (fase A documental; sin sesión exclusiva ni identidad del binario instalado). P1 fundados: DATA nueva vfat/primer boot sin `/mnt`, SIGSEGV de DB corrupta/RO/faltante, update con owner 1000 y backup 706/5466, sin watchdog. STALE para el tip GUI `d4fb57f8` (25 archivos de producción) y Buildroot `f23d14f`. Evidencia: [`assessment-rom-194/05-summary/`](evidencia/assessment-rom-194/05-summary/README.md) · [dossier](evidencia/assessment-rom-194/05-summary/ASSESSMENT_ROM_194.md) · [findings](evidencia/assessment-rom-194/05-summary/findings.csv) · [hardware-plan](evidencia/assessment-rom-194/05-summary/hardware-plan.csv) |
```

## Sección para el final de `docs/PRUEBAS.md`

```markdown
## 22/10 (UTC) — Assessment pre-release #194: dossier `c888308e` (sin radio)

**Qué se probó:** la campaña completa de evidencia #194 (unidades #195..#199) sobre el candidato
GUI `c888308eede71205605c06383432ee7e165f202e` + Buildroot
`d084e2870a00b64d9c57474e7eceeef80f43f25b` + LVGL `74d0a816…`, con el artefacto de #195
(`sdcard.img` `d0ff4435…`, `rootfs.tar` `7c26c76b…`, GUI md5 `1f998c46…`, fuente-md5
`964193ae…`, gate #86 PASS). **No se desplegó, flasheó, transmitió ni se tocó la radio**; esta fase
es consolidación documental. El hardware queda **BLOCKED/PENDING_HARDWARE** con causa.

**Medido (HOST/QEMU_USER):** 334 ítems de inventario; 1.025 filas de resultados (PASS 933,
INCONCLUSIVE 24, REVIEW_REQUIRED 21, FAIL 15, BLOCKED 13, PENDING_HARDWARE 12, SKIP 5,
UNSUPPORTED 2); 199 capturas UI, 26.455 objetos, 2.056 checks de contrato y 90 pares WCAG;
3.980 frames de render (p50 4,2946 ms / p95 5,3839 ms / MAD-CV mediana 7,579 %); 10 estados de
recursos QEMU; 3 pares de audio digital null-sink. `sha256sum -c` de 01..04: **OK**
(80/102/815/272).

**Código (sin ejecutar):** timers WiFi de 30 ms incondicionales (~41,8 wakeups/s); ausencia de
watchdog y wrapper `while true` sin backoff; migración 2 no idempotente; `/bin/sh`→bash con arrays
en init scripts.

**No medido (hardware/instrumentos):** energía/térmica, boot-to-ready real, LCD/input físico,
audio analógico/mixer/dropouts, FFT/carga DSP, RX/TX reales, update/rollback con corte de energía,
DB llena/RO en el equipo, servicios residentes. Todo tiene caso y criterio en
[`hardware-plan.csv`](evidencia/assessment-rom-194/05-summary/hardware-plan.csv).

**Hallazgos fundados P1:** tarjeta nueva crea DATA vfat y el primer boot no monta `/mnt`; DB
corrupta/RO/faltante → SIGSEGV determinista + crash-loop; empaquetador de update exit 1 y
`S02update` owner 1000 (`pulse`); primera update con backup 706/5466 y sin checksum/rollback; sin
watchdog. Los P2/P3 y las **hipótesis separadas** están en
[`findings.csv`](evidencia/assessment-rom-194/05-summary/findings.csv) /
[`findings.md`](evidencia/assessment-rom-194/05-summary/findings.md). No se abrió ningún ticket.

**Limitaciones:** sin radio; #197 se ejecutó antes del merge de #196 (su `DEP-196` quedó superado,
pero los 22 diálogos inertes siguen sin captura); no hay CI del SHA candidato; el defconfig
congelado no reproduce el artefacto desde cero (F-11 resuelto en Buildroot PR #2, sin reconstruir);
el dossier es **STALE** para el tip GUI `d4fb57f8` (25 archivos de producción) y Buildroot
`f23d14f`. Detalle y repeticiones en
[`stale.md`](evidencia/assessment-rom-194/05-summary/stale.md).

**Veredicto:** **CONDITIONAL** para el alcance software; **NO_GO de release** hasta ejecutar la
fase B con identidad exacta y disponer de los P1. Cierre de recolección **no** es aprobación de
producto ni autorización de deployment. Evidencia completa:
[`05-summary/README.md`](evidencia/assessment-rom-194/05-summary/README.md).
```

## Nota de auditoría

Al pegar la sección, `python3 tests/audita-pruebas.py` debe seguir saliendo **0**; los enlaces
citados deben existir en `docs/evidencia/assessment-rom-194/05-summary/`. En este PR la auditoría
se corrió sobre el repo **sin** modificar `PRUEBAS.md` y dio **0** (ver
`05-summary/logs/audita-pruebas.log`).
