# Radio de vuelta con la DATA en **ext4** (post-apagón, issue #188)

- Rama `lu2dsx/x6200-rom`; build en la radio: **`760af1734ea7e57b10419d4e33df9368`** (= fuentes del tip
  `23313505`) — no hizo falta desplegar.
- El 21/10 ~12:28 el owner/otro rol hizo la **prueba de corte de energía** con la SD nueva
  (`1c900a98`: "prueba de corte de energía exitosa - ext4 recov[eró]").

## Medido por este rol a las 12:33 y 12:53 (post-corte)

| Item | Resultado |
|---|---|
| Root | ext4 **se recupera con journal** (recuperación limpia) |
| **DATA `/mnt`** | `/dev/mmcblk0p3 on /mnt type **ext4 (rw**,relatime)` — **ya no vfat** ✔ |
| `params.db` | 118.784 B, **mtime 15:33** (persiste) ✔ |
| `lastbreath` | fresco (`up_s=309` → sigue subiendo) ✔ |
| `dmesg` FAT | **0 errores** (`FAT-fs` 0 ocurrencias) ✔ |
| Log vivo | **0 `CRASHLOG`** ✔ |
| Params | `cw_tune=0`, `cw_decoder=0`, `key_train=0`, `theme_name=Aurora`, `pwr=60` ✔ |
| GUI | 1 sola, PID estable (2055) durante todo el chequeo ✔ |
| Barrrido 23 apps | **24/24 ok, 0 "REINICIO", 0 "SE CAYO", 0 "no responde"** ✔ |
| Vigilancia 180 s | PID 2055 estable, 0 logs nuevos, `verifica-radio` TODO OK ✔ |
| Batería | externa **11.8 V** (cargada) ✔ |

## Nota

El riesgo de corrupción que describía #188 (vfat + corte) **desaparece con ext4**: el corte de prueba
se recuperó con el journal y no hubo errores de FAT. El `FS` de DATA quedó verificado escribiendo
(`params.db` con mtime nuevo) y leyendo (`lastbreath` avanzando).

## Estado dejado

`7.032.038 kHz LSB, BW 2800`, pagina GEN, RX, sin TX.
