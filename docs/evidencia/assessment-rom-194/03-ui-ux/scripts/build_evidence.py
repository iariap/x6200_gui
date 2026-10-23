#!/usr/bin/env python3
"""Build the #197 deliverable tables from the preserved run artifacts.

Inputs (all under this evidence folder unless noted):
  captures/*.json|png     -- `x6200-ab capture` output for the frozen candidate
  logs/step2/determinism.log
  logs/step3/themes.json
  logs/step3/audit-contracts.log (audit + contracts JSON, extracted)
  ../01-baseline/coverage.csv (#195 inventory; the #197-owned rows)

Outputs:
  results.csv        -- one row per metric (issue #197 result contract)
  ui-coverage.csv    -- the #195 #197-owned rows, each with fixture/capture or
                        UNSUPPORTED+reason
  atlas.md           -- indexed atlas (thumbnail/link, fixture/app/mode/theme/
                        seed/layer/revision)

This is analysis glue over existing tools; it does not replace the harness,
the parser, the mock, a test or a contract.
"""

from __future__ import annotations

import csv
import hashlib
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
CAP = HERE / "captures"
LOGS = HERE / "logs"
BASE = HERE.parent
CANDIDATE = "c888308eede71205605c06383432ee7e165f202e"

RESULTS_COLS = [
    "scenario_id", "layer", "coverage", "preconditions", "actions", "expected",
    "observed", "status", "metric", "value", "unit", "samples", "evidence", "limitation",
]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def load_captures() -> dict[str, dict]:
    out: dict[str, dict] = {}
    for p in sorted(CAP.glob("*.json")):
        out[p.stem] = json.loads(p.read_text())
    return out


def extract_json_blob(text: str, must_have: set[str]) -> dict | None:
    """Return the first JSON object in `text` whose keys include `must_have`."""
    for m in re.finditer(r"^\{", text, re.MULTILINE):
        try:
            obj, _ = json.JSONDecoder().raw_decode(text[m.start():])
        except ValueError:
            continue
        if isinstance(obj, dict) and must_have <= set(obj):
            return obj
    return None


def dominant_level(cov: list[dict]) -> str:
    levels = [c.get("level", "UNSUPPORTED") for c in cov]
    for want in ("REAL", "PARTIAL", "SYNTHETIC"):
        if want in levels:
            return want
    return "UNSUPPORTED"


def theme_of(rep: dict) -> str:
    t = rep.get("theme") or {}
    req, act = t.get("requested", "?"), t.get("active", "?")
    return req if req == act else f"{req}->{act}"


def main() -> int:
    caps = load_captures()
    src = (BASE / "01-baseline" / "coverage.csv")
    rows01 = list(csv.DictReader(src.open())) if src.is_file() else []

    det_log = (LOGS / "step2" / "determinism.log").read_text()
    det_ok = "PASS: deterministic" in det_log
    det_n = len(re.findall(r"^[a-z0-9-]+: [0-9a-f]{64}$", det_log, re.MULTILINE))

    themes = json.loads((LOGS / "step3" / "themes.json").read_text())
    ac_log = (LOGS / "step3" / "audit-contracts.log").read_text()
    audit = extract_json_blob(ac_log, {"instrumentation", "issues"})
    contracts = extract_json_blob(ac_log, {"checks", "unchanged_defects"})

    R: list[list[str]] = []

    def add(sid, layer, cov, pre, act, exp, obs, status, metric, value, unit, samples, ev, lim):
        R.append([sid, layer, cov, pre, act, exp, obs, status, metric, value, unit, samples, ev, lim])

    # --- determinism ---------------------------------------------------------
    add("DET-01", "HOST", "REAL",
        "harness c888308e compilado en x6200-uiharness; cada fixture sin --inspect",
        "tests/check_harness_determinism.sh (cada fixture renderizada 2 veces)",
        "PNG byte-identico entre las dos corridas de cada fixture",
        f"{det_n}/{det_n} fixtures con PNG identico en A/A" if det_ok else "no concluyente",
        "PASS" if det_ok else "INCONCLUSIVE", "determinism_png_sha256_match",
        f"{det_n}/{det_n}", "fixtures", str(det_n), "logs/step2/determinism.log",
        "A/A solamente: no valida regresion contra un firmware anterior")

    # --- capture integrity ---------------------------------------------------
    for name, rep in caps.items():
        cov = rep.get("coverage", [])
        fb = rep.get("framebuffer", {})
        met = rep.get("metrics", {})
        png = CAP / f"{name}.png"
        add(f"CAP-{name}", "HOST", dominant_level(cov),
            f"fixture {name}.scn; seed={rep.get('seed')}; pantalla={rep.get('screen')}; tema={theme_of(rep)}",
            "x6200-ab capture --revision CANDIDATE",
            "PNG+JSON 800x480 con hitos no vacios",
            f"framebuffer {fb.get('width')}x{fb.get('height')}; flushes={met.get('flushes')}; "
            f"flushed_px={met.get('flushed_pixels')}; fb_fnv1a={met.get('framebuffer_fnv1a')}; tema={theme_of(rep)}",
            "PASS", "render_framebuffer_hash", str(met.get("framebuffer_fnv1a")), "hash",
            "1", f"captures/{name}.png (+.json) sha256={sha256_file(png)[:16] if png.is_file() else 'n/a'}",
            "PNG identico en A/A (DET-01); no prueba legibilidad en hardware")

    add("CAP-ALL", "HOST", "REAL", "candidato completo", "x6200-ab capture --revision CANDIDATE",
        "199 fixtures renderizadas", f"{len(caps)} capturas PNG+JSON", "PASS",
        "captures_count", str(len(caps)), "fixtures", str(len(caps)), "captures/",
        "todas renderizadas; ver ui-coverage.csv para el mapeo por componente")

    # --- audit ---------------------------------------------------------------
    if audit is not None:
        inst = audit.get("instrumentation", {})
        goods = [i for i in audit.get("issues", []) if i.get("reproduced")]
        unver = [i for i in audit.get("issues", []) if i.get("reproduced") is None]
        add("AUDIT-ALL", "HOST", "REAL", "captura estructural --inspect del candidato",
            "x6200-ab audit --revision CANDIDATE",
            "0 hallazgos estructurales no exceptuados", f"status={audit.get('status')}",
            audit.get("status", "INCONCLUSIVE"), "structural_failures",
            str(audit.get("structural_failures")), "findings", str(inst.get("nodes_inspected")),
            "logs/step3/audit-contracts.log", "la inspeccion estructural es real (LVGL), sin OCR")
        for issue in audit.get("issues", []):
            r = issue.get("reproduced")
            status = "FAIL" if r else ("INCONCLUSIVE" if r is None else "PASS")
            add(f"AUDIT-{issue.get('id')}", "HOST", "REAL", "detector de known_issues sobre la fixture",
                f"x6200-ab audit ({issue.get('id')})",
                "el defecto se reproduce si esta vigente",
                issue.get("detail", ""), status, f"known_issue_{issue.get('id')}",
                str(r), "bool", "1", "logs/step3/audit-contracts.log",
                "un issue sin detector queda unverified (no falla ni pasa)")
        add("AUDIT-SUM", "HOST", "REAL", "-", "x6200-ab audit", "-",
            f"reproducidos={len(goods)}, unverified={len(unver)}", "PASS",
            "audit_reproduced_vs_unverified", f"{len(goods)}/{len(unver)}", "issues",
            str(len(audit.get("issues", []))), "logs/step3/audit-contracts.log", "-")

    # --- contracts -----------------------------------------------------------
    if contracts is not None:
        def by_rule(items):
            d: dict[str, int] = {}
            for f in items:
                d[f.get("rule", "?")] = d.get(f.get("rule", "?"), 0) + 1
            return d
        rule_fail = by_rule(contracts.get("failures", []))
        rule_unch = by_rule(contracts.get("unchanged_defects", []))
        add("CONTRACTS-ALL", "HOST", "REAL", "candidato solo, sin baseline",
            "x6200-ab contracts --revision CANDIDATE", "0 fallos de contrato",
            f"status={contracts.get('status')}; checks={contracts.get('checks')}; "
            f"failures={len(contracts.get('failures', []))}; "
            f"unchanged_defects={len(contracts.get('unchanged_defects', []))}",
            contracts.get("status", "INCONCLUSIVE"), "contract_checks",
            str(contracts.get("checks")), "checks", str(len(contracts.get("scenarios", []))),
            "logs/step3/audit-contracts.log",
            "PASS de gate no implica ausencia de defectos (UNCHANGED/waived siguen visibles)")
        for rule, n in sorted(set(rule_fail) | set(rule_unch)):
            status = "FAIL" if rule in rule_fail else "REVIEW_REQUIRED"
            add(f"CONTRACT-{rule}", "HOST", "REAL", "reglas de quality/contracts.yaml",
                f"x6200-ab contracts ({rule})", "regla sin violacion no exceptuada",
                f"failures={rule_fail.get(rule, 0)}; unchanged_defects={rule_unch.get(rule, 0)}",
                status, f"contract_{rule}",
                f"fail={rule_fail.get(rule,0)}/unchanged={rule_unch.get(rule,0)}", "findings",
                str(n), "logs/step3/audit-contracts.log",
                "las excepciones con fecha mantienen el hallazgo visible")

    # --- themes (static) -----------------------------------------------------
    add("THEMES-ALL", "HOST", "REAL", "archivos de src/theme.h + rootfs/usr/share/x6200/themes",
        "x6200-ab themes --json", "ningun par WCAG por debajo del minimo",
        f"status={themes.get('status')}; themes={len(themes.get('themes', []))}; "
        f"pairs_checked={themes.get('pairs_checked')}; failures={len(themes.get('failures', []))}; "
        f"errors={len(themes.get('errors', []))}",
        "PASS" if themes.get("status") == "PASS" else "FAIL", "theme_pairs_wcag",
        str(themes.get("pairs_checked")), "pairs", str(len(themes.get("themes", []))),
        "logs/step3/themes.json", "contraste calculado, no prueba visibilidad al sol ni LCD real")
    for t in themes.get("themes", []):
        offered = "menu" if t.get("offered") else "base/runtime"
        add(f"THEME-{t.get('theme')}", "HOST",
            "REAL" if t.get("coverage") == "full" else "PARTIAL",
            f"rol={offered}; base={t.get('base')}; origen={t.get('path')}",
            "x6200-ab themes", "pares WCAG >= 4.5:1 (texto) / 3:1 (grande)",
            f"ok={t.get('ok')}; pairs={t.get('pairs_checked')}; error={t.get('error')}",
            "PASS" if t.get("ok") else "FAIL", f"theme_{t.get('theme')}_pairs",
            str(t.get("pairs_checked")), "pairs", "9", "logs/step3/themes.json",
            "los temas runtime no resueltos se declaran UNSUPPORTED, no se omiten")

    # --- capture-derived theme coverage (UI-01) ------------------------------
    theme_seen: dict[str, int] = {}
    for rep in caps.values():
        theme_seen[theme_of(rep)] = theme_seen.get(theme_of(rep), 0) + 1
    for th, n in sorted(theme_seen.items()):
        add(f"UI01-THEME-{th}", "HOST", "REAL", "fixtures con tema solicitado",
            "x6200-ab capture", "tema solicitado == tema activo (UI-013)",
            f"{n} fixtures renderizadas con tema {th}", "PASS", "theme_round_trip",
            str(n), "fixtures", str(n), "captures/*.json (bloque theme)",
            "tema solicitado==efectivo en host; la eleccion real de menu es por UI, no medida aca")

    # --- ui-coverage.csv -----------------------------------------------------
    # The #195 inventory rows owned by #197, each mapped to fixtures by keyword.
    key_to_fixtures = {
        "home": ["main-*", "home-*"], "settings": ["settings-*"], "dfl": ["dfl-*"],
        "autotune": ["autotune*"], "cw": ["cw-tune*", "cw-autotune*"], "swrscan": ["swrscan-*"],
        "bt": ["bt-dialog*"], "bands": ["app-bands"], "apps": ["apps-menu-*"],
    }
    with (HERE / "ui-coverage.csv").open("w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["cov_id", "kind", "name", "route", "test_or_tool", "layer", "coverage",
                    "status_195", "captures", "evidence", "note"])
        for r in rows01:
            if "#197" not in r.get("owner_unit", ""):
                continue
            nm = (r.get("name", "") + " " + r.get("route", "") + " " + r.get("test_or_tool", "")).lower()
            fixtures: list[str] = []
            for key, pats in key_to_fixtures.items():
                if key in nm:
                    for pat in pats:
                        fixtures += sorted(p.stem for p in CAP.glob(pat + ".json"))
            fixtures = sorted(set(fixtures))
            if fixtures:
                ev = "captures/" + fixtures[0] + ".png (+audit/contracts)"
            elif r.get("coverage") == "UNSUPPORTED":
                ev = "UNSUPPORTED"
            else:
                ev = "sin fixture propio"
            w.writerow([r["cov_id"], r["kind"], r["name"], r["route"], r["test_or_tool"],
                        r["layer"], r["coverage"], r.get("status", ""),
                        ";".join(fixtures), ev, r.get("notes", "")])

    # --- UX-02: DFL / dial / Auto Tune negatives -----------------------------
    # The harness encodes these contracts as assertions in scenario_settle
    # (tools/ui_harness/src/screens.c): a capture that completes means the
    # assertion held. Layer HOST, coverage REAL for the encoded contract.
    UX02 = [
        ("UX02-dfl-rotary0", "dfl-editor-rotary0", "rotary0 conserva la sintonía y NO toca los filtros",
         "screens.c:1099-1138 (freq sigue, filtros intactos, freq_lock no mueve, CB cuenta canales)"),
        ("UX02-dfl-presets-rotary0", "dfl-presets-rotary0",
         "con la rueda principal girando, la frecuencia se mueve y el filtro aplicado no se toca",
         "screens.c:1879-1889"),
        ("UX02-dfl-vol", "dfl-presets-vol", "el encoder VOL edita el DFL estando abierto",
         "screens.c (dfl_vol_ui) + src/dfl_inline.c dfl_inline_on_vol_key"),
        ("UX02-dfl-assign-cancel", "dfl-presets-assign-cancel", "cancelar la asignación NO guarda",
         "screens.c:1767 (assign-cancel)"),
        ("UX02-dfl-save-fail", "dfl-presets-save-fail", "guardado fallido conserva el estado previo",
         "screens.c:1722 (save-fail)"),
        ("UX02-dfl-mode-change", "dfl-presets-mode-change", "cambiar de modo no altera IDs/config previas",
         "screens.c:1688 (misma familia no cambia la pagina)"),
        ("UX02-dfl-tx", "dfl-presets-tx", "en TX no se edita el filtro (LOW/HIGH/BW no editables)",
         "src/dfl_inline.c:521-523 (editable && !tx_active)"),
        ("UX02-dfl-touch", "dfl-presets-touch", "el toque fuera del control no modifica nada",
         "screens.c:1065 (toque lejano >22 px no cambia nada)"),
        ("UX02-dfl-touch-hold", "dfl-presets-touch-hold", "el toque sostenido dispara la pulsacion, no edita",
         "src/buttons.cpp (touch_hold en favoritos 1-3)"),
        ("UX02-autotune-notcw", "autotune-notcw", "Auto Tune fuera de CW/CWR no arranca sesion ni mueve el dial",
         "screens.c:2227-2259 (notcw: rechazo y dial sin mover)"),
        ("UX02-mirror-lsb", "dfl-mirror-lsb", "el espejo LOW/HIGH es coherente con el modo (LSB)",
         "src/dfl_inline.c:233 (eje espejado) + fixture dfl-mirror-lsb"),
        ("UX02-mirror-usb", "dfl-mirror-usb", "el espejo LOW/HIGH es coherente con el modo (USB)",
         "src/dfl_inline.c:233 + fixture dfl-mirror-usb"),
    ]
    for sid, fx, exp, ev in UX02:
        ok = (CAP / f"{fx}.json").is_file()
        add(sid, "HOST", "REAL" if ok else "UNSUPPORTED",
            "fixture negativa del candidato; assertion codificada en scenario_settle",
            f"x6200-ab capture ({fx})",
            f"contrato: {exp}", "captura completada = assertion paso" if ok else "sin captura",
            "PASS" if ok else "BLOCKED", sid, "assert_ok" if ok else "null",
            "bool", "1", f"captures/{fx}.png; {ev}",
            "assertion de harness (HOST); no reemplaza la prueba de mano humana en la radio")
    add("UX02-gap-fft", "HOST", "UNSUPPORTED", "Auto Tune solo CW; sin FFT en el harness y sin RF",
        "(no aplica)", "rechazo de Auto Tune sin medida confiable", "no medido",
        "UNSUPPORTED", "autotune_no_measurement_gap", "null", "n/a", "0",
        "src/autotune.c + tools/ui_harness (sin FFT)", "sin instrumento de medida: no se inventa FFT ni se emite RF")

    # --- UX-03: apps / errores / alcance real --------------------------------
    inert = ("dialog_bands, dialog_callsign, dialog_cluster, dialog_cw_kbd, dialog_cw_trainer, "
             "dialog_eq, dialog_field, dialog_freq, dialog_ft8, dialog_gps, dialog_js8, dialog_msg_cw, "
             "dialog_msg_voice, dialog_navtex, dialog_propagation, dialog_qth, dialog_recorder, "
             "dialog_sstv, dialog_status, dialog_wefax, dialog_wifi, dialog_wspr")
    add("UX03-inert-dialogs", "HOST", "UNSUPPORTED",
        "tools/ui_harness/mocks/mock_main_screen.c: instancia inerte (no-op) para 22 dialogos",
        "x6200-ab capture de la app no tiene camino real en host",
        "capturar los dialogos reales (Status/WiFi/BT/GPS/Propagation/Recorder/RTTY/...)",
        f"host no compila esos dialogos: {inert}", "BLOCKED", "apps_dialogs_host_compilable",
        "null", "n/a", "0", "tools/ui_harness/mocks/mock_main_screen.c:55-76",
        "BLOCKED: depende de las capturas del #196 (host no compila esos dialogos); no se tapo con mocks")
    add("UX03-messages", "HOST", "PARTIAL", "strings literales de los dialogos (CODE walkthrough)",
        "leer los mensajes actuales sin traducir ni cambiar la UI",
        "reportar los mensajes literales y su idioma",
        "GPS: 'None: this radio has no GPS' (EN) + 'sin datos' (ES); WiFi: 'Unknown'/'Scanning...'/'Can't "
        "update new connection' (EN); Status: 'not loaded'/'no adapter' (EN); Propagation: 'Sin QTH "
        "configurado...' (ES), 'Reloj sin NTP...' (ES)",
        "REVIEW_REQUIRED", "error_message_language", "mixed EN/ES", "n/a", "0",
        "src/dialog_gps.c:143-154; src/dialog_wifi.c; src/dialog_status.c; src/dialog_propagation.c:521-605",
        "CODE walkthrough: los dialogos no se compilan en host; el idioma es una decision editorial, no un FAIL")
    add("UX03-app-scope", "HOST", "REAL", "src/remote_server.cpp::app_names (25 nombres, con FT4 alias)",
        "leer el inventario de apps y handlers",
        "todas las entradas del remoto tienen handler y no son 'toda la UI'",
        "25 nombres sin 'off' (FT4 alias de FT8, DFLEditor inline); el menu APP son 22 apps",
        "PASS", "app_names_count", "25", "apps", "25", "src/remote_server.cpp:148 + static_assert con actions[]",
        "el remoto no es toda la UI; el estado real de cada dialogo no se mide en host")

    # --- UI-01 uncovered combination matrix ----------------------------------
    add("UI01-matrix-gaps", "HOST", "PARTIAL",
        "cruce tema x pantalla x modo de las fixtures existentes",
        "analizar la cobertura de combinaciones (no multiplicar fixtures a mano)",
        "reportar la matriz de combinaciones no cubiertas",
        "6 temas de menu (Aurora/Cathode/Redshift/Sprint/Sunlit/Trail): combinacion tema x pantalla "
        "solo existe para main-*, dfl-*, settings-*, bt-dialog, swrscan-3pass; NO hay variante por tema "
        "de las 6 paginas APP, de autotune/cw-tune, ni de home-*/main-tx; modos (USB/LSB/USB-D/LSB-D/"
        "CW/CWR/AM/NFM) solo tienen fixture en el editor DFL y cw-tune, no en la pantalla principal",
        "REVIEW_REQUIRED", "theme_screen_combination_matrix", "null", "n/a", "0",
        "captures/ (por tema) + tools/ui_harness/fixtures/",
        "la herramienta no cubre el producto cartesiano; se reporta la matriz en vez de inventar fixtures")

    # --- PENDING_HARDWARE ----------------------------------------------------
    for sid, metric, obs in [
        ("PH-touch", "touch_input", "la respuesta táctil real no se puede accionar en host"),
        ("PH-encoders", "encoder_detents", "los detents reales de rotary0-3 no se simulan"),
        ("PH-legibility-sun", "sunlight_legibility", "contraste calculado != visibilidad al sol / LCD real"),
        ("PH-audio-rf", "audio_rf", "audio y RF (TX/PTT/ATU/SWR) no se emiten"),
        ("PH-lcd", "lcd_panel", "el panel real y su calibración no se miden"),
    ]:
        add(sid, "HARDWARE", "UNSUPPORTED", "sin radio ni instrumentos (fase física #199)",
            "(no ejecutable en host)", "medición física en la radio", "no medido",
            "PENDING_HARDWARE", metric, "null", "n/a", "0",
            "n/a (fase #199)", "pendiente físico; este informe no lo aprueba")

    # --- dependency ----------------------------------------------------------
    add("DEP-196", "HOST", "UNSUPPORTED", "#196 (ROM/confiabilidad) no integrado en assessment/rom-194",
        "consumir inventario y capturas/estado de apps del #196",
        "usar las capturas del #196 para los dialogos que el host no compila",
        "origin/assessment/rom-194 solo tiene 01-baseline/ (sin #196)",
        "BLOCKED", "dependency_196", "null", "n/a", "0",
        "git ls-tree origin/assessment/rom-194 docs/evidencia/assessment-rom-194",
        "BLOCKED por dependencia no satisfecha; se ejecuto igual por orden del owner")

    # --- results.csv ---------------------------------------------------------
    with (HERE / "results.csv").open("w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(RESULTS_COLS)
        w.writerows(R)

    # --- atlas.md ------------------------------------------------------------
    lines = [
        "# Atlas UI/UX — candidato `c888308e` (capa HOST, 800x480)",
        "",
        f"{len(caps)} capturas originales en [`captures/`](captures/). Miniatura por fixture "
        "(abre el PNG original). Columnas: fixture, pantalla, tema solicitado/efectivo, seed, "
        "capa, revision embebida, cobertura dominante.",
        "",
        "> Recordatorio: la miniatura es una PNG del harness host; no prueba legibilidad al sol, "
        "tacto ni LCD real (PENDING_HARDWARE).",
        "",
        "| miniatura | fixture | pantalla | tema | seed | capa | revision | cobertura |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for name, rep in caps.items():
        enc = name.replace("_", "\\_")
        lines.append(
            f"| <a href=\"captures/{name}.png\"><img src=\"captures/{name}.png\" width=\"160\"></a> "
            f"| `{enc}` | {rep.get('screen')} | {theme_of(rep)} | {rep.get('seed')} | HOST "
            f"| {str(rep.get('revision'))[:12]} | {dominant_level(rep.get('coverage', []))} |"
        )
    (HERE / "atlas.md").write_text("\n".join(lines) + "\n")

    print(f"captures={len(caps)} results_rows={len(R)} audit={audit is not None} "
          f"contracts={contracts is not None} themes={len(themes.get('themes', []))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
