# X6200 LVGL GUI — rama `lu2dsx/x6200-rom`

Este repositorio es el fork de [iariap](https://github.com/iariap/x6200_gui) del firmware
alternativo de R1CBU para el Xiegu X6200. **Esta rama** (`lu2dsx/x6200-rom`) es la ROM de LU2DSX:
el firmware del X6200 con las features del X6100 de
[IU1WXY](https://github.com/IU1WXY/X6100-IU1WXY) portadas encima.

## Qué trae esta rama

18 features del X6100, más extras, cada una documentada y con tests:

- **Modos digitales completos**: WSPR, NavTex, SSTV (7 modos), WeFax y JS8.
- **DX Cluster**: spots, filtros, alertas y **columna sobre el waterfall** (se toca un spot y
  sintoniza).
- **Field Ops SOTA/POTA**: referencias, log ADIF, feed RBN y **CQ de voz** (graba el CQ y lo emite
  en SSB).
- **Bluetooth**: CAT (SPP), auriculares A2DP, teclados y una **app de dispositivos** (listar,
  escanear, vincular, passkey).
- **wfview** (la radio como IC-705 en red) y **control remoto en vivo** para la página `/remote`
  del web configurator, con todas las apps.
- **Temas JSON** (6 incluidos + propios en `/mnt/themes`): los sigue **toda** la UI.
- **Band plan IARU**, **aguja de S-meter**, **grabación de pantalla** (AVI con audio de RX),
  **aviso de versión**, **CW Keyboard**, **CW Trainer**, **decodificados FT8 por UDP** (con país y
  bandera desde `cty.dat`) y una app **Status** con el estado del equipo.
- **DRM/KMS** disponible (apagado por defecto: se prueba creando `/mnt/use_drm`) y la **perilla
  principal** leída en su propio hilo.

El detalle de cada una, cómo se usa y qué le falta está en
[`docs/NEW_FEATURES.md`](docs/NEW_FEATURES.md). El estado frente al X6100, en
[`docs/X6100_FEATURE_GAP.md`](docs/X6100_FEATURE_GAP.md), y cómo se llega a cada cosa y qué se
probó sin radio, en [`docs/INTEGRATION_AUDIT.md`](docs/INTEGRATION_AUDIT.md).

## Instalar

Las releases de esta rama están en
[**iariap/x6200_gui/releases**](https://github.com/iariap/x6200_gui/releases). Hay dos caminos.

**Actualización en el lugar** (recomendada: no toca tus datos). El equipo ya sabe actualizarse:
`S02update` busca `x6200_r1cbu_*.tgz` en la partición DATA (`/mnt`) y lo descomprime sobre la raíz.

1. Copiá el `x6200_r1cbu_<versión>.tgz` de la release a `/mnt` (por SSH o sacando la SD).
2. Reiniciá. El firmware hace un backup y aplica la actualización.
3. La configuración, los logs, los temas, las grabaciones y los mensajes de voz quedan como
   estaban.

Ojo: este camino no actualiza el kernel ni u-boot (esos van en la imagen).

**Imagen completa**: bajá el `x6200_<fecha>.img` y grabalo en la microSD con balenaEtcher o Rufus.
Insertá la tarjeta y encendé. Grabar la imagen recrea las particiones: **respaldá `/mnt`** antes.

Verificá lo que bajaste con el `SHA256SUMS` de la release.

## Compilar

Esta rama se compila con el buildroot del mismo fork:

```
mkdir x6200
cd x6200
git clone -b lu2dsx/x6200-rom https://github.com/iariap/AetherX6200Buildroot
git clone -b lu2dsx/x6200-rom https://github.com/iariap/x6200_gui
```

* Build buildroot

```
cd AetherX6200Buildroot
./br_build.sh
cd ..
```

* Build app

```
cd x6200_gui
git submodule init
git submodule update
cd buildroot
./build.sh
```

Los tests (todo lo que se puede probar sin la radio) se corren con `tests/run_all_tests.sh`; ver
[`docs/TESTING.md`](docs/TESTING.md).

## Importing ADI log

Application could mark already worked callsign in the UI.
To load information about previous QSOs - copy your ADI log to the `DATA` partition and rename it to `incoming_log.adi`.
Application will import records to own log on the next boot and will rename `incoming_log.adi` to `incoming_log.adi.bak`.

*Note*: `DATA` partition will be created after first launch transceiver with inserted SD card.


## Exporting ADI log

Open in browser `http://<transceiver IP>/logs` and click `Export`.

## Créditos y licencia

El firmware original es el port de R1CBU hecho por [gdyuldin](https://github.com/gdyuldin/x6200_gui)
(LGPL-2.1-or-later). Las features portadas vienen del firmware del X6100 de
[IU1WXY](https://github.com/IU1WXY/X6100-IU1WXY), también LGPL-2.1-or-later, con su atribución en
cada archivo. `js8core` es GPLv3 y el decodificador WSPR viene del `wsprd` de K1JT: ambos están
vendorizados con su licencia. Las banderas de país son de flagcdn.com (dominio público) con códigos
ISO de [k0swe/dxcc-json](https://github.com/k0swe/dxcc-json) (MIT).
