# A/B/A en vivo 40 m — 2026-11-04/05

## Condiciones
- Radio: **7.100.000 kHz LSB**, filtro 2700, volumen 20, RX-only.
- QSO real de 40 m con **QRM alto**.
- Sin TX/PTT/ATU; sin firmware/flash/MUX manual.
- Bloques: BASE ~12 s → Linux DSP SSB Voice ~15 s (QA activo) → BASE ~12 s.
- El owner informó antes que, al volver a BASE, Linux DSP le había parecido **más brillante y claro**.

## Activación QA
```text
x6200 QA: gate OK ipc=/run/x6200_route.sock
x6200 QA: IPC conectado, wiring READY (sin activar)
x6200 QA: PCM post-Speex fresco; pidiendo ENABLE al arbitro
x6200 QA: activation err=0 state=ACTIVE
```

## Resultado perceptual
El owner eligió el **bloque Linux DSP** como mejor en A/B/A, describiéndolo más brillante/claro.
Sin cortes reportados.

## Lectura correcta
- Evidencia **AUDIBLE/PARTIAL**, no A/B ciego ni nivelado formalmente.
- No demuestra que el MUX físico esté verificado ni que el restore ≤250 ms.
- No es PASS de producción.
- `LINUX_DSP_AUDIBLE_DIFFERENCE` pasa de PENDING a **PARTIAL (AUDIBLE)**.
- `PRODUCTION_READY` sigue **FAIL** por los gates físicos/recuperación de #398.

## Restauración
GUI normal restaurada; QA/servicio detenidos; gate eliminado; estado 7.100 kHz LSB, filtro 2700,
volumen 20, RX, tx=false; firmware 1.0.8; sin deploy.
