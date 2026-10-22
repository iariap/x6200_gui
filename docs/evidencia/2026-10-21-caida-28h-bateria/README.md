# Caída de la radio tras 28h20min — datos de alimentación previos (apunta a corte de energía)

El 21/10 ~05:00 (hora host) la radio **dejó de responder** (sin ping ni SSH). El otro agente lo registró
en `68dd9e63` ("la radio dejo de responder tras 28h20min (primera caída después del fix del #131)").

## Lo que este rol midió en las horas previas (batería, del LASTBREATH)

| Hora host | bat interna | ext (externa) | msg del LASTBREATH |
|---|---|---|---|
| 23:53 (20/10) | 8.4 V | 9.8 V | `Battery recovered: 8.4 V` |
| 00:13 | 8.3 V | 9.6 V | `Battery low: 8.3 V` |
| 00:53 | 8.0 V | 9.4 V | `Battery recovered: 8.1 V` |
| 02:33 | 7.5 V | 9.5 V | `Battery recovered: 7.5 V` |
| 04:13 | 7.3 V | 9.7 V | `Battery recovered: 7.3 V` |
| **04:53 (última lectura)** | **6.7 V** ✗ | **9.9 V** | `Battery recovered: 6.7 V` |
| 05:13 | — | — | **sin respuesta** ✗ |

Además, antes de la caída la radio ya **derrateaba por alimentación**: `Power was limited to 7W` (~12.0 V
de externa) y `Power was limited to 5W` (10.7 V). La interna de 6.7 V está **muy por debajo** de lo sano.

## Lectura (hipótesis, NO conclusión)

**La causa más probable es un corte/agotamiento de alimentación**, no un cuelgue del firmware:

- La interna venía en **caída libre** (8.4 → 6.7 V en 5 h) y la externa ya estaba en 9.9 V (baja).
- La radio ya estaba **limitando potencia por alimentación** en las horas previas.
- El uptime del equipo era **28h20** sin cuelgues (7× el ritmo anterior de 1-4 h): un cuelgue justo al
  final de una descarga es mucho menos probable que el corte de energía.

**No se puede afirmar**: un cuelgue del firmware también deja `clean=0` y sin ping. Para distinguirlo hace
falta, cuando vuelva la radio:

1. Leer `/mnt/lastbreath` (estado final: `radio`, `tx`, `app`, `bat_mv`, `ext_mv`, `msg`).
2. Leer `dmesg` (¿mensajes de undervoltage/apagado?) y los `app_logs` nuevos.
3. Si hay `CRASHLOG`, es firmware (issue). Si no, y `bat_mv` estaba por el piso, es alimentación.
4. Confirmar si el equipo arrancó con **uptime chico** (corte) o reporta `LASTBREATH salida sucia`.

## Nota de método

Este rol estaba en **modo pasivo** (solo `verifica-radio` cada 20 min) desde que la batería entró en zona
crítica, para no consumir de más. Se hizo un `sync` a las 02:33 (Dirty 16 kB) para proteger la DATA de un
corte inminente.
