# Banco privado del camino de actualización (issue #215, épica #211)

Banco **hermético** del contrato de actualización verificada: corre el
`S02update` real dentro de un **chroot sobre una copia del rootfs de la imagen**,
con el **bash y el BusyBox ARM del target**, en un contenedor de laboratorio
privado y desechable.

Nunca toca la radio, la SD del owner, ni los contenedores o volúmenes
compartidos (`banco-apps`, `banco-audio`, `x6200-build-output`): todo pasa en
copias dentro del directorio de laboratorio que le pasás.

## Cómo se corre

```bash
tests/release/test_safe_update.sh <paquete-publicado.tgz> [directorio-de-lab]
```

El fixture es un **paquete publicado de la ROM** (`x6200_r1cbu_*.tgz`). De ahí
sale el árbol de la "imagen" (sus archivos, dueños y permisos) y el contraste
histórico. Se lo usa como copia: nada se modifica en el original.

El lanzador construye la imagen privada (`tests/release/Dockerfile`: userland
x86_64 con `qemu-user-static`) si hace falta y corre el banco dentro. Con el
contenedor corre como root porque el chroot y los montajes (`/mnt` tmpfs, `/proc`)
lo necesitan. Al terminar deja el laboratorio a tu nombre y saca el scratch
pesado.

**Cotas de recursos (issue #231):** el `docker run` del banco lleva
`--pids-limit` (default 512, `X6200_LAB_PIDS_LIMIT`) y `--memory`/
`--memory-swap` (default 6g, `X6200_LAB_DOCKER_MEMORY`), los mismos nombres que
el arnés A/B. El banco corre el `S02update` del target, que puede re-ejecutarse a
sí mismo: sin cota, un lazo de procesos se lleva la RAM y el slab **del host**
(medido en el issue: 70 GB, 2502 procesos) y tumba la máquina. Con las cotas el
lazo se corta por fork (EAGAIN) y la memoria del cgroup queda acotada.

Sin fixture (o sin `docker`) imprime `SKIP:` y sale **77**, que
`tests/run_all_tests.sh` cuenta como omitido, nunca como pase.

## Qué mide

| Caso | Condición | Esperado |
| --- | --- | --- |
| `types-rule` | la regla de tipos y de targets de enlaces, contra la tabla de `check_types_rule.py` | 19/19 (los `..` que quedan adentro del rootfs y los target absolutos se admiten; los `..` que se salen, no) |
| `good-first-apply` | imagen auditada + paquete bueno con sidecar | backup verificado, GUI del paquete, setuid intactos, updater root:root 0755, paquete y sidecar consumidos |
| `good-added-paths` | el paquete agrega archivos | se instalan y quedan en `/mnt/new_*.list`, **no** en el backup |
| `subsequent-apply` | dos paquetes en `/mnt` | se aplican en orden, un backup por paquete |
| `sidecar-missing` / `sidecar-bad-hash` / `sidecar-wrong-name` | sin sidecar / hash malo / nombre de otro archivo | `Update verification failed`, nada bajo `/` cambia |
| `truncated-tgz` | tgz cortado (con sidecar correcto) | `gzip -t` lo detecta, no se toca nada |
| `absolute-entry` `dotdot-entry` | nombre absoluto o con `..` (el tar del target los recorta y lo avisa) | rechazo antes de tocar `/` |
| `dot-entry` `duplicate-entry` | `./` adelante, entrada duplicada | rechazo |
| `forbidden-mnt` `forbidden-dev` `forbidden-tmp` | entradas que reemplazarían el punto de montaje `/mnt` `/dev` `/tmp` | rechazo |
| `fifo-entry` `device-entry` | nodos en el paquete | rechazo |
| `unexpected-setuid` | setuid de más | rechazo (inventario exacto de la imagen) |
| `symlink-dotdot` | symlink relativo que se sale del rootfs | rechazo |
| `symlink-absolute-admitted` | symlink con target absoluto (la rootfs auditada los trae: `etc/alsa/conf.d/*`) | se admite y se aplica |
| `updater-uid-1000` | `S02update` con uid 1000 (el bug del empaquetador viejo) | rechazo antes de tocar `/` |
| `updater-reentry` | la re-entrada del pase (el shell re-parseando la cola del script recién instalado, issue #231): se corre con la marca del pase que ya corrió | `S02UPDATE_PASS` no vacío ⇒ no toca nada y sale con el estado de ese pase; sin marca (arranque nuevo) el update corre normal |
| `extract-fails` | tar que falla al extraer | `Update failed: package retained; restore backup manually`, paquete y backup retenidos; el backup idéntico se reusa y un backup con otro contenido da error cerrado |
| `no-space` | `/mnt` de 48 MiB | `Update backup failed`, no se extrae |
| `readonly-mnt` | `/mnt` montado read-only | `Update backup failed` |
| `mnt-not-mounted` | `/mnt` sin montar | `Update backup failed` (el backup no puede caer en el rootfs) |
| `legacy-first-update` | imagen con el `S02update` stock | el script viejo corre igual, deja el sidecar, el backup truncado y borra el tgz: **UNSUPPORTED/UNSAFE_LEGACY** |

La última fila no es un pase de seguridad: es el escenario que el contrato pide
reportar como **no soportado**. Para migrar una imagen histórica, reimagenar la
tarjeta.

## Evidencia cruda

En `<lab>/work/`: `results.tsv` (caso → descripción → veredicto) y
`cases/<caso>/{stdout.txt,stderr.txt,steps.txt}` con la salida real del updater y
las comprobaciones de cada caso.
