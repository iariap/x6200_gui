# STALE y validez del dossier

## 1. Declaración formal

Este dossier es válido **exclusivamente** para:

| Elemento | SHA / identidad |
|---|---|
| GUI candidato | `c888308eede71205605c06383432ee7e165f202e` |
| Buildroot congelado | `d084e2870a00b64d9c57474e7eceeef80f43f25b` |
| LVGL (gitlink) | `74d0a816440eea53e030c4f1af842a94f7ce3d3` (v8.3.11) |
| `fuente-md5` | `964193ae9306b5127845cec44a57d52c` |
| Artefacto | `sdcard.img` `d0ff4435…`, `rootfs.tar` `7c26c76b…`, `rootfs.ext4` `9365b52f…`, `boot.vfat` `914f07eb…` |
| GUI en rootfs | 13.407.624 B, md5 `1f998c46909307397525c975a712adfd`, sha256 `7e2e9331…` |
| Gate #86 | PASS (`tools/verifica-firmware-port.py`) |

**No es válido** para el tip de `lu2dsx/x6200-rom` ni para el ROM del orquestador
(`rom-build`, GUI md5 `3b22a03a…`, `fuente-md5 a576d48d…`), que pertenecen a otro SHA.

## 2. GUI: tip `d4fb57f8` (STALE confirmado)

`d4fb57f8` incluye:

- **#200** "Neo-retro animated text boot with safe waterfall handoff" (`50bf8ab0`).
- **#202** limpieza del u-boot prebuilt (`9ed25fa9`).
- **#206** "Continuous Decrypt, Beams and Rain until GUI handoff" (`d4fb57f8`).
- Commits de QA de arranque/MBR (`0d00e7fc`, `f3f381fd`, `2a6059b5`, `dadb1705`).

`git diff --name-only c888308e d4fb57f8 | grep -v '^docs/'` = **25 archivos de producción**
(74 archivos totales incluyendo 49 de `docs/`):

```
CMakeLists.txt
.github/workflows/boot-rootfs.yml
.github/workflows/main.yml
rootfs/etc/init.d/S01bootanim
src/CMakeLists.txt
src/boot_splash.c
src/boot_splash.h
src/bootanim/CMakeLists.txt
src/bootanim/fb.c
src/bootanim/fb.h
src/bootanim/main.c
src/bootanim/renderer.c
src/bootanim/renderer.h
src/main.c
tests/bootanim/fake_animator.c
tests/bootanim/fake_fb.c
tests/bootanim/fb_test.c
tests/bootanim/handoff_test.c
tests/bootanim/render_bench.c
tests/bootanim/renderer_test.c
tests/bootanim/replay_dump.c
tests/bootanim/start-stop-daemon.sh
tests/bootanim/test_util.h
tests/check_bootanim.sh
u-boot/u-boot-sunxi-with-spl.bin   (borrado)
```

> **Corrección:** #198 escribió "26 archivos"; el recuento reproducible es **25**. Se corrige acá
> sin editar el informe de #198.

## 3. Buildroot: tip `f23d14f` (segundo STALE)

El par congelado usa Buildroot `d084e287`. El repo avanzó a `f23d14f` con:

- **PR #2** (`f23d14f`, merge 2026-10-21T21:51:25Z): agrega `BR2_PACKAGE_LIBTOOL`,
  `BR2_PACKAGE_LIBSNDFILE` y `BR2_PACKAGE_PULSEAUDIO` a `X6200_defconfig`.
- **PR #3** (`128d96d`, merge 2026-10-21T21:50:38Z): quita el logo de kernel
  (`# CONFIG_LOGO is not set`), para que solo se vea la animación de #200.

El artefacto medido **no** se reconstruyó con `f23d14f`; su kernel y `rootfs.tar`/`sdcard.img`
siguen siendo los de `d084e287`. Por lo tanto:

- F-11 (defconfig sin audio) está **resuelto en el repo**, pero el artefacto medido sigue siendo
  el construido con la desviación local declarada en #195/#196.
- Adoptar `f23d14f` cambia kernel (logo/tiempos de boot) y la config de audio: hay que repetir
  build, identidad, boot, energía y audio.

## 4. Cobertura que habría que repetir si se adopta el tip

No se ejecutó ninguna repetición; esta es la lista de trabajo futura.

| Área | Qué repetir | Por qué |
|---|---|---|
| Build/identidad | build ARM/ROM desde el par nuevo, SHA256 de sdcard/rootfs/boot/GUI, `fuente-md5`, gate #86 | cambian `CMakeLists.txt`, `src/main.c`, bootanim y workflows |
| Layout/DATA | layout/MBR, p3, primer boot en tarjeta nueva, montaje `/mnt`, `S01create_data` | cambia `S01bootanim`; el tip ya mostró fragilidad de p3/MBR |
| Init/prune | inventario de `init.d`, orden, sintaxis, prune | nuevo `S01bootanim`; workflows de boot/rootfs |
| Arranque real | HW-01: 5 fríos + 5 ordenados, boot.log, ready-to-RX, 1 GUI + 1 daemon | `main.c`/bootanim/handoff cambian el arranque |
| Estrés/reposo | 100 ciclos, 1 h/8 h, recursos | `main.c` y el handoff cambian el camino |
| Update | empaquetado, setuid/owner, primera update, rollback/corte | cambian `main.yml` y el u-boot prebuilt |
| UI/UX | capturas/contratos/determinismo, 6 temas, diálogos | `boot_splash`/`main.c` y el handoff pueden cambiar primer frame |
| Performance | render, natural/wakeups, boot-to-socket, CPU/RSS | el bootanim/handoff agrega trabajo antes de la GUI |
| Hardware | **todo** HW-01..HW-08 + HR-01..HR-06 | el artefacto y el software instalado serían otros |

## 5. Regla de revalidación

Antes de la fase B (o de cualquier PR final) hay que volver a ejecutar:

```bash
git ls-remote https://github.com/iariap/x6200_gui.git refs/heads/lu2dsx/x6200-rom
git ls-remote https://github.com/iariap/AetherX6200Buildroot.git refs/heads/lu2dsx/x6200-rom
```

Si los tips cambiaron, este dossier **sigue siendo válido para `c888308e`** y se actualiza la
declaración STALE con los SHAs nuevos; no se reescriben los resultados de 01..04. Cualquier
resultado de otro SHA se marca **HISTORICAL** y no rellena cobertura del candidato.
