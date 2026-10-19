# CB 27 MHz (#146): evidencia de host y banco QEMU

Rama `factory/cb-27mhz-146` (base `ae1ba0fe`), sin despliegue y **sin transmitir**.

## Arnés de UI (host, `tools/ui_harness`)

Fixtures nuevos `home-cb` (27.185 MHz, `CH 19`) y `home-cb-offchannel` (27.000 MHz,
`CH --`), validados por `scenario_settle` (si la celda no muestra el texto, el arnés corta).

Medición de la celda VFO en los seis temas (`--inspect`, `text_natural` contra la celda de
56 px), en `arnes-temas-ch19.json`:

| Tema | `text_natural` ancho | Celda | `text_truncated` |
|---|---|---|---|
| Aurora | 41 px | 56 px | false |
| Cathode | 47 px | 56 px | false |
| Redshift | 40 px | 56 px | false |
| Sprint | 35 px | 56 px | false |
| Sunlit | 48 px | 56 px | false |
| Trail | 47 px | 56 px | false |

Capturas: `arnes-home-cb-aurora.png` y `arnes-home-cb-sunlit.png`.

## Banco QEMU (binario ARM real, gate #86)

`tools/captura-banco.sh` con `CHROOT=/home/pablo/desa/x6200/.vfix3` y
`RECONSTRUIR=no` (binario ya compilado desde este árbol):

```
OK: /home/pablo/desa/x6200/.vfix3/usr/sbin/x6200_gui es el firmware del port
    (ELF32 ARM, 13783668 B, 3/3 marcas)
binario: md5 882e1023493c   qemu-cpu: cortex-a7
```

Capturas: `banco-ch19.png` (27.185 MHz: `CH 19`, AM, **TX power 4.0 W** con la base en 5.0) y
`banco-ch--.png` (27.000 MHz: `CH --`, AM, TX power 4.0 W). En ninguna corrida se envió un
comando de TX: el estado del banco da `tx=false` (`banco-estado.txt`).

`banco-estado.txt` también muestra:

- los cuatro grids quedan en **AM** (26.100 / 26.600 / 27.450) y `band=CB A..D`;
- 27.300 MHz cae en **CB C** con los rangos migrados (con los rangos viejos, 27.205–27.405 no
  tenía banda);
- los rangos de `/mnt/params.db` del chroot son los canónicos de `sql/bands_cb.csv`.

El campo `pwr` de `{"cmd":"get"}` es el espejo de `SettingsManager` (`cfg_sm.p_pwr`), que
conserva el último valor que le escribió el control remoto; no refleja el recorte hecho por el
módulo CB. La potencia que muestra la pantalla (4.0 W) sale de `cfg.pwr`, que es el valor
recortado.

## F0 (requires-radio)

No se transmitió, no se desplegó y no se flasheó. Quedan pendientes de medir en la radio, con
autorización del owner y carga fantasma: RF real en 27.185, potencia, armónicos, PTT del
micrófono y ATU (checklist §10.3 del issue).
