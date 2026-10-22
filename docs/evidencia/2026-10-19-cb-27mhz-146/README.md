# CB 27 MHz (#146): cierre de la corrida — host, arnés y banco QEMU

Rama `factory/cb-27mhz-146`, base `1e3a1e49`, HEAD `ade36bbb` (F4-fix). **Sin desplegar,
sin flashear y sin transmitir**: en el banco el estado da `tx=false` en todas las
consultas. La RF real (F0) queda pendiente y la hace el owner con autorización.

## Arnés de UI (`tools/ui_harness`)

Fixture `home-cb` (27.185 MHz, AM): la celda VFO de 56×26 px muestra `CH 19` sin
recortarse en los seis temas. El ancho se midió con `--inspect` sobre la etiqueta viva
(`text_natural`), con la familia tipográfica que nombra cada tema (el arnés no aplica el
font del tema por sí solo: hay que pasarlo con `--font`). Datos en
`arnes-temas-ch19.json`:

| Tema | Familia (`--font`) | `text_natural` ancho | Celda | `text_truncated` |
|---|---|---|---|---|
| Aurora | inter | 41 px | 56 px | false |
| Cathode | ibmplexsans | 47 px | 56 px | false |
| Redshift | sourcesans3 | 40 px | 56 px | false |
| Sprint | robotocondensed | 35 px | 56 px | false |
| Sunlit | notosans | 48 px | 56 px | false |
| Trail | ibmplexsans | 47 px | 56 px | false |

Máximo 48 px ≤ 56 px; `text_overflow=false` en los seis. Capturas:
`arnes-home-cb-aurora.png` y `arnes-home-cb-sunlit.png`.

El fixture `home-cb-offchannel` (27.000 MHz) muestra `CH --`; ambos fixtures cortan
(`scenario_settle`) si la celda no muestra el texto esperado, así que el arnés no puede
pasar con la celda rota.

## TDD de `tests/native/test_cb.c`

`red-green-cb.txt`: contra una implementación degenerada de `cb.c` la suite marca
**85 comprobaciones, 56 fallos**; contra `src/cb.c` real, **85 comprobaciones, 0 fallos**.
Cubre los casos de §10.1 (los cinco huecos, el desorden 23/24/25 —27.255/27.235/27.245—
y el 23→24 que baja de frecuencia) y los bordes de la ventana de TX.

## Banco QEMU (binario ARM real, gate #86)

`tools/captura-banco.sh` con `CHROOT=/home/pablo/desa/x6200/.vfix3`,
`VOLUMEN=x6200-build-output` y `RECONSTRUIR=no` (binario ya compilado desde este árbol):

```
OK: /home/pablo/desa/x6200/.vfix3/usr/sbin/x6200_gui es el firmware del port
    (ELF32 ARM, 13783700 B, 3/3 marcas)
binario: md5 4893b8f39bf3   qemu-cpu: cortex-a7   fuente: ea96e4f92dff
```

Capturas y estado en `banco-estado.txt`:

- 27.185 MHz → `CH 19` (`banco-ch19.png`), AM, **TX power 4.0 W** (la base tenía 5.0 y el
  módulo la recortó);
- 27.000 MHz → `CH --` (`banco-ch--.png`), AM;
- 27.255 MHz → `CH 23` (`banco-ch23.png`) y 27.235 MHz → `CH 24` (`banco-ch24.png`): el
  dial cuenta canales, por eso 23→24 sube el número y **baja** la frecuencia;
- 26.100 / 26.600 / 27.300 / 27.450 → `CB A` / `CB B` / `CB C` / `CB D`, todos en **AM**;
- rangos de `/mnt/params.db` del chroot = los canónicos de `sql/bands_cb.csv` (migrados);
- al arrancar con la base sembrada en CB con `xit=100` y `split=1` en 203, la GUI los deja
  en **0/0**: política de entrada del arranque;
- `tx=false` en todas las consultas: no se envió ningún comando de TX.

**El dial en el banco:** el control remoto no tiene comando para inyectar eventos del
rotativo (no hay `{"cmd":"rotary"}`), así que el banco mide la **celda de canal** en las
frecuencias del plan (19, 23, 24 y el hueco `--`). El conteo 1 detent = 1 canal está
medido en el test nativo (`cb_step_channel`) y en el fixture del arnés.

## F0 (requires-radio)

No se transmitió, no se desplegó y no se flasheó. Quedan pendientes de medir en la radio,
con autorización del owner y carga fantasma: RF real en 27.185, potencia, armónicos, PTT
del micrófono y ATU (checklist §10.3 del issue).
