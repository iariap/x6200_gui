# Deployment de la GUI para la validación física de #351

## Base y build

| Campo | Valor |
|---|---|
| Repositorio | `iariap/x6200_gui` |
| Base | `main` en `fc18a831a3db1e15f316be7d613152fcefc705ec` (verificado con `git fetch` + `git rev-parse` al arrancar) |
| Fuente del build | `rom-build/clone-gui` sincronizado a `FETCH_HEAD` de `main` = `fc18a831` |
| Build | contenedor `x6200-buildroot`; `make x6200-gui-dirclean && make -j8`; exit 0 |
| Binario | `x6200_gui`, ELF 32-bit LSB pie executable, ARM, EABI5, stripped |
| Tamaño nuevo | 13.478.088 B |
| sha256 nuevo | `4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0` |
| md5 nuevo (prefijo usado en PRUEBAS) | `80bbe5bd70b345e1e9a563d06d047a1c` |
| Tamaño previo | 13.473.960 B |
| sha256 previo (instalado) | `ac2f6febb63b518b398779503d14f6993b721b21fb873cd630b8599e12f73d87` |
| `boot_identity` del nuevo build | `42cd5c541237db86d23441505637f5cd` |

El binario **no se versiona en el repositorio**: el build completo y sus logs están en
`logs/build.log`; el ejecutable y los backups se mantienen fuera del repo.

## Tests ejecutados antes del deploy

- `sh tests/native/run_native_tests.sh` → **exit 0**, 174 comprobaciones, 0 fallos
  (`logs/native-tests.log`).
- `sh tests/run_all_tests.sh --quick` → **exit 1**, 22 OK y 1 fallo **preexistente**:
  `audita-pruebas` marca que el build `a92248cceef0` está en la tabla de resumen de
  `docs/PRUEBAS.md` sin sección propia. No está causado por este cambio; se conserva el registro
  original (`logs/quick-tests.log`).
- Verificación del ELF ARM (`file`/`readelf`) y del hash del binario de buildroot antes de subirlo.

## Backup y rollback

En la radio, fuera del repositorio:

- `/mnt/ui-bak-191601/x6200_gui` — binario anterior, sha256 `ac2f6feb…73d87`.
- `/mnt/ui-bak-191601/params.db.pre351` — copia de `params.db` (sha256 `0661bd52…d5d4e`).
- `/mnt/ui-bak-191601/installed.sha256` — hashes del binario instalado y del backup.

Rollback probado como procedimiento (no fue necesario ejecutarlo, no hubo regresión bloqueante):

```sh
cp /mnt/ui-bak-191601/x6200_gui /usr/sbin/x6200_gui
kill $(pidof x6200_gui)      # el daemon relanza la GUI
sha256sum /usr/sbin/x6200_gui # verificar ac2f6feb…73d87
```

La conexión se verificó con 5 pings y con consultas `get` repetidas antes del deploy.

## Deploy realizado

- Se subió **sólo** `/usr/sbin/x6200_gui` por SSH y se reinició la GUI con `kill` (el daemon la
  relanza). **No se flasheó la ROM, no se reemplazó el kernel y no se escribieron registros del
  BASE.**
- El hash instalado se verificó después del reemplazo y luego del arranque:
  `4ee87aef…dd66b0` en ambos casos.
- La GUI arrancó en 6 s (pid nuevo 2983), `crashlog 0`, `get` respondiendo.
- `Settings → Audio` muestra la fila **`Capture filters`**, lo que confirma que la UI desplegada
  incluye el cambio de #351 (`captures/settings-audio-capture-filters.png`).

## Verificación post-deploy

- 5 consultas `get` consecutivas: ~404–408 ms, todas OK.
- Waterfall: std 14,75 y 140 filas no vacías (antes 15,09/140) → sigue dibujando.
- Audio RX: `parec` del source devuelve señal (rms 2699 en la primera comprobación; luego 3238).
- `crashlog 0`, un solo proceso GUI, daemon vivo.
- Estado antes/después: frecuencia 27.505 MHz, banda CB D, modo USB, filtro 2700, NR 1/30, NB 0.
  El volumen pasó de 0 a 14 (y luego a 1) por la recarga de estado persistido al reiniciar la GUI;
  el preset de `Capture filters` quedó en 2 (SSB voice), como estaba antes del deploy.
- `params.db` cambió de hash (`0661bd52…` → `ea321847…`) porque la GUI lo reescribe al arrancar;
  el backup previo está en `/mnt/ui-bak-191601/` y la configuración se conservó.

## Incidente de UI y rollback disponible

Durante la sesión de pruebas, una vez, con Ajustes abierto, la UI quedó con el contenido central en
negro (barra superior y botones visibles). La GUI seguía viva y `get` respondía; `crashlog 0`. Se
reinició la GUI (kill; el daemon la relanzó en 6 s) y volvió a renderizar; no se repitió. No se
determinó la causa raíz; las capturas están en `captures/incidente-ui-*.png` y el backup permite
rollback inmediato si reaparece. Este incidente **no** se declara resuelto.

## Estado final

- Preset `Capture filters` restaurado a `Off` por el operador.
- `crashlog 0`; GUI nueva viva; sin TX; sin cambios en ROM/BASE.
- Los temporales de la radio y las grabaciones privadas se borraron de la radio después de
  copiarlos fuera del repositorio; no se publica audio de QSO.
