# Evidencia del banco — tabs de Ajustes en inglés y ordenados por utilidad QSO (issue #109)

**Estado: BLOCKED** — el banco QEMU estaba **ocupado por otra sesión** (la corrida
hermana `factory/settings-year-110`, que corre en paralelo en otro worktree) al
momento de esta corrida. Por la regla del owner y la spec (`docs/seten109-spec.md`,
§Banco: *"Banco ocupado por otra sesión → reportá BLOCKED (no toques infra
compartida)"*) **no se tocó la infraestructura compartida**: no se reinició el
contenedor `banco-apps`, no se reescribió el chroot `/home/pablo/desa/x6200/.vfix3`
ni el volumen `x6200-build-output`.

No hay captura de pantalla de esta corrida. Abajo queda el método exacto para
repetirla cuando el banco quede libre.

## Por qué BLOCKED (evidencia)

El contenedor del banco estaba **vivo y recién (re)creado**, no estancado:

```
$ docker ps --filter name=banco-apps
banco-apps   Up ...   2026-10-16 16:08:32 -0300

$ docker inspect banco-apps --format '{{.State.StartedAt}} {{.Config.Cmd}}'
2026-10-16T19:08:32.847947883Z [sh -c chroot /t /bin/busybox env
  LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so
  /usr/sbin/x6200_gui > /t/out-banco.txt 2>&1]

$ stat -c '%y %s' /home/pablo/desa/x6200/.vfix3/out-banco.txt
2026-10-16 16:09:27  ...  61411     # el log de la GUI crecía (35 KB un minuto antes)

$ docker exec banco-apps sh -c 'ps | grep -c x6200_gui'
1                                    # la GUI del banco estaba corriendo
```

La corrida hermana estaba **en su etapa de banco** en ese mismo momento:

```
# settings-year-110 (worktree hermano, state/worktrees/x6200/settings-year-110)
2026-10-16 16:07   tools/remote/x6200-remote      # compiló el cliente del banco
2026-10-16 16:09   docs/year110-spec.md           # spec en edición (activo)
```

`banco-apps` es un recurso compartido **sin lock**: `tools/captura-banco.sh`
reutiliza el contenedor si ya está corriendo (`docker ps ... | grep banco-apps`)
y su teardown documentado es `docker rm -f banco-apps`. Manejar el contenedor
mientras otra corrida captura cambiaría el estado de la GUI (la pantalla que el
otro run está midiendo) y corrompería su evidencia. De ahí el BLOCKED.

## Método (para repetir cuando el banco esté libre)

1. **Cliente del banco** (falta en este worktree; `tools/captura-banco.sh --check`
   lo reporta):

   ```sh
   tools/remote/compila.sh          # deja tools/remote/x6200-remote (gitignored)
   ```

2. **Rebuild + captura** de la pantalla de Ajustes con el binario ARM real
   (mismo camino que `docs/evidencia/2026-10-15-fallback91/banco.txt`). El
   binario sale del firmware del port; el script corre antes el gate fail-closed
   de #86 (`tools/verifica-firmware-port.py`):

   ```sh
   CHROOT=/home/pablo/desa/x6200/.vfix3 \
   BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot \
   RECONSTRUIR=si \
   tools/captura-banco.sh /tmp/opencode/seten109-banco/ajustes-home.png
   ```

   La pantalla de Ajustes se abre con la acción `APP Settings` por el socket de
   control; para ver el grupo `Clock` (fila `Day, Month, Year`) hay que apretar el
   tab `Clock` (botón índice 3 de la página 1). El orden final de los tabs es
   `Operation · Audio · Spectrum · Clock · More >` (página 1) y
   `Display · Appearance · Actions · Info · < More` (página 2).

3. **Medición** sobre la captura (PIL, determinista): el **orden y el idioma** de
   los labels de la barra de tabs. Los ocho tabs en inglés (`Operation`, `Audio`,
   `Spectrum`, `Clock`, `Display`, `Appearance`, `Actions`, `Info`) y los dos
   botones de página (`More >`, `< More`), sin ninguna cadena en español
   (`Reloj`, `Pantalla`, `Espectro`, `Operacion`, `Apariencia`, `Acciones`,
   `Mas >`, `< Mas`).

4. **Cerrar el banco** (teardown documentado):

   ```sh
   docker rm -f banco-apps
   ```

5. Committear acá la captura + su medición (`md5` del binario y del fuente que el
   script imprime) y reemplazar este README por la evidencia real.

## Referencias

- `docs/seten109-spec.md` — la spec de la corrida (§Banco).
- `docs/settings-order.md` — el orden final y el porqué de cada tab.
- `docs/evidencia/2026-10-15-fallback91/banco.txt` — precedente del camino a Settings en el banco.
- `docs/evidencia/2026-10-15-wf92-banco/README.md` — patrón de la evidencia de banco.
- `.agents/pruebas-sin-radio/SKILL.md` — el método del banco sin radio.
