# Evidencia — issue #231: `S02update` se re-ejecuta a sí mismo (guardia de re-entrada)

- **Fecha:** 2026-10-22
- **App:** `iariap/x6200_gui` — rama de trabajo `factory/fork-bomb-231`
- **Base medida:** `0e37b9c1` (= tip de `origin/lu2dsx/x6200-rom` al iniciar el worktree)
- **Contrato:** [iariap/x6200_gui#231](https://github.com/iariap/x6200_gui/issues/231). Relacionado: #230 (cotas en la ruta Docker del arnés A/B), #215/#211 (updater verificado).
- **Repos:** `iariap/x6200_gui` (este) + `iariap/AetherX6200Buildroot` (privado, `factory/fork-bomb-231`, base `e5a6393` = tip de `lu2dsx/x6200-rom`).
- **Alcance:** el updater del target (un solo archivo, byte a byte igual en los dos puntos de instalación) + el banco `tests/release/`. Sin push, sin radio, sin SD del owner, sin TX.
- **Estado:** implementado y verificado en contenedores propios con cota. **Sin fusionar.**

## 1. Qué cambió (y qué no)

| Repo | Archivo | Cambio |
| --- | --- | --- |
| `x6200_gui` | `tools/release/update-overlay/etc/init.d/S02update` | guardia de re-entrada del pase: la marca `S02UPDATE_PASS` (en el entorno, con el estado del pase ya corrido) hace que una re-entrada no toque nada y salga con ese estado. Sin marca, un arranque nuevo corre normal. Tres tramos: cabecera documentando el contrato, la marca al lado de `MNT`/`WORK`, y el corte al principio de `start()`. |
| `x6200_gui` | `tests/release/test_safe_update.sh` | caso nuevo `updater-reentry` (+ helper `reentry_updater`), cotas de recursos en el `docker run` del banco (`--pids-limit`, `--memory`, `--memory-swap`; defaults 512 y 6g, override `X6200_LAB_PIDS_LIMIT` / `X6200_LAB_DOCKER_MEMORY`, mismos nombres que el arnés A/B), y la línea del contrato en la cabecera. |
| `x6200_gui` | `tests/release/README.md` | fila del caso nuevo y la nota de cotas. |
| `AetherX6200Buildroot` | `br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update` | el mismo archivo, byte a byte (imagen SD nueva). |

No se tocó: el contrato de la actualización (sidecar `sha256`, `gzip -t`, preflight de nombres/tipos/duplicados/puntos de montaje/inventario setuid, `/mnt` montaje propio y escribible, el backup verificado y renombrado, la lista de rutas nuevas, la extracción con `pipefail`, la verificación del árbol instalado, el consumo del paquete y el sidecar), las cuatro líneas de estado, ni el código de la GUI.

## 2. Causa raíz y nivel elegido

**El `S02update` es el archivo que la propia actualización reemplaza.** El paquete que se está aplicando lleva dentro el `etc/init.d/S02update` nuevo y la extracción lo escribe encima del que está corriendo. Un shell lee su script **por offset sobre el mismo fd** (no lo re-abre por path): si el archivo cambia bajo sus pies, sigue leyendo desde donde iba. Si el contenido instalado es **más largo** que el offset donde terminaba el script viejo, el shell **re-parsea la cola del script recién instalado y la ejecuta**: funciones redefinidas y, al final, el mismo `case "$1" in start) start;; ...` ⇒ **el update re-entra en sí mismo** (issue #231: "S02update se re-ejecuta a sí mismo"). Cada pasada vuelve a extraer el paquete y a volver a leer el archivo, así que **por diseño la re-entrada no tiene tope propio**.

Mediciones que sostienen el diagnóstico (salida cruda en `repro/salida-repro.txt`):

1. **El shell lee por offset (reproduce la re-entrada).** En un contenedor privado, `bash` corriendo un script que se reescribe *en el lugar* durante su propia corrida re-parsea el contenido nuevo desde el offset viejo: se ve la cola del script instalado ejecutándose (y, si esa cola llega al despacho, un pase de actualización entero). Con la re-entrada forzada a la forma del bug (la cola del script instalado, con la marca del pase que ya corrió):
   - **base `0e37b9c1` (sin guardia):** `rc=1`, `stdout=[Update verification failed]`, `stderr=[... reason: the package has no ... sidecar next to it]` ⇒ **corrió un pase entero de nuevo**.
   - **worktree (con guardia):** `rc=0`, `stdout=[]` ⇒ **no tocó nada** y salió con el estado del pase que ya corrió.
2. **El camino `tar` no re-lee.** El tar del target (`/bin/tar` → BusyBox de la ROM, verificado en el chroot) **reemplaza el inodo** (medido: `inode=70404855` → `inode=70404858`), así que por ese camino el shell queda leyendo el inodo viejo ya deslinkeado y no hay re-lectura. Por eso el banco actual **no** reproduce el síntoma: tres corridas completas (una con cota de 3g — que además dejó 2 casos de `/mnt` sin espacio, artefacto de esa cota — y dos con 6g, "antes" y "después" de §4) terminan con **pico de 13 procesos** (`pids.current`, cota 512), sin escalado.
3. **Nivel elegido: el updater, no el banco.** El defecto es de producción: el offset de re-lectura depende del **largo del `S02update` anterior**, que este archivo no controla (cada versión histórica tiene otro largo: el stock del fixture 428 B, el publicado 1869 B, el nuevo 23 810 B). Ningún cambio en el banco evita la re-entrada; lo único fail-closed es que la re-entrada **no corra el update**, y eso vive en el archivo que la radio ejecuta ⇒ en los dos puntos de instalación (GUI + Buildroot), con paridad.

### Lo que NO se pudo reproducir (BLOCKED, con lo que sí se midió)

El **escalado sin tope** (la cadena de 2502 procesos y los 70 GB del reporte) **no se reprodujo** con los archivos actuales: el banco completo no escala (2 corridas acotadas, pico 13 y 7 procesos) y el mecanismo que sí reproduje queda **acotado a un pase extra** en las condiciones que pude construir. Detalle de por qué:

- Con `tar` la extracción **reemplaza el inodo**: el fd del shell queda con el contenido congelado del script viejo ⇒ a lo sumo el shell termina en EOF (sin re-lectura), o sale en silencio con el estado del último comando si el contenido nuevo quedó **más corto** que su offset (segundo hallazgo, §7).
- Para que la cadena se repita, el archivo tiene que reescribirse **en el lugar** una y otra vez; nada del camino actual (tar, `install`, `mv`, `cp`) escribe el updater en el lugar, así que no encontré un camino de código que lo haga sin tope.

Queda como **pendiente para el humano**: el entorno/estado exacto de la corrida donde se midió la cadena de 2502 procesos (¿imagen/`tar` distinto? ¿un `sh` del target distinto de bash?). Lo que sí queda cubierto con este cambio: (a) la re-entrada **no puede volver a correr el update** (guardia, fail-closed), y (b) **el banco ya no puede llevarse el host** aunque algo vuelva a escalar (cotas), que son las tres direcciones que pide el issue.

## 3. Repro acotado (con cota de PIDs y de RAM)

Harness: `repro/repro-231.sh` (contenedores privados y desechables, `--pids-limit=64`, `--memory=--memory-swap=512m`; no toca los contenedores/volúmenes compartidos). Salida cruda: `repro/salida-repro.txt`.

```
== 0. cotas del contenedor y contención de un fork bomb
pids.max=64  memory.max=536870912
bash: fork: retry: Resource temporarily unavailable        <- el lazo se corta en la cota
bash: fork: Resource temporarily unavailable
== 1. el tar del target reemplaza el inodo
antes:  inode=70404855 size=1869
después:inode=70404858 size=25587  <- inodo nuevo = reemplazado
== 2. la re-entrada: el shell re-parsea [L, fin) del script recién instalado
--- base: cola de 21941 bytes, desde el offset 1869
    rc=1  stdout=[Update verification failed]  stderr=[... reason: the package has no ... sidecar next to it]
--- after: cola de 23718 bytes, desde el offset 1869
    rc=0  stdout=[]                             stderr=[]
```

Cómo se acota y por qué es seguro correrlo: el límite de PIDs se verifica **antes** de lanzar nada (`pids.max`/`memory.max` leídos del cgroup, más 200 forks de prueba que rebotan con EAGAIN) y el `memory-swap` igual a `--memory` evita el fallback a swap. Host medido antes/después de las corridas del banco: sin cambios relevantes.

## 4. El banco (matriz del updater) antes/después

`tests/release/test_safe_update.sh <fixture> <lab>` — misma matriz en las dos corridas; lo único que cambia es el `S02update` (el de la base vs. el del worktree). La corrida "antes" usa una copia del árbol con el test nuevo y el updater de `0e37b9c1`; la "después" corre sobre el worktree.

`tests/release/test_safe_update.sh <fixture> <lab>` — misma matriz en las dos corridas; lo único que cambia es el `S02update` (el de la base vs. el del worktree). La corrida "antes" usa una copia del árbol con el test nuevo y el updater de `0e37b9c1`; la "después" corre sobre el worktree. Las dos con las cotas nuevas del banco (512 pids / 6g).

| Corrida | `S02update` | Resumen | `updater-reentry` |
| --- | --- | --- | --- |
| antes (`0e37b9c1`) | sin guardia | `PASS: 26   FAIL: 1   UNSUPPORTED/UNSAFE_LEGACY: 1` | **FAIL** |
| después (worktree) | con guardia | `PASS: 27   FAIL: 0   UNSUPPORTED/UNSAFE_LEGACY: 1` | PASS (12/12) |

Caso `updater-reentry` crudo en las dos corridas (es el que reproduce el bug):

```
antes (sin guardia) — la re-entrada corrió el pase entero:
ok   rc=0
FAIL la re-entrada no debe escribir ninguna línea de estado y dice 'Update applied|'
ok   stderr vacío
FAIL el paquete x6200_r1cbu_v215.tgz desapareció
FAIL el sidecar de x6200_r1cbu_v215.tgz desapareció
ok   nada bajo / cambió
ok   la GUI quedó la de antes
FAIL quedó 1 archivo(s) backup_* en /mnt
ok   rc=0
FAIL stdout no dice 'Update applied'          <- el arranque nuevo ya no tenía paquete: lo consumió la re-entrada
ok   el paquete x6200_r1cbu_v215.tgz se consumió
ok   el sidecar de x6200_r1cbu_v215.tgz se consumió

después (con guardia) — la re-entrada no tocó nada y el arranque nuevo actualizó normal:
ok   rc=0
ok   la re-entrada no escribe ninguna línea de estado
ok   stderr vacío
ok   el paquete x6200_r1cbu_v215.tgz sigue en /mnt
ok   el sidecar de x6200_r1cbu_v215.tgz sigue en /mnt
ok   nada bajo / cambió
ok   la GUI quedó la de antes
ok   no quedó backup ni .partial
ok   rc=0
ok   stdout dice 'Update applied'
ok   el paquete x6200_r1cbu_v215.tgz se consumió
ok   el sidecar de x6200_r1cbu_v215.tgz se consumió
```

Recursos de la corrida "después" (muestreados del cgroup del contenedor desde el host cada 3 s): pico `pids.current=13` (cota 512) y pico `memory.current` 3,25 GB (cota 6g) ⇒ el banco completo entra cómodo en las cotas y no escala en procesos. La corrida "antes" no agrega casos fuera de `updater-reentry`: los 26 PASS son los mismos casos que ya validaba la épica #215.

## 5. Gates

| Gate | Comando | Resultado |
| --- | --- | --- |
| Sintaxis | `bash -n tools/release/update-overlay/etc/init.d/S02update` y `sh -n` | OK |
| Sintaxis | `bash -n tests/release/test_safe_update.sh` | OK (también vía matriz: `[check] bash -n tests/release/test_safe_update.sh ok`) |
| Paridad del updater | `tools/release/check_updater_parity.sh <clon Buildroot>` | `9e3ad96b5731e8a45aff9376455d8ac22a55f839d0831f661a6b787cd858ecab` en los dos ⇒ **paridad OK** |
| Regla de tipos/enlaces | `python3 tests/release/check_types_rule.py <updater>` | `19/19 casos de la tabla de tipos/enlaces OK` |
| Matriz del harness | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | rc=0 (`x6200-ab themes` ok, `contracts` ok, `bash -n` del banco ok; nativo/wcag/build-identity/harness-determinism/QEMU con `SKIP` por paths) |
| Auditoría de pruebas | `python3 tests/audita-pruebas.py` | `OK: estructura y evidencia en orden` (rc=0) |
| Cobertura de diálogos | `python3 tests/check_dialog_coverage.py` | `OK: 22 dialogos inertes ... sobre las 25 apps del barrido` (rc=0) |
| Tests nativos | `sh tests/native/run_native_tests.sh` | `36 comprobaciones, 0 fallos` |

## 6. SKIP / BLOCKED razonados

- **BLOCKED — el escalado sin tope (fork bomb) no se reprodujo** con los archivos actuales: dos corridas completas del banco acotadas (pico 13 y 7 procesos) y el mecanismo reproducido queda en un pase extra. Ver §2. Se documenta en vez de forzarlo: forzar la condición (reescribir el archivo en el lugar con un `cat >`, que es lo que el harness del repro hace para el "antes/después") **no** es lo que hace el camino real de instalación (medido: `tar` reemplaza el inodo).
- **SKIP — sin SSH a la radio, sin flash, sin TX, sin SD del owner**: fuera de alcance por el issue y por las reglas de la corrida. La verificación en radio del primer update in-place queda `PENDING_HARDWARE` (es donde corren las cotas reales del target).
- **SKIP — `--privileged` sigue siendo necesario** en el banco (chroot + montajes); las cotas `--pids-limit`/`--memory` se verifican igual dentro de un contenedor privilegiado (`pids.max`/`memory.max` leídos del cgroup).

## 7. Hallazgos y pendientes (no arreglados acá, para no ampliar el cambio)

1. **Salida temprana en silencio (fail-open), si el archivo instalado queda más corto que el offset del shell.** Con el mismo mecanismo de §2, si el contenido nuevo es más corto, el shell lee EOF en su offset y termina **sin** ejecutar el resto del script, con el estado del último comando (podría ser `0`). Con `tar` el inodo se reemplaza, así que hoy no es alcanzable por el camino de la extracción; queda anotado como la cara opuesta del mismo defecto (la re-entrada ya está acotada por la guardia, pero la salida silenciosa no tiene guardia propia).
2. **Un directorio temporal por re-entrada.** La cola re-parseada re-ejecuta `WORK=` y `trap cleanup EXIT` antes de llegar al despacho, así que el `mktemp -d` del pase original queda huérfano (uno solo: la re-entrada corta y sale). No se toca: arreglarlo requiere que la guardia actúe antes del parse de la cola, que no está bajo control del script.
3. **Alternativa de diseño (no implementada): correr el pase desde una copia de sí mismo** (`exec /bin/sh <copia> "$@"`). Elimina la causa (el shell nunca lee el archivo que la actualización reemplaza) y también el hallazgo 1, pero cambia `$0` (la línea de uso), agrega un archivo temporal y un `exec`: más superficie que la guardia y fuera de "fix mínimo". Queda propuesta para decisión humana.

## 8. Cómo se reprodujo todo

```bash
WT=/home/pablo/desa/asf/state/worktrees/x6200/fork-bomb-231
FX=/home/pablo/desa/x6200/images/x6200_r1cbu_v0.1.2-lu2dsx.tgz

# repro acotado (contenedores privados, cotas 64 pids / 512m)
$WT/docs/evidencia/2026-10-22-forkbomb231/repro/repro-231.sh "$WT" "$FX" 0e37b9c1

# matriz del banco (dentro de un contenedor con cotas 512 pids / 6g)
$WT/tests/release/test_safe_update.sh "$FX" /tmp/lab-231

# paridad del updater con la copia de Buildroot
$WT/tools/release/check_updater_parity.sh /home/pablo/desa/asf/state/worktrees/x6200/fork-bomb-231-buildroot
```

Sin residuos: los contenedores del repro y del banco se eliminan al terminar (verificado con `docker ps -a`; el banco usa `--rm`).
