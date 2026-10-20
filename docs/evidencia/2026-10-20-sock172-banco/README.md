# Evidencia #172 — el socket de control no se le roba a una instancia viva

Fecha: 2026-10-20. Rama: `factory/remote-sock-172`.
**HEAD**: `8e5e3eb4a3819face165dc59fa14e7f928883635` (fix del socket + TDD).
**Base**: `39539f9360611c0c4ac8c9f07c66a277ad159667` (trunk tip al arrancar).
Binario ARM del banco: `md5 d2afdf771fd8bd0c11b2315e70f56bd6` (13.869.700 B), sello de
fuentes `819579203ccd94ff257d8d4d6eefcb8f` (el mismo que embebe el binario, #165).

---

## 1. Estado actual, con líneas

### Quién hace el `unlink` del path del socket

`remote_server_init()` en `src/remote_server.cpp`. En la base (commit `39539f93`) era:

```
1135:    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", REMOTE_SOCK);
1136:    unlink(REMOTE_SOCK);                     <-- a ciegas, sin mirar si hay un servidor vivo
1137:    listen_fd = socket(AF_UNIX, ...);
1138:    if (listen_fd < 0 || bind(...) < 0 || listen(listen_fd, 8) < 0) { ... }
```

`REMOTE_SOCK` es `/tmp/x6200_remote.sock` (`src/remote_server.cpp:96`). `remote_server_init()`
lo llama `main()` una sola vez al arrancar (marca `control remoto` del `boot_mark`).

Con dos GUI vivas: la segunda instancia corre su `remote_server_init()`, hace `unlink()` del
path de la primera y `bind()` el suyo. Si después esa segunda muere, el path queda apuntando
a un **inodo muerto** (ECONNREFUSED) mientras la primera sigue viva con su listener
**desvinculado** (nadie llega por el path). Es exactamente lo medido en el issue.

### Quién lanza la segunda GUI

La GUI la supervisa el daemon, que es un `while true` que la relanza apenas muere:

```
rootfs/usr/sbin/x6200_daemon:14-16
14: while true; do
15:     /usr/sbin/x6200_gui > /tmp/x6200_log.txt 2>&1 || sync; mv ...
16: done
```

El daemon lo arranca `S20gui` con **un solo** `start-stop-daemon`:

```
rootfs/etc/init.d/S20gui:25
25: start-stop-daemon -S -b -q -m -p "$PIDFILE" -x "/usr/sbin/$DAEMON"
```

**El relanzador puede lanzar dos.** `start-stop-daemon -S -p PIDFILE` decide si ya hay un
daemon **por el pidfile**, y el pidfile puede estar **stale**. Medido en la radio el
2026-10-20 15:35 UTC:

```
$ cat /var/run/x6200_daemon.pid     -> 747
$ ps | grep x6200_daemon            -> 298  {x6200_daemon} /bin/sh /usr/sbin/x6200_daemon
$ ls -d /proc/747                   -> No such file or directory      (el pidfile apunta a un muerto)
```

Y el comportamiento de `start-stop-daemon` (reproducido en la radio, con un `sleep` de prueba
en vez de la GUI, para no romper nada):

```
# pidfile FRESCO (el proceso vive): 2do arranque rc=1, UN solo proceso
# pidfile STALE  (pid 99999, muerto): 2do arranque rc=0, DOS procesos
start-stop-daemon -S -b -q -m -p /tmp/ssdtest.pid -x /bin/sleep -- 23456   # -> 12836
echo 99999 > /tmp/ssdtest.pid
start-stop-daemon -S -b -q -m -p /tmp/ssdtest.pid -x /bin/sleep -- 23456   # rc=0 -> 12839
ps | grep "[s]leep 23456"  ->  12836 ... y 12839 ...      <-- DOS daemons
```

O sea: con el pidfile stale, **cualquier segunda invocación de `S20gui start` arranca un
segundo daemon** → segunda GUI → la segunda le roba el socket a la primera.

### La segunda invocación de `S20gui start` (evidencia en la radio)

`/mnt/app_logs/boot.log` del boot del incidente (03:19 UTC) tiene **dos** líneas
`GUI arrancando` en el **mismo boot**, 28 s aparte y con el uptime avanzando:

```
15341: 03:19:26  kernel listo: 3.22 s de uptime
15342: 03:19:28  GUI arrancando: 5.16 s de uptime      <-- S20gui start
15343:   GUI inicio     5.9 s de uptime
15374: GUI lista: 8.1 s
15375:   GUI inicio     27.0 s de uptime                <-- otra GUI
15406: GUI lista: 29.3 s
15407: 03:19:56  GUI arrancando: 34.35 s de uptime      <-- S20gui start OTRA VEZ (mismo boot)
15408:   GUI inicio     34.5 s de uptime               <-- la segunda GUI
15439: GUI lista: 36.1 s
```

`GUI arrancando` **solo** lo escribe `S20gui` (`rootfs/etc/init.d/S20gui:22`). Y
`boot_scripts.log` prueba que **`rcS` corrió `S20gui` una sola vez por boot** (23 boots, 23
`inicio S20gui`), así que la segunda invocación **no** vino de `rcS`: fue un
`/etc/init.d/S20gui start` (o `restart`) suelto sobre un pidfile stale. Con eso quedan dos
daemons y dos `x6200_gui`, que es el `pidof x6200_gui -> 752 687` del issue.

**Causa de fondo**: pidfile stale + `start-stop-daemon` confiando en él ⇒ segundo daemon ⇒
segunda GUI. **Estado: propuesta** (no aplicada, ver §5).

---

## 2. Fix elegido (el pedido principal)

`src/remote_server.cpp` — antes de tocar el path, **se sondea**:

- `remote_sock_should_unlink(exists, attends)`: decisión **pura**, sin syscalls.
  - path no existe → `true` (unlink es no-op; bind normal)
  - existe y **alguien atiende** → `false` (instancia viva: **NO** robarle el socket)
  - existe y nadie atiende (huérfano) → `true` (unlink + bind, como hoy)
- `remote_sock_attends(path, &exists)`: la sonda real. `stat()` para ver si existe y
  `connect()` (socket no bloqueante) para ver si hay alguien del otro lado. `connect()` a un
  AF_UNIX muerto da `ECONNREFUSED`; **no** hay `SIGPIPE` (SIGPIPE es de `write()`). Si sale
  `EINPROGRESS` (backlog lleno), un `poll` de 250 ms + `SO_ERROR` decide, para no colgar el
  arranque.

`remote_server_init()` ahora: si `!remote_sock_should_unlink(...)` → loguea
`LV_LOG_WARN` y **sale sin arrancar el servidor** (la instancia viva conserva el socket).
Si no, `unlink` + `bind` + `listen` como siempre.

**Por qué "yield" y no un path alternativo (`/tmp/x6200_remote-<pid>.sock`)**:

- El **contrato de los clientes no cambia**: `tools/remote/x6200-remote` y el QA hablan
  siempre `/tmp/x6200_remote.sock`. Con un path alternativo, la segunda instancia tendría un
  socket que **nadie** conoce; el cliente seguiría yendo al de la primera. El path alternativo
  agrega un socket muerto y no aporta.
- Con "yield" el socket de la instancia viva queda intacto y **el mismo path** sigue
  atendiendo: los clientes existentes no se enteran de que hubo una segunda instancia.
- El fix de fondo (una sola GUI) es el que elimina la duplicación; el del socket es la red de
  seguridad para que, pase lo que pase, **el control remoto no se caiga**.

**Estado intermedio si el `bind` falla**: no se hace `unlink` después de un `bind` fallido. Si
falló con `EADDRINUSE`, otra instancia ganó la carrera y el path es suyo: robarle el socket
sería reintroducir el bug. Si `bind` sí fue nuestro y falla `listen`, ahí sí se limpia el
archivo (no dejamos un huérfano).

---

## 3. TDD (RED → GREEN, salidas reales)

Suite `tests/native/test_remote.cpp` (se compila incluyendo el `remote_server.cpp` staged,
por eso ve las funciones estáticas). Casos nuevos:

- **Puro** (`remote_sock_should_unlink`): no existe / existe y atiende / existe y huérfano
  (más el ausente-con-atiende, que no depende del flag).
- **Sonda real** (`remote_sock_attends`): crea un listener AF_UNIX vivo (connect OK), un
  **huérfano** (bind+listen+close: el archivo queda, `ECONNREFUSED`), un path **ausente** y un
  archivo **que no es socket**. Verifica `exists`/`attends` y la decisión.

- `tdd-red.log`: el test contra el `remote_server.cpp` de la base (`39539f93`) **no compila**:
  el seam (`remote_sock_should_unlink`/`remote_sock_attends`) no existe todavía.
  `error: ‘remote_sock_should_unlink’ was not declared in this scope` (exit 1).
- `tdd-green.log`: el mismo test con el fix: **138 comprobaciones, 0 fallos** (exit 0).

---

## 4. Gates (salida real)

| Gate | Resultado |
|---|---|
| Suite nativa completa + `-fanalyzer` | **exit 0**, 0 leaks (`native-tests.log`; bloque remote: 138/0) |
| Estáticos de la app (10) | 10/10 ok (`check-matrix.log`, exit 0) |
| Determinismo del arnés de UI | **PASS**, PNG idénticos por fixture (`determinism.log`) |
| A/B `AB-SOCK172` (`39539f93` vs `8e5e3eb4`) | **PASS**, 112 escenarios **pixel-idénticos**, 1147 contratos (`ab-sock172.log`, `ab-sock172-report.md`) |
| `contracts` | PASS — 1147 checks, 0 fallos (`contracts.log`) |
| `audit` | 11 issues conocidos, **0 reproducidos**; 12353 objetos, 0 hallazgos estructurales (`audit.log`) |
| `themes` | PASS (WCAG 2.1 AA sobre las paletas servidas; `themes.log`) |
| `ruff check` / `ruff format --check` / `ty check` (A/B framework) | todo OK (`ruff-format-ty.log`) |
| `pytest` (unit) | **227 passed**, 13 deselected (`pytest.log`) |
| `pytest -m e2e` | **13 passed** (`pytest-e2e.log`; `TMPDIR` en disco) |
| Cross-compile ARM (toolchain buildroot 12.3.0) | ok, sello `819579203ccd` = el del binario (`arm-cross-build.log`); gate #86 OK |
| Banco QEMU: **dos instancias** | **PASS** (`banco-transcript.txt`) |

**Pixel-idéntico: sí.** El arnés de UI no compila `remote_server.cpp` (es el protocolo del
socket, no píxeles), así que el A/B es PASS con capturas idénticas. El gate visual no aplica.

---

## 5. Causa de fondo (dos instancias): PROPUESTA

No se aplicó porque toca el **rootfs** (`S20gui`/`x6200_daemon`), que **el deploy por WiFi no
actualiza** (solo copia el binario y las libs; ver `.agents/qa-radio/SKILL.md`), y porque
cambiar el arranque es de riesgo alto (un error deja la radio sin GUI). El fix del socket ya
hace que el síntoma (control remoto muerto) no ocurra aunque haya dos instancias.

Patch propuesto (fail-open: si `pidof` no está, arranca como hoy):

```sh
# rootfs/etc/init.d/S20gui, en start():
start() {
    if pidof x6200_daemon >/dev/null 2>&1; then
        printf 'Starting %s: ' "$DAEMON"; echo "ya corriendo"; return 0
    fi
    ...   # el resto igual
}
```

Alternativa (o además) en el daemon: `flock -n` sobre un lockfile al entrar al `while true`
(la radio tiene el applet `flock` de busybox). Las dos requieren **flash de imagen** para
llegar al equipo, no deploy por WiFi.

Evidencia de la causa: §1 (pidfile stale + `start-stop-daemon` con pidfile stale arrancando un
segundo proceso, reproducido en la radio; dos `GUI arrancando` en un boot; `rcS` corriendo
`S20gui` una sola vez por boot).

---

## 6. Banco QEMU (sin radio): dos instancias

`correr.sh` corre el binario ARM real (`d2afdf77`) en un chroot **privado**
(`/home/pablo/desa/x6200/.vsock172`, reflink de `.vfix3`) y un volumen **privado**
(`x6200-build-output-dfl173`, no el compartido `x6200-build-output`).

`banco-transcript.txt`:

1. Arranca la instancia 1, handshake `{"cmd":"get"}` → contesta la state.
   Inodo del path `29703197`, listener `/proc/net/unix` `298855182`.
2. Lanza una **segunda** instancia de la GUI en el mismo chroot (el relanzador duplicado).
3. **Dos** `x6200_gui` corriendo (`111` y `7`), y sin embargo:
   - el **inodo del path NO cambia** (`29703197`),
   - el **listener de `/proc/net/unix` es el mismo** (`298855182`),
   - el cliente **sigue conectando y contestando** `{"cmd":"get"}`.
4. La instancia 2 loguea el yield:
   `[Warn] remote_server_init: remote: /tmp/x6200_remote.sock ya tiene un servidor vivo: esta
   instancia no arranca el servidor (in remote_server.cpp line #1199)`.

**No medible sin radio**: que el arranque real del equipo (el daemon relanzando tras un deploy)
produzca las dos instancias; eso es `requires-radio`.

---

## 7. requires-radio (pendiente, no ejecutado)

En la radio con el binario de este commit desplegado (`tools/prueba-gui-en-radio.sh`, que corre
antes el gate #86) y **con autorización del owner** (una segunda GUI dibuja sobre `/dev/fb0`:
parpadeo momentáneo):

1. Estado sano: `pidof x6200_gui` (un pid), `ls -i /tmp/x6200_remote.sock`, `grep x6200_remote
   /proc/net/unix` (inodo del listener), `/tmp/x6200-remote '{"cmd":"get"}'` (contesta).
2. Reproducir el disparador: `/etc/init.d/S20gui start` con el daemon ya vivo (con el pidfile
   stale, como está hoy, arranca un **segundo** daemon → segunda GUI).
3. Verificar con el fix: `pidof x6200_gui` da **dos** pids, pero el inodo del path y el
   listener de `/proc/net/unix` **no cambiaron**, y `/tmp/x6200-remote '{"cmd":"get"}'` sigue
   contestando. En el log de la segunda instancia tiene que estar `servidor vivo`.
4. Cerrar la segunda instancia (`kill <pid2>`); el socket del path tiene que **seguir
   atendiendo** (sin el fix, moriría: es el síntoma del issue).
5. Dejar la radio como estaba (una sola GUI, en RX).

---

## 8. Archivos

| Archivo | Qué es |
|---|---|
| `tdd-red.log` | test nuevo contra `remote_server.cpp` de la base: no compila (seam ausente) |
| `tdd-green.log` | el mismo test con el fix: 138/0 |
| `native-tests.log` | suite nativa completa + `-fanalyzer` |
| `check-matrix.log` | matriz de checks de x6200 (estáticos + nativos + banco) |
| `determinism.log` | gate de determinismo del arnés de UI |
| `ab-sock172.log`, `ab-sock172-report.md`, `ab-sock172-metadata.json` | A/B `AB-SOCK172` |
| `contracts.log`, `audit.log`, `themes.log` | contratos, auditoría y contraste de temas |
| `pytest.log`, `pytest-e2e.log`, `ruff-format-ty.log` | pytest unit + e2e; ruff/format/ty |
| `arm-cross-build.log` | cross-compile ARM (sello de fuentes #165) |
| `banco-transcript.txt`, `out-banco.txt`, `out-banco-2.txt`, `correr.sh` | banco QEMU, dos instancias |
