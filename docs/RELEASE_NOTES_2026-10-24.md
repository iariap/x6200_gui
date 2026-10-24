# X6200 LU2DSX ROM — v0.1.3-lu2dsx (pre-release)

**Fecha**: 24/10/2026 · **GUI**: main `7fca2a59` · **Buildroot**: `de27194c`

## Qué trae esta versión

### Fix #292: la banda lateral LSB/USB demodula correctamente
El BASE demodula con el modo del **VFO activo**; la GUI solo escribía el modo del VFO A (#244),
así que si el VFO activo era el B su registro quedaba viejo (USB) y en LSB se oía la banda de
arriba. Ahora la GUI sincroniza el modo del VFO activo **al arrancar, al cambiar de modo y al
cambiar de VFO** (PR #303), sin escribir el VFO B en CW/CWR para no reproducir el mute de #244.

**Validado en la radio** con portadora real de AM en 790 kHz (dial 1 kHz abajo/arriba):

| prueba | con el fix | firmware original |
|---|---|---|
| 789-LSB (debe filtrar arriba) | 27.0 dB | 37.1 dB |
| 789-USB (debe oír) | 57.2 dB | 68.8 dB |
| 791-LSB (debe oír abajo) | 56.9 dB | 70.4 dB |
| 791-USB (debe filtrar) | 26.5 dB | 39.3 dB |

### UI #301/#302
Fix P1 de legibilidad de widgets en el tema Sunlit (texto invisible) y la evidencia de QA de la
épica #284 (PR #301, PR #302).

### Herramientas y documentación
- `tools/lsb-sideband-test.sh`: re-valida la banda lateral en un minuto (portadora + FT8).
- Hallazgos §68 (causa raíz de #292), §69 (mapa de lectura I2C del BASE) y §70 (mute de #244).

## Artefactos
- **Imagen SD completa**: `x6200_20261024_083535.img` (sha256 `85361131…`).
- **Actualización en el lugar**: `x6200_r1cbu_v0.1.3-lu2dsx.tgz` + sidecar sha256 (`7d1e57bb…`);
  el `S02update` la busca en DATA. El registro §4 del proyecto dice que para actualizar conviene
  la imagen completa.
- `SHA256SUMS` con ambos.

## Estado verificado en la radio
GUI `063f42f8d8284ba5f9aa40d9ae4a2d2d` (misma que la imagen y el tgz), `CRASHLOG` 0, audio de RX
sano, BT apagado. La ROM original de fábrica (eMMC) no se tocó.
