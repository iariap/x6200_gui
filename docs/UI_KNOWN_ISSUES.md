# Defectos conocidos de la UI

Registro de defectos de la interfaz. **Regla**: un defecto entra acá solo si el
**arnés lo reproduce** (`tools/ab_testing/`); lo que no se puede reproducir queda
en el [anexo de hipótesis](#anexo-hipótesis-sin-reproducir) con lo que haría
falta para fundarlo. La fuente de verdad operativa es
[`tools/ab_testing/quality/known_issues.yaml`](../tools/ab_testing/quality/known_issues.yaml):
este documento la explica, no la duplica.

Un defecto corregido pasa a test permanente: en cuanto el arnés lo reproduce, la
comparación A/B lo clasifica (`FIXED` / `UNCHANGED DEFECT` / `REGRESSION` /
`NEW DEFECT`) y una reaparición vuelve a fallar la corrida.

## UI-001 — Etiqueta del multiplicador de tuning visible sobre la barra superior

- **Severidad**: major.
- **Fixture**: `home-usb-20m`, región `multiplicador`.
- **Estado**: `fixed` (si vuelve a aparecer en B, es `REGRESSION`).
- **Síntoma**: la etiqueta `x%.1f` del tuning adaptativo se creaba **visible** y
  tapaba la barra de estado (clima/voltaje). Ver `docs/ROM_REVIEW.md` §5.3.
- **Fix en el código**: la etiqueta se crea con `LV_OBJ_FLAG_HIDDEN` y el timer
  de ocultado la vuelve a esconder (`src/main_screen.c`).
- **Cómo lo reproduce el arnés**: el detector `region_content_above` mide la
  fracción de píxeles que se apartan del color mediano de la región
  `multiplicador`. Oculta, la región es fondo puro (≈0); si la etiqueta se
  dibuja, sube muy por encima del umbral (0.05). Ver la corrida
  `tools/ab_testing/reports/AB-e2e-fix/` (`FIXED`) y `AB-e2e-reappear/`
  (`REGRESSION`).

---

## Anexo: hipótesis sin reproducir

Semillas con evidencia previa en el repo pero **sin detector** todavía: no
bloquean ni pasan, quedan listadas como `unverified` hasta que el arnés las
reproduzca. Cada una dice qué falta.

| ID | Tema | Evidencia previa | Qué falta para fundarla |
|---|---|---|---|
| UI-002 | Las apps solo vivían en el long-press de Ajustes | `docs/HALLAZGOS.md` 17, 29 | un fixture que abra el menú de apps |
| UI-003 | Pares texto/fondo por debajo de 4.5:1 / 3:1 | `docs/HALLAZGOS.md` 24, 26, 27, 28; `tests/check_wcag.py` | que el arnés exponga los pares que dibuja, o un detector de contraste sobre regiones de texto |
| UI-004 | Ajustes: widgets inalcanzables con el foco | `docs/HALLAZGOS.md` 18; `tests/check_settings_focus.py` | un fixture que abra Ajustes y emita la grilla de foco |
| UI-005 | Diálogo viejo: texto tapado por los botones | `docs/HALLAZGOS.md` 21 | un fixture que abra el diálogo |
| UI-006 | Glifo BT U+F293 inexistente en `sony_20` | `docs/HALLAZGOS.md` 19; `src/info.c` | un detector de glifos sobre las fuentes (la runa se dibuja con `lv_line`) |
| UI-007 | Temas RS sin clave propia, contraste 2.58:1 | `docs/HALLAZGOS.md` 28; `docs/THEMES.md` | chequeo de contraste sobre los pares del tema |
| UI-008 | CMA: 128 MB reservados para GPU no usada | `docs/RESOURCE_USAGE.md` | presupuesto de recursos del firmware, no defecto de la UI; el arnés no mide memoria del sistema |
