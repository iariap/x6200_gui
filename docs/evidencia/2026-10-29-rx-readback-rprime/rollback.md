# Rollback / restauración — R′ (#369)

## 1. Ejecución real (reintento 2026-10-30 03:17Z): backup + deploy + rollback EJECUTADOS

Con la radio liberada por el owner, el procedimiento se ejecutó y verificó:

| paso | resultado |
|---|---|
| backup | `/mnt/ui-bak-rprime-20261030-031717/x6200_gui` · sha256 `4ee87aef…66b0` (verificado) |
| prueba de rollback **antes** del deploy | copia del backup sobre `/usr/sbin/x6200_gui` → hash idéntico `4ee87aef…66b0` |
| deploy temporal | `/usr/sbin/x6200_gui` sha256 `25f281b0…c19d` (14006244 B), sólo GUI |
| ejecución | flag `/tmp/x6200_reg12_probe.enable`; kill GUI (pid 4716) → relanzada (pid 15356) → `READBACK_PROBE_DONE`; flag eliminado |
| rollback | `cp backup /usr/sbin/x6200_gui; chmod 755; kill GUI` → pid 15470; sha256 `4ee87aef…66b0` verificado |
| limpieza | `/tmp/x6200_reg12_probe.enable`, `/tmp/x6200_reg12_probe.log` y `/tmp/x6200_gui.rprime` eliminados |
| estado final | `tx=false`, `radio=RX`, AM 7.355 MHz, vol 1, `app=""`; MUX nunca conmutado |

Estado medido antes y después (el proceso GUI sí se reinició por el deploy/rollback):

| ítem | valor |
|---|---|
| `/usr/sbin/x6200_gui` sha256 (antes y después) | `4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0` |
| `/usr/sbin/x6200_gui` md5 (antes y después) | `80bbe5bd70b345e1e9a563d06d047a1c` |
| `libaether_x6200_control.so.0.15.3` sha256 | `80c460e403bfe77684c773a50a054bfe6c4a858753695a01167493e35e126df6` |
| GUI pid | antes `4716`; después `15470` (el kill/relanzamiento del deploy/rollback es esperado) |
| estado final | `tx=false`, `radio=RX`, sin ATU/SWRscan/CW-key |
| MUX `voice_rec` | nunca conmutado; ninguna escritura de registros |
| backup creado | `/mnt/ui-bak-rprime-20261030-031717/` (GUI original + `installed.sha256`) |

Evidencia: `logs/deploy.txt`, `logs/restore.txt`, `logs/final-state-retry.txt` y `logs/preflight-monitor2.log`.

## 2. Plan de backup/rollback (referencia; ejecutado en el reintento)

Si en un reintento con preflight PASS se llegara a desplegar el binario temporal, el procedimiento
(ya usado y verificado en #351) es:

```sh
# 1) Backup del binario instalado y de su hash (en la radio, fuera del repo)
BK=/mnt/ui-bak-$(date -u +%y%m%d-%H%M%S)
mkdir -p "$BK"
cp /usr/sbin/x6200_gui "$BK/x6200_gui"
sha256sum /usr/sbin/x6200_gui | tee "$BK/installed.sha256"
cp /mnt/params.db "$BK/params.db.pre-rprime" 2>/dev/null || true

# 2) Deploy SOLO del binario GUI temporal (nunca rootfs/daemon/libs/params)
#    scp del binario ARM compilado -> /usr/sbin/x6200_gui
sha256sum /usr/sbin/x6200_gui          # verificar el hash del temporal

  # 3) Ejecutar el sondeo: el parche corre UNA vez si existe el flag
  #    no persistente /tmp/x6200_reg12_probe.enable; luego se lee el log
  #    /tmp/x6200_reg12_probe.log y se vuelca fuera de la radio.

# 4) Rollback SIEMPRE (exito o fallo): reponer el binario original
cp "$BK/x6200_gui" /usr/sbin/x6200_gui
kill $(pidof x6200_gui)                # el daemon la relanza
sleep 8
sha256sum /usr/sbin/x6200_gui          # debe ser 4ee87aef…66b0
pidof x6200_gui                        # 1 GUI viva
# verificar tx=false y estado RX por get
cat /mnt/lastbreath
```

Criterios de aceptación del rollback (a verificar en el reintento): hash restaurado
`4ee87aef…66b0`, una sola GUI viva, `get` respondiendo, `tx=false`, `radio=RX`, sin MUX cambiado.

## 3. Condición para reintentar (histórico)

- Radio en **RX estable** sin operador activo (coordinado): sin TX observado, sin app abierta ni
  cambios de frecuencia/modo/volumen durante la ventana de preflight.
- Pipeline de build #351 **poblado y reproducible** (volumen con toolchain/build de la ROM) y build ARM
  completo del binario parcheado con hash registrado.
- Con ambas condiciones: una única ejecución del protocolo y rollback inmediato; si cualquiera falla,
  `BLOCKED_PREFLIGHT`/`BLOCKED_BUILD` otra vez y **no se fuerza**.

## 4. Reglas que se respetaron

No se flasheó ROM ni se tocó rootfs/daemon/libs/params; no hubo TX/PTT/ATU, ni `audio_play_en`, ni
cambios de mixer/modo/VFO/frecuencia/volumen/BT/MUX; no se grabó ni publicó audio; no se usaron
credenciales/IPs en la evidencia; no se versionaron binarios. El MUX experimental sigue **OFF**.
