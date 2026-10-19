# TDD de la app Propagación (#145): RED → GREEN

Los tests nativos de `src/propagation.c` se escribieron antes que la implementación.
Acá queda la corrida real que lo demuestra: con la matemática del turno rota, la
suite falla (RED); con la fórmula correcta, pasa (GREEN). El binario y el runner
son los del repo (`tests/native/run_native_tests.sh`), sin radio, sin red y sin LVGL.

Los tres logs son corridas completas del runner:

- `01-green.log`: suite completa en verde antes de tocar nada (`40158
  comprobaciones, 0 fallos`).
- `02-red.log`: RED. Se cambió **una sola línea** de `src/propagation.c`, el
  turno del ciclo (`(slot - band)` → `(slot + band)`), y la suite del módulo
  Propagación falló con **25 fallos** (bordes de slot, tabla oficial del IBP,
  plan del barrido). El runner sale con código 1.
- `03-green.log`: se restauró la fórmula y la suite vuelve a verde (`40158
  comprobaciones, 0 fallos`).

La mutación es del **código**, nunca del test: el test es el oráculo (la tabla
oficial NCDXF/IARU, los bordes de slot y el cambio de ciclo) y se queda fijo.

```
$ grep -c FALLO 02-red.log        # 25 fallos, todos del módulo Propagación
$ tail -1 02-red.log              # 40158 comprobaciones, 25 fallos
$ tail -1 03-green.log            # 40158 comprobaciones, 0 fallos
```
