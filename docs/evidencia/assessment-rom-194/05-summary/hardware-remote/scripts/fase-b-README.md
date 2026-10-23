# Fase B del #199 — QA físico remoto (lista para ejecutar)

**Estado:** PREPARADA, no ejecutada. Bloqueada por: radio apagada/no accesible + autorización escrita del owner + ventana exclusiva.

## Script

`phase-b-remote.sh` — corre el runbook HR-01..HR-06 del dossier (`docs/evidencia/assessment-rom-194/05-summary/hardware-plan.md`),
**solo RX, sin TX, sin flash, sin reformatear**, con snapshot y restauración. Guarda evidencia en `fase-b/<label>/`:
`logs/`, `json/`, `capturas/`, y `BLOCKED` si falta un prerrequisito.

```bash
cd /home/pablo/desa/x6200/assessment-194
./phase-b-remote.sh --i-have-authorization --label 20261022Txxxx
```

- Sin `--i-have-authorization` → **rechaza** (exit 3).
- Sin radio en red o sin ping → **BLOCKED** (exit 4) con causa en `BLOCKED`.
- Verifica exclusividad (conexiones SSH ajenas a la PC del operador) y la deja en `logs/hr01-ssh-conns.txt`.

## Qué cubre y qué NO

| Cubre (automatizado) | No cubre (requiere instrumentos/manual) |
|---|---|
| HR-01 identidad/boot_id/uptime/montajes/params/CRASHLOG/boot.log | HW-01 boot real cronometrado con cámara/fuente con interruptor |
| HR-02 baseline (get inicial, captura, verificación de salud) | HW-02 controles/touch/encoders/temas a la luz del sol |
| HR-03 navegación RX (apps una por una, 20 s cada una) | HW-03 audio analógico, mixer, FFT/DSP, Auto Tune con RF |
| HR-04 QSY reversible 20 repeticiones con restauración | HW-04 TX/PTT/ATU/SWR (autorización TX específica) |
| HR-05 60 min RX con serie nativa (CPU/RSS/fds/threads/DB) | HW-05 energía/térmica (fuente + medidor calibrado) |
| HR-06 cierre, restauración y evidencia antes/después | HW-06 update/rollback con corte de energía, DB llena/RO |

## Requisitos del owner

1. Autorización escrita para la fase B (RX; TX sólo si se autoriza aparte).
2. Radio encendida y en red, ventana exclusiva (sin otros agentes).
3. Decisión de identidad/SD: (a) medir el tip instalado, (b) flashear el artefacto del candidato en una SD de prueba, o (c) ambas.
4. Backup+restore de DATA verificado antes de tocar nada (ya hay backups locales).
5. Instrumentos para HW-02..HW-08 (V/I/P calibrado, térmica, audio/RF, cámara, medios de prueba) o aceptar BLOCKED documentado.
