# El rootfs de la imagen: lo que el port le saca al vendor

El rootfs de la imagen se arma con el rootfs del vendor (el origen que Buildroot cachea para el
paquete `x6200-gui`) **mas** el `rootfs/` del port. El vendor trae `/etc/init.d/S95gui`, que arranca
el mismo `x6200_daemon` que el `S20gui` del port: dos GUIs sobre `/dev/fb0` (flicker + doble CPU,
issue #89). El target de Buildroot es incremental y no limpia lo que un paquete dejo de instalar, asi
que ese `S95gui` sobrevive a cada build hasta que se lo saca.

El port declara lo prohibido en `buildroot/prune-vendor-rootfs.sh` (lista explicita, por path
exacto —nunca comodines: `S*gui*` borraria tambien el `S20gui`—) y lo saca en cada instalacion: el
`install(CODE ...)` del `CMakeLists.txt` raiz corre ese script sobre `DESTDIR` junto al
`install(DIRECTORY rootfs/)`. Como todo lo del port, toma efecto cuando el paquete se reinstala
(`make x6200-gui-rebuild`); de ahi sale la imagen.

Verificacion sin radio (sobre una copia del rootfs target, nunca sobre la radio): queda `S20gui`, no
queda `S95gui`:

```bash
buildroot/prune-vendor-rootfs.sh <rootfs-copy>
ls <rootfs-copy>/etc/init.d/S20gui <rootfs-copy>/etc/init.d/S95gui 2>&1
```
