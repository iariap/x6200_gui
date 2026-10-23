# Revisión de documentación contra el candidato `c888308e` (unidad #195)

Fecha de revisión: 2026-10-21 (UTC). Checkout de medición limpio:
`assessment-194/clone-gui` en `c888308eede71205605c06383432ee7e165f202e`.
Este documento **no corrige** la documentación: registra qué afirma cada documento y qué se
observó en el candidato. La actualización de `PRUEBAS.md` y del dossier final es de #199; el
manual y las guías son de #197/#199 según el alcance de cada unidad.

Estados usados: `CURRENT` (coincide con el candidato), `STALE` (histórico de otra imagen/fecha,
no es evidencia del candidato), `INCONSISTENT` (se contradice con el código o con otro documento),
`REVIEW_REQUIRED` (necesita decisión de una unidad posterior; no se toca acá).

| # | Documento | Afirmación | Observado en el candidato | Estado | Evidencia |
|---|---|---|---|---|---|
| D1 | `docs/TESTING.md` (tabla resumen) | "Tests unitarios nativos: 55 checks" y "tests en ISA del objetivo: 55 checks" | La suite nativa host termina en **138 comprobaciones, 0 fallos** (`host` y `host_ci_parity`) | STALE | `logs/step4-runners/host.log`, `logs/step4-runners/host_ci_parity.log` |
| D2 | `docs/TESTING.md` (tabla de suites) | `host`: "469 comprobaciones, 0 fallos" | **138 comprobaciones** en el candidato; además el mismo documento dice 55 en la tabla de arriba | INCONSISTENT + STALE | `logs/step4-runners/host.log` |
| D3 | `docs/TESTING.md` | `wspr`: "158 comprobaciones, 0 fallos" | **158 comprobaciones, 0 fallos** (imagen `x6200-uiharness-full`) | CURRENT | `logs/step4b/codec-wspr.log` |
| D4 | `docs/TESTING.md` | `js8`: "1994 aserciones en 88 casos" | **1994 aserciones en 88 test cases** | CURRENT | `logs/step4b/codec-js8.log` |
| D5 | `docs/TESTING.md` | `sstv`/`wefax`/`navtex`: all OK / PASS / all OK | SSTV `all OK` (correlación 0,956–0,982), WeFax `PASS` (300/300, 0,9953), NavTex `all OK` | CURRENT | `logs/step4b/codec-sstv.log`, `logs/step4b/codec-wefax.log`, `logs/step4-runners/navtex.log` |
| D6 | `docs/TESTING.md` | `webserver`: "19 páginas/endpoints" | No ejecutado: `WS` inexistente (SKIP por diseño de #195) y la imagen no trae `/b/target/usr/bin/python3` | STALE / SKIP | `logs/step1/webserver-skip.log` |
| D7 | `docs/NEW_FEATURES.md` (tabla inicial) | "Web configurator ampliado (15 páginas + HTTPS)" | `docs/TESTING.md` dice 19 páginas para el mismo webserver | INCONSISTENT | documentos citados |
| D8 | `docs/INTEGRATION_AUDIT.md` §3 | "401 comprobaciones del arnés nativo del host" | 138 comprobaciones; además difiere de los 55/469 de `TESTING.md` | INCONSISTENT + STALE | `logs/step4-runners/host.log` |
| D9 | `docs/manual/MANUAL.md` §6.22 | Documenta la app **About** | `ACTION_APP_ABOUT` **fue eliminada** (parámetro eliminado el 2026-10-14; el hueco 121 queda libre en `src/params/params.h`). No hay app About en el dispatcher | STALE | `src/params/params.h:64`, `src/main_screen.c` |
| D10 | `docs/manual/MANUAL.md` §6 | 24 secciones de apps (6.1–6.24) | El firmware tiene 24 `ACTION_APP_*`, pero incluye `ACTION_APP_DFL_EDITOR` (sin sección en el manual) y no tiene About: el manual cubre una app inexistente y omite el editor DFL | REVIEW_REQUIRED | `logs/step4-runners/manual_apps.log` |
| D11 | `tests/check_manual_apps.py` | "todas las apps tienen su sección (24 apps)" | El chequeo **reporta `ACTION_APP_DFL_EDITOR` como "sin nombre mapeado" y aun así retorna 0**; tampoco detecta secciones sobrantes (About). El verde no cubre el hueco | REVIEW_REQUIRED | `logs/step4-runners/manual_apps.log` (línea "apps sin nombre mapeado") |
| D12 | `docs/manual/MANUAL.md` tabla APP 1:6…6:6 | Página APP 6:6 = WiFi, Bluetooth, SWR Scan (3 ítems) | La página 6 tiene 4 celdas: WiFi, BT, SWR Scan y **Auto Tune** (`buttons.cpp page_app_6`) | STALE | `src/buttons.cpp` |
| D13 | `docs/ROM_REVIEW.md` | Imagen `x6200_20261011_201456.img`, MD5 `4a8264bf…` | Artefacto del 2026-10-11, **no** es el candidato `c888308e` ni un build de esta campaña | STALE | `docs/ROM_REVIEW.md` §0 |
| D14 | `docs/RESOURCE_USAGE.md` | Mediciones sobre `x6200_20261011_221133.img` | Imagen del 2026-10-11; consumo/RSS no re-medidos para el candidato | STALE | `docs/RESOURCE_USAGE.md` |
| D15 | `docs/IMAGE_INVENTORY.md` | Imagen `x6200_20261012_013612.img` sha256 `8d9868da…`; temas "RS (10 PNG) + Holo.json + Material 3.json" | Imagen del 2026-10-12; el candidato trae **6 temas JSON** (Aurora, Cathode, Redshift, Sprint, Sunlit, Trail) y los temas legados fueron retirados por #74 | STALE | `rootfs/usr/share/x6200/themes/` |
| D16 | `docs/UI_KNOWN_ISSUES.md` | Registro de defectos con fixture, actualizado 2026-10-15; fuente de verdad `tools/ab_testing/quality/known_issues.yaml` | No re-auditado en #195 (dueño #197). El archivo existe y el registro es consistente con el mecanismo A/B | PENDING (#197) | documento citado |
| D17 | `docs/TESTING.md` §"Pruebas y capturas sin radio" | Referencia a `.agents/pruebas-sin-radio/SKILL.md` | El archivo existe en el candidato | CURRENT | `test -e` OK |
| D18 | `docs/HARDWARE_TEST_GUIDE.md` | Checklist físico | Existe; no ejecutado en #195 (sin radio) | PENDING_HARDWARE (#199) | documento citado |

## Consecuencias para la campaña

1. Ningún conteo de `TESTING.md`/`INTEGRATION_AUDIT.md` debe copiarse como actual: los únicos
   números válidos para `c888308e` son los medidos en esta unidad (138 comprobaciones host, 158
   WSPR, 1994/88 JS8, 300/300 WeFax, SSTV `all OK`).
2. `ROM_REVIEW.md`, `RESOURCE_USAGE.md` e `IMAGE_INVENTORY.md` son evidencia histórica de otras
   imágenes: sirven como antecedente con fecha/SHA, no como identidad ni medición del candidato.
3. El manual tiene un hueco real (editor DFL) y una sección muerta (About); el chequeo
   `check_manual_apps.py` no lo detecta. No se corrige en #195: se registra para #197/#199.
4. El webserver no se pudo validar (SKIP documentado); sus páginas no se adjudican a la GUI.
