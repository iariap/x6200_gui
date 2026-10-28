# X6200 LU2DSX ROM — v0.1.11-lu2dsx (pre-release)

**Fecha**: 28/10/2026 · **GUI**: main `b9ea103e` (código `c7e7f9ec`) · **Buildroot**: `b44eb81`

## Qué trae esta versión

### Barrido de ROE (SWR Scan)
- **Techo de TX por pasada con el dwell real medido**: la pasada asumía 35 ms por punto y la
  radio tarda 48,4 ms de media (63 ms peor caso). `SWRSCAN_DWELL_MS` 35 → 70, presupuesto
  1785 → **3570 ms** y timeout 2285 → **4070 ms**: el barrido ahora **completa los 51/51 puntos**
  (antes abortaba con `Error: RF timeout` a los 33–43 puntos).
- **Potencia del barrido 0,1 W → 8,0 W pedidos** (techo del control del equipo): medido
  48–49/51 muestras con potencia en 40/17/15/10/6 m (con 0,1 W el detector no medía en 15/10 m).
  La potencia reportada por el flow queda ~1 W (el proceso del barrido la limita).
- **Gate de validez**: un paquete sin TX/potencia (<0,3 W) no se registra; se reintenta el mismo
  punto hasta 6 paquetes y, si no hay medición, queda un hueco (no se inventa ROE 1.0).
- **Curva = cuadrática libre de las muestras medidas** (elegida por el dueño por ser la más
  **fiel y suave**: RMSE 0,03–0,23 ROE contra las muestras; sin espejar, centrar ni forzar valle).
  El mínimo del readout sale de las muestras.
- El chart ya no dibuja la polilínea cruda ni el ajuste forzado de versiones intermedias.

### Ya incluido desde v0.1.10
- Tab **Logs** en Ajustes (primera página) con las credenciales de QRZ/eQSL/Clublog inline.
- Uploader multi-proveedor (eQSL verificado), TLS en la imagen, Auto CQ de FT8.

## Artefactos
- Imagen SD completa (`sha256 d3f22d806f5279d533ca86d712c903d911b378be139ea67735141fa06f496d0b`)
- Actualización en el lugar `x6200_r1cbu_v0.1.11-lu2dsx.tgz` + sidecar + `SHA256SUMS`

## Nota de flasheo
Flashear con `sudo bash verify-flash-rom.sh /dev/sda` (conserva DATA y verifica UUID).
