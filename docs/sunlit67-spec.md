# Spec: theme-sunlit-67 — tema Sunlit · sol y exteriores (THEMES v2, issue #67)

**Fecha**: 2026-10-15 · **Corrida**: `factory/theme-sunlit-67` · **Base**: trunk `4e9b5f4`
**Issues**: #67 (el tema) + #66 (requisitos comunes de la colección — leelos ambos ANTES con
`gh issue view 67`/`66 --repo iariap/x6200_gui`).

## Qué
Tema **Sunlit**: directo sol, alta visibilidad — fondo claro cálido/marfil, azul profundo,
amarillo, **Noto Sans** (ya compilada en ROM desde #60 — no descargar ni regenerar), bordes
firmes. Es el primero de los 6 temas del epic: queda además como **referencia de forma** para
los 5 siguientes (los otros temas son corridas separadas con este PR de ejemplo).

## Requisitos duros (del epic #66)
1. **Solo el esquema real** de `src/theme.c` + `docs/THEMES.md`: keys `base`, `colors`,
   `font`, `font_digits`, `waterfall`, `radius`, `radius_btn`, `radius_popup`, `border`,
   `btn_gradient`, `box_gradient`, `focus_solid`, etc. `font` resuelve por
   `src/fonts/font_families.c` (id de familia; Noto Sans se referencia por el nombre que
   exista ahí). Archivo nuevo: `rootfs/usr/share/x6200/themes/Sunlit.json`.
   No tocar `/mnt/themes` ni la copia de `src/main.c`: el nombre nuevo no colisiona.
2. **Identidad global, no solo bg/accent**: frecuencia, modo, selección, foco, botones
   (normal/disabled), S-meter incl. `meter_over`, RX/TX, warning/error y **paleta waterfall**
   propia (no la default: el requiem del epic es que se distinga en composición).
3. **Los dígitos de frecuencia no deben quedar invisibles** por markup/editor de tema
   (cláusula del epic — verificar en las Writerplas/DSEG fixtures).
4. **Capturas para cada estado**: principal, Ajustes, diálogo, RX/TX simulado, batería
   baja/ECO, waterfall — con el arnés A/B (fixtures existentes del día ya cubren la mayoría;
   si falta alguna fixture nueva para el estado, agregarla a la matriz).
5. **Contraste WCAG**: texto 4.5:1 y componentes 3:1 (salvo excepciones justificadas en la
   ficha) — `check_wcag.py` pasa con el tema nuevo (los pares se evalúan automáticamente por
   el checker; si el checker no evalúa temas JSON, corrá el chequeo por tema como siempre).
6. **Medición**: rasterizado/CPU (bench ECO del arnés, comparado contra Classic y su base) —
   declarar preso del presupuesto; NO asumir ahorro de batería por colores (LCD con backlight).
7. **QEMU ARM**: una captura del binario real en el banco (modo check/captura puntual, no
   dirclean: el banco tiene su guard nuevo y se puede pedir RECONSTRUIR una vez con el trunk)
   para mostrar el tema funcionando fuera del arnés.
8. **Galería**: el epic pide galería comparativa — con un solo tema hoy, dejar la captura del
   tema (los 6 formatos la consolidan en #73). Si el arnés ya produce galería por tema, mmengarla;
   sino dejar la evidencia por separado para el PR de integración.

## Fuera de alcance
- El selector de Ájustes y su default (eso es #73); la remoción de los temas viejos (#74).
- TPHs unicornios del tema: SOLO el esquema real (sin agregar fields al parser, si algo
  pedida no existe, declararla PENDIENTE y elegir el reemplazo del esquema de siempre).
- `lvgl/` pinned; DSP/RF/TX; energía física real.

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/theme-sunlit-67
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a 4e9b5f4 --b HEAD --experiment-id AB-SUNLIT67
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7  (el WCAG incl.)
bench del tema vs Classic (la métrica del render que corre el compare)
```

## Invariantes
- El epic exige PR independiente; **los PRs de temas requieren human review para merge** —
  el orquestador publica el PR y lo deja en verde, NO mergea.
- Commits `Pablo Iaria <iariap@gmail.com>` — **sin push/PR** (publica el orquestador).
- Banco: UNA captura del binario ARM (con RECONSTRUIR=sugno); NO correr barridos ni dirclean
  exhaustivo acá (corridas del banco las serializa el orquestador).

## Reporte final
1. El JSON del tema COMMITTEADO completo (con cada choice justificada vs Classic/composición).
2. Las capturas por estado (paths de archivos) y la evidencia WCAG/de contraste.
3. El bench del tema vs Classic/base con números.
4. La captura QEMU ARM con el banco (md5 de binario).
5. Los gates completos con salida real (el `themes` gate incluido).
6. PENDIENTES honestos + las decisiones de diseño (si el issue #67 preescribe para algo que
   el esquema hoy no soporta, la excepción o el reemplazo y la falta se registra).
