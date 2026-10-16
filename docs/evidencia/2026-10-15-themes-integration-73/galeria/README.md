# Evidencia: galería consolidada de los seis temas v2 (THEMES v2, issue #73) — 2026-10-15

Las **hojas de contactos** de la colección completa: los seis temas v2 (Sunlit,
Redshift, Sprint, Cathode, Trail, Aurora) en el **mismo estado de radio**, así el tema
es la única variable. Son renders reales del arnés de UI (`tools/ui_harness`, LVGL 8.3.11
sobre el framebuffer de 800×480), nunca una ilustración.

**Esto NO es la radio.** Es el render determinista del arnés sobre el mismo código
(`src/theme.c`, `src/styles.c`, `src/main_screen.c`, `src/fonts/`): sirve para comparar
composición, tipografía, color y jerarquía, y no dice nada del brillo al sol ni del
consumo. La captura del **binario ARM real** (QEMU user-mode / banco) está en
`../banco/`.

## Las hojas

| Hoja | Estado | Fixture por tema |
|---|---|---|
| `galeria-principal.png` | pantalla principal, RX (frecuencia, modo, VFO, S-meter, band plan, cascada) | `main-tema-<tema>` |
| `galeria-ajustes.png` | Ajustes → Apariencia, con el foco sobre la fila del tema | `settings-tema-<tema>` |
| `galeria-dialogo.png` | diálogo de app (`dialog_bt.c`): selección, texto atenuado, acción | `bt-dialog-tema-<tema>` |
| `galeria-tx.png` | TX simulado (visor TX, split, `tx_bg`/`tx_box`) | `main-tema-<tema>-tx` |
| `galeria-cascada.png` | waterfall en movimiento (peaks, línea central, espectro lleno) | `main-tema-<tema>-agua` |
| `galeria-eco.png` | batería baja (reloj en %) + política en ECO | `main-tema-<tema>-eco` |

`<tema>` en minúscula (`sunlit`, `redshift`, `sprint`, `cathode`, `trail`, `aurora`): es
el nombre de la fixture del arnés.

## Cómo se reprodujo

Los renders, desde `tools/ab_testing` en la revisión de esta corrida (`HEAD`):

```bash
uv run x6200-ab capture --revision HEAD --label int73 \
    --scenario main-tema-sunlit --scenario main-tema-redshift --scenario main-tema-sprint \
    --scenario main-tema-cathode --scenario main-tema-trail --scenario main-tema-aurora \
    --scenario settings-tema-sunlit --scenario settings-tema-redshift --scenario settings-tema-sprint \
    --scenario settings-tema-cathode --scenario settings-tema-trail --scenario settings-tema-aurora \
    --scenario bt-dialog-tema-sunlit --scenario bt-dialog-tema-redshift --scenario bt-dialog-tema-sprint \
    --scenario bt-dialog-tema-cathode --scenario bt-dialog-tema-trail --scenario bt-dialog-tema-aurora \
    --scenario main-tema-sunlit-tx --scenario main-tema-redshift-tx --scenario main-tema-sprint-tx \
    --scenario main-tema-cathode-tx --scenario main-tema-trail-tx --scenario main-tema-aurora-tx \
    --scenario main-tema-sunlit-agua --scenario main-tema-redshift-agua --scenario main-tema-sprint-agua \
    --scenario main-tema-cathode-agua --scenario main-tema-trail-agua --scenario main-tema-aurora-agua \
    --scenario main-tema-sunlit-eco --scenario main-tema-redshift-eco --scenario main-tema-sprint-eco \
    --scenario main-tema-cathode-eco --scenario main-tema-trail-eco --scenario main-tema-aurora-eco \
    --out /tmp/opencode/int73-renders
```

Y las hojas, con el compositor del repo (Pillow, sin red):

```bash
python3 tools/galeria-temas.py --renders /tmp/opencode/int73-renders \
    --out docs/evidencia/2026-10-15-themes-integration-73/galeria
```

El arnés es determinista (`tests/check_harness_determinism.sh`: dos corridas del mismo
escenario dan el mismo sha256), así que los renders y las hojas se pueden volver a
generar byte a byte.

## Contraste (medido, no estimado)

`tests/check_wcag.py` mide las quince paletas servidas y cierra en **0 FALLAS**;
`x6200-ab themes` mide los pares que sólo viven en Ajustes (135 pares, **0 fallas**) y el
no-texto por píxeles (WCAG 1.4.11) sobre los renders dorados. Los ratios de cada tema
están en `docs/THEMES.md` (una sección por tema).
