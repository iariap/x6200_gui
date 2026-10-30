# R′ (#369) — sondeo read-only del registro 12: veredicto

**Issue:** [#369](https://github.com/iariap/x6200_gui/issues/369) · **Épico:** [#362](https://github.com/iariap/x6200_gui/issues/362) · **Antecedentes:** [#363](https://github.com/iariap/x6200_gui/issues/363)/[PR #366](https://github.com/iariap/x6200_gui/pull/366), [#367](https://github.com/iariap/x6200_gui/issues/367)/[PR #368](https://github.com/iariap/x6200_gui/pull/368).
**Base `main` verificada:** `cb9ea287548534ac5c4438c786b57deb436c7c40`.
**Worktree/rama:** `x6200-rx-readback-probe` / `audit/rx-reg12-readback-probe` · **PR docs-only:** [#370](https://github.com/iariap/x6200_gui/pull/370).

## Veredicto: `R_INCONCLUSIVE` — no se acredita readback real; no hay `A_PASS`

El intento único de lectura se **ejecutó** en la radio (GUI temporal, una sola vez, sin conmutar MUX) y devolvió:

| paso | addr/len | rc | bytes crudos | LE u32 | t (ms, monótono) |
|---|---|---|---|---|---|
| control | `0x2000`/1 | 0 | `01` | 1 | 32092679 |
| b1 | `0x0038`/4 | 0 | `00 00 00 00` | 0 | 32092680 |
| c1 | `0x0030`/4 | 0 | `00 00 00 00` | 0 | 32092680 |
| b2 (tras 52 ms) | `0x0038`/4 | 0 | `00 00 00 00` | 0 | 32092732 |
| c2 (tras 53 ms) | `0x0030`/4 | 0 | `00 00 00 00` | 0 | 32092733 |

**Shadow (sólo diagnóstico, nunca readback):** `reg12=0x00001000`, `rxvol=0x00000001`.
**Log:** `READBACK_PROBE_DONE verdict=R_INCONCLUSIVE reads=5 reason="ambas lecturas uniformes/triviales (00/FF); no atribuible"`.

Interpretación (regla pre-registrada del issue): el control `0x2000` demuestra que el **selector/framing
I²C es aceptado** (byte `0x01`), pero las dos lecturas de datos fueron **uniformes y triviales
(`00000000`)**, estables entre las dos tomas. La uniformidad trivial **no prueba error ni éxito** ⇒
`R_INCONCLUSIVE`, **no** `R_CANDIDATE_FOUND` ni `A_PASS`. Se documenta además una **incongruencia** con un
valor conocido: el control `0x0038` (reg 14, `rxvol`) debía reflejar el nivel `1` que muestran el GET y
la sombra (`0x00000001`), y devolvió `0`: señal de que la vía probada no entrega el estado real, pero el
issue reserva la uniformidad trivial a `INCONCLUSIVE`; no se declara
`R_NO_READBACK_ON_TESTED_PROTOCOL` con una sola toma uniforme.

**Conclusión operativa:** la vía probada **no habilita** una seguridad basada en readback.
`R_PRIME_PHYSICAL_NOT_VERIFIED`; el MUX no se conmutó; **#364/#365 siguen bloqueados** y no hay `A_PASS`.

## Secuencia ejecutada (reintento con radio liberada por el owner)

1. **Preflight pasivo** (8 muestras `get` cada ~14 s, `logs/preflight-monitor2.log`): `tx=false`,
   `app=""`, AM 7.355 MHz, vol 1, SSH/ping OK **en las 8** — ventana RX estable.
2. **Build ARM temporal** con el árbol autorizado (`rom-build/clone-buildroot/build` 19 G +
   `rom-build/clone-gui`): parche temporal **OFF por defecto** activado por flag no persistente
   `/tmp/x6200_reg12_probe.enable`; `make x6200-gui-dirclean && make -j8 x6200-gui` → **exit 0**;
   GUI ARM `sha256 25f281b05bfd10990c47d6f38d369af93171c6bdaf751161987b728b121ac19d`
   (14.006.244 B). `clone-gui` y el binario del build tree se **restauraron** a su estado previo
   (hashes en `logs/clone-gui-*.txt`).
3. **Backup/rollback del binario de radio probado antes del deploy**:
   `/mnt/ui-bak-rprime-20261030-031717/x6200_gui` (sha256 `4ee87aef…66b0` verificado; copia de rollback
   probada con hash idéntico).
4. **Deploy sólo GUI temporal** (`/usr/sbin/x6200_gui` sha `25f281b0…`; nunca rootfs/daemon/libs/params),
   flag en `/tmp`, **una sola ejecución**: `kill` de la GUI ⇒ el daemon la relanza en ~5 s ⇒
   `READBACK_PROBE_DONE` capturado ⇒ flag **eliminado de inmediato** (sin segunda pasada).
5. **Restauración inmediata** del binario original (`4ee87aef…66b0`), `kill` ⇒ GUI nueva pid **15470**,
   `tx=false`/`radio=RX` verificado, temporales de `/tmp` borrados (`logs/restore.txt`,
   `logs/final-state-retry.txt`).
6. **Sin efectos:** no se conmutó el MUX, no hubo escrituras de registros, ni TX/PTT/ATU, ni cambios de
   modo/VFO/frecuencia/volumen/mixer/BT por parte del agente; la radio quedó con su GUI original.

## Lo que se entrega

- `protocol.md`: protocolo exacto (una pasada, 0x2000/1B, 0x0038/4B, 0x0030/4B, segunda lectura única
  tras 50 ms, máx 5 lecturas, sin reintentos/barrido, sin escritura de valores, OFF por defecto vía
  flag `/tmp`, gate RX rechequeado por transacción, único dueño GUI antes de `radio_thread`).
- `logs/probe-patch.diff`: diff completo del parche temporal usado (módulo `rx_reg12_probe.[ch]` +
  hook en `src/radio.c` + `src/CMakeLists.txt`), **no integrado al PR** (el PR es sólo docs).
- `logs/host-mocks-flag.txt`: **33 comprobaciones, 0 fallos** (flag OFF por defecto sin abrir bus,
  control fallido aborta, timeout sin reintento, `tx=true`/writer concurrente abortan antes del bus,
  trivial `00/FF` ⇒ INCONCLUSIVE, inestable ⇒ INCONCLUSIVE, sólo direcciones/longitudes permitidas,
  0 payloads de escritura).
- `readback-values.csv`: las 5 lecturas reales con bytes/rc/tiempos/sombra.
- `rollback.md`: backup/rollback **ejecutados** y verificados (hashes antes/después).
- `gates.csv`, `commands.log`, `logs/`.

## Limitaciones declaradas

1. **No hay verdad física** de que la lectura corresponda al registro real: el control conocido
   (`rxvol=1`) no se reflejó (`0`). Sin instrumentación presencial no se puede discriminar entre
   "el MCU devuelve ceros" y "esa ventana no es de lectura".
2. La prueba fue un **único intento** por diseño: no se probaron más direcciones ni reintentos.
3. La ventana de deploy implicó ~5 s de reinicio de GUI (autorizado por el owner); sin TX/PTT.
4. El veredicto máximo posible sin instrumentación física es `R_INCONCLUSIVE`/`R_PRIME_PHYSICAL_NOT_VERIFIED`.

## Estado final de la radio (verificado)

`tx=false`, `radio=RX`, AM 7.3550 MHz, vol 1, `app=""`; GUI sha256 `4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0`
(md5 `80bbe5bd70b3…`), pid 15470; temporales `/tmp/x6200_reg12_probe*` y `/tmp/x6200_gui.rprime`
eliminados; backup original en `/mnt/ui-bak-rprime-20261030-031717/`; **MUX nunca conmutado**.

## Qué queda

`R_INCONCLUSIVE` **no desbloquea nada**: #362/#363/#367 no se cierran y #364/#365 siguen bloqueados.
La siguiente vía es **instrumentación física presencial** (medir el estado del MUX/PA con el equipo en
mano), no otra ronda de lectura remota; opcionalmente, documentación del MCU/FPGA que especifique una
ventana de lectura con semántica conocida.
