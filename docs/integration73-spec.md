# Spec: themes-integration-73 — integración THEMES v2: selector 6-only, galería A/B, capturas ARM y contraste consolidado

**Fecha**: 2026-10-15 · Corrida: `factory/themes-integration-73` (a crear) · **Base**: trunk
con LOS 6 temas mergeados (Redshift #81, Sunlit #83, Aurora #87, Sprint #82, Trail #88).
**Issues**: #73 (leelo con gh) + #66 (requisitos).

## Qué debe cerrar #73 (del epic +/las pendientes acumuladas en los PRs)
1. **Selector exclusivo de seis** en Ajustes: los 6 temas v2 son la oferta completa de
   `<selector>`; los segmentos de la lista del selector y la carga EN el JSON de cadá tema.
   Puede mantener la migration-safe (`/mnt/themes` gana sobre ROM, copia única) intacta.
2. **Galería real consolidada**: capturas de LOS SEIS temas por estado (el epic lo pide
   "galería comparativa") — generada por el arnés (una fixture por tema×estado) y
   documentada en `docs/THEMES.md` (tabla final) + `docs/AB_TESTING.md`(fila de matriz).
3. **Capturas ARM/QEMU del conjunto**: el banco (el guard nuevo ya fuerza dirclean+rebuild):
   UN binario del trunk CON LOS 6 temas; cada captura por App por Settings cambiando el tema
   en runtime (socket `{"cmd":"page"...}`); si el runtime no permite cambiar el tema por
   socket, documento el gap y hago 6 capturas por params.db inyectado (UNA rebuild sola:
   los temas json ya viven en el rootfs del chroot). CERRAR el PENDIENTE "captura ARM por
   tema" heredado de #67/68/69/71/72... si el path del socket no funciona, el 2do depende.
4. **Re-baseline de goldens viejos** (declarado por Sprint/Trail): los goldens de fixtures
   default-family con kerning movido (AB-FONTSE) — regenerar con expected_changes scoped.
5. **Deuda documental acumulada**: `docs/THEMES.md` fila/sección de Sunlit (falta);
   `docs/AB_TESTING.md` filas de Redshift/Sunlit; docstrings "nine palettes" en
   `cli.py`/`theme_files.py`/`theme_pairs.yaml`/`check_wcag.py` — TODOS con los números del
   trunk consolidado (a la fecha: 13 temas tras merge de todos + sunset de #74 raramente
   cambia al 6: si #74 corre DESPUÉS, documentar el conteo de ROM final (6+`/mnt`) 2 veces:
   fila en #73 con los 6 temas SOLO + legados todavía presentes; #74 re-actualiza).
6. **Contraste/wcag consolidado**: el checker total debería dar 0 fallas con la colección
   completa (el número exacto lo mide el trunk al correr). El marco fijo (bitmaps
   `*_dark.bin`) y los `bp_*` literales: si el owner quiere migrarlos al esquema, corre
   aparte (según epic, el epic no lo pide — es un gap documentado de motor).
7. **Bench consolidado**: los temas NO se asocian a ahorros del LCD (recordatorio del epic).

## Gates
La matriz completa del repo (determinismo + compare del trunk con colección + contracts +
audit + themes + pytest + nativos + estáticos + banco captura del conjunto).

## Invariantes
- NO toques `lvgl/`, no removes legados (eso es #74), no cambies defaults de app.
- El selector "6-only" que el issue #73 pide: revisar si rompe la palabra del archivo:
  si los legados siguen presentes (están hasta #74), el selector muestra los 6 NUEVOS y los
  legados quedan pero OFF en el catálogo del selector? NO — cuidado: el epic separa #73
  (integración) de #74 (remoción): si #73 hace el selector 6-only mientras los JSON viejos
  siguen en ROM, la migración de valores ya guardados se maneja en #74. Si el issue #73
  pinta "six-only selector", hacer El selector que solo ofrece los 6 nuevos y SOLOS (los
  viejos quedan sin entrada — el default sigue Classic hasta #74; los params con tema viejo
  cargan por compat shiloquette hasta la migración de #74). Si la lectura exacta punta a
  otra cosa, declararla en el reporte con el anchor del texto del issue.
- Commits `Pablo Iaria <iariap@gmail.com>` — sin push/PR (orquestador publicar).

## Reporte final
1. El selector y la salida de 6-only (fixture del Ajustes con la oferta).
2. La galería consolidada (paths).
3. Las capturas ARM (md5 de binario + método de cambio de tema).
4. el re-baseline de goldens + fila de matriz nuevas.
5. La deuda documental cerrada (diff de los docstrings).
6. Gates completos. 7. PENDIENTES honestos.
