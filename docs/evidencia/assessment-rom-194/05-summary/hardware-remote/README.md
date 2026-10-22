# Fase remota RX (HR-01..HR-06) — EJECUTADA en el tip

- **Campaña:** #194, unidad 5/5 (#199), fase B (QA físico remoto RX).
- **Ejecución:** 2026-10-22 02:24–03:35 UTC, por el orquestador con autorización del owner (opción "correr HR-01..06, solo RX").
- **Equipo medido:** la ROM **instalada = tip** (`sdcard.img 03c6f9e6…`, GUI md5 **3b22a03a3b7850e5c546c564125d0eb0**, 13.412.248 B,
  `boot_identity fuente a576d48d9994efd3e28b9f1438fa7b24`, kernel 6.1.82, `boot_id cdd42e40`). **No** es el candidato del dossier
  (`1f998c46…` / `964193ae…`) → el dossier del candidato sigue **STALE para el tip** y estos resultados valen para el tip.
- **Salvaguardas:** sesión exclusiva verificada (0 conexiones SSH ajenas); **sin TX**, sin flash, sin reformatear; snapshot y
  restauración de freq/app; cliente ARM de control copiado a `/tmp/x6200-remote` (no persistente, se borra al reiniciar).

## Resultados

| Caso | Estado | Medición |
|---|---|---|
| HR-01 exclusividad/identidad/snapshot | **PASS** | 0 SSH ajenas; identidad md5/boot_id; `/` y `/mnt` ext4 rw; CRASHLOG 0 |
| HR-02 baseline física | **PASS** | captura `home-before.png` 268.069 B; `verifica-radio` 57 líneas OK (audio con sink real) |
| HR-03 navegación RX | **PASS** | **8/8 apps** abiertas/capturadas/cerradas (Settings, BT, WiFi, GPS, SSTV, WeFax, CW, FT8), sin crash |
| HR-04 QSY reversible | **PASS** | **20/20** vuelven a F0=7.105.008 Hz; set 394-402 ms · restore 1,398-1,637 s |
| HR-05 estabilidad 60 min RX | **PASS** | 60 muestras/59,5 min; 1 boot_id; CRASHLOG 0; RSS 23.348 kB constante; fds 39-40; threads 13; get RTT mediana 408 ms; params md5 estable salvo 1 cambio (sin corrupción); CPU GUI mediana 54,0% de un core (52,4-62,0%) |
| HR-06 cierre/restauración | **PASS** | freq restaurada 7.105.008 Hz; app off→home; captura `home-after.png` |

Observación no instrumentada (no cuenta como HW-05): durante el soak la batería pasó de 8.600→8.000 mV y cap 90→71% con
`ext_mv` 10,8→10,2 V; sin medidor calibrado no se reporta consumo en W ni autonomía.

## Pendiente (BLOCKED)

- **HW-01..HW-08** siguen **BLOCKED/PENDING_HARDWARE**: boot cronometrado con fuente/cámara, controles/touch a la luz del sol,
  audio analógico/mixer/FFT, **TX/ATU/SWR** (requiere autorización TX específica), energía/térmica con instrumentos, update con
  corte de energía y DB llena/RO, periféricos/cluster, y 8 h de estabilidad. Ver `../hardware-plan.md` y `../hardware-plan.csv`.
- La fase B midió el **tip**, no el candidato: si el maintainer adopta el tip, estos HR son evidencia directa; el dossier del
  candidato (`c888308e`) conserva su calidad de STALE y requiere repetición si se adopta el tip.

## Crudo

`raw/` contiene `logs/` (run.log, identidad, SSH, latencias, serie del soak), `json/` (87 archivos: get inicial y por app,
aperturas/cierres, 20 repeticiones QSY, get final) y `capturas/` (10 PNG: home antes/después + 8 apps). Todo incluido en
`../SHA256SUMS`.
