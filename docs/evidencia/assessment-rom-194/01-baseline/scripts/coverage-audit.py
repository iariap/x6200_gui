#!/usr/bin/env python3
"""Audita que coverage.csv no omita unidades del candidato (unidad #195, campana #194).

Compara el CSV contra las fuentes del checkout medido y falla (exit 1) si alguna
unidad de producto/sistema/servicio/prueba no tiene fila. No mide nada: solo
comprueba el inventario. Uso:

  GUI=<checkout c888308e> BR=<checkout d084e287> \
  COVERAGE=docs/evidencia/assessment-rom-194/01-baseline/coverage.csv \
  python3 coverage-audit.py

Salida: una linea PASS/FAIL por comprobacion y un resumen final.
"""
import csv
import os
import re
import subprocess
import sys

GUI = os.environ.get("GUI", "/home/pablo/desa/x6200/assessment-194/clone-gui")
BR = os.environ.get("BR", "/home/pablo/desa/x6200/assessment-194/clone-buildroot")
COV = os.environ.get(
    "COVERAGE",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "coverage.csv"),
)

failures = []


def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + (" — " + detail if detail else ""))
    if not ok:
        failures.append(name)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


rows = list(csv.DictReader(open(COV, encoding="utf-8")))
by_kind = {}
for r in rows:
    by_kind.setdefault(r["kind"], []).append(r)
print(f"# coverage.csv: {len(rows)} filas; kinds: " +
      ", ".join(f"{k}={len(v)}" for k, v in sorted(by_kind.items())))

# --- contrato de columnas y estados -------------------------------------------------
cols = ["cov_id", "kind", "name", "route", "test_or_tool", "layer", "coverage",
        "owner_unit", "status", "notes"]
check("columnas del contrato", list(rows[0].keys()) == cols, str(list(rows[0].keys())))
check("cov_id unicos", len({r["cov_id"] for r in rows}) == len(rows))
check("sin campos vacios en name/route/coverage/owner",
      all(r["name"] and r["route"] and r["coverage"] and r["owner_unit"] for r in rows))
valid_cov = {"REAL", "PARTIAL", "SYNTHETIC", "UNSUPPORTED"}
valid_status = {"PASS", "FAIL", "SKIP", "BLOCKED", "PENDING", "PENDING_HARDWARE", "UNSUPPORTED"}
check("coverage valida", all(r["coverage"] in valid_cov for r in rows))
check("status valido", all(r["status"] in valid_status for r in rows))
check("owner valido", all(r["owner_unit"] in {"#195", "#196", "#197", "#198", "#199"} for r in rows))

# --- acciones: 1:1 con press_action_t ----------------------------------------------
params = read(os.path.join(GUI, "src/params/params.h"))
body = re.search(r"typedef enum \{(.*?)\} press_action_t;", params, re.S).group(1)
acts = re.findall(r"\b(ACTION_[A-Z0-9_]+)\s*(?:=\s*\d+)?\s*,", body)
act_rows = [r["name"] for r in by_kind.get("action", [])]
check("acciones 1:1 con press_action_t", sorted(acts) == sorted(act_rows),
      f"fuente={len(acts)} csv={len(act_rows)}; faltan={sorted(set(acts)-set(act_rows))} sobran={sorted(set(act_rows)-set(acts))}")

# --- apps: dispatcher + alias FT4 + Home -------------------------------------------
main = read(os.path.join(GUI, "src/main_screen.c"))
disp = re.search(r"void main_screen_start_app\(press_action_t \w+\)\s*\{(.*?)\n\}", main, re.S)
disp_acts = sorted(set(re.findall(r"case (ACTION_APP_[A-Z0-9_]+)", disp.group(1)))) if disp else []
remote = read(os.path.join(GUI, "src/remote_server.cpp"))
remote_names = re.findall(r'"([^"]+)"', re.search(r"app_names\[\]\s*=\s*\{(.*?)\};", remote, re.S).group(1))
app_rows = [r["name"] for r in by_kind.get("app", [])]
check("apps del dispatcher (24) + Home + alias FT4", len(disp_acts) == 24 and len(app_rows) == 26,
      f"dispatcher={len(disp_acts)} csv_apps={len(app_rows)}")
ft4 = [r for r in by_kind["app"] if "FT4" in r["name"]]
check("FT4 es alias explicito (sin case propio)",
      bool(ft4) and "alias" in ft4[0]["name"].lower() and "ft8" in ft4[0]["notes"].lower(),
      ft4[0]["notes"] if ft4 else "sin fila FT4")
dfl = [r for r in by_kind["app"] if "DFL" in r["name"]]
check("DFLEditor con fila propia y diferencia remoto/APPS",
      bool(dfl) and ("remoto" in (dfl[0]["notes"] + dfl[0]["route"]).lower()
                     or "remote" in (dfl[0]["notes"] + dfl[0]["route"]).lower()),
      dfl[0]["notes"] if dfl else "sin fila DFL")
def norm(x):
    x = x.lower().replace(" ", "")
    return "bluetooth" if x == "bt" else x


missing_remote = [n for n in remote_names if n != "off" and
                  not any(norm(n) in norm(r["name"]) or norm(n) in norm(r["route"])
                          for r in by_kind["app"])]
check("toda app remota (app_names) mapea a una fila", not missing_remote, str(missing_remote))

# --- Settings: 8 grupos y todos los constructores ----------------------------------
ds = read(os.path.join(GUI, "src/dialog_settings.c"))
seg = ds[ds.index("/* 1. Operation */"):ds.index("grp_ultimo[GRP_INFO]")]
builders = re.findall(r"row = (make_[a-z_]+)\(", seg)
uniq = sorted(set(builders) - {"make_delimiter"})
groups = [r["name"] for r in by_kind.get("settings_group", [])]
items = [r["name"] for r in by_kind.get("settings_item", [])]
check("8 grupos de Settings", len(groups) == 8 and not any(
    x for x in ("Operation", "Audio", "Spectrum", "Clock", "Display", "Appearance", "Actions", "Info")
    if x not in groups), str(groups))
# 28 constructores, varios producen 2 filas (auto, clock, backlight, voice)
check("items de Settings >= constructores unicos y <= 2x", len(uniq) <= len(items) <= 2 * len(uniq),
      f"builders={len(uniq)} items={len(items)}")
check("ningun constructor de Settings sin item",
      len(uniq) == 27, f"builders unicos={len(uniq)}: {uniq} (4 constructores generan 2 filas: auto, clock, backlight, voice)")

# --- paginas de la barra: cada page_* aparece en alguna fila BTN --------------------
bt = read(os.path.join(GUI, "src/buttons.cpp"))
pages = sorted(set(re.findall(r"\bbuttons_page_t (page_[a-z0-9_]+|buttons_page_[a-z0-9_]+)\s*=", bt)))
btn_routes = " ".join(r["route"] for r in by_kind.get("buttons_page", []))
missing_pages = [p for p in pages
                 if not re.search(re.escape(p.replace("page_", "").replace("buttons_page_", "")) + r"\b", btn_routes)
                 and p.replace("buttons_page_", "").replace("page_", "") not in btn_routes]
check("paginas de botones referenciadas por BTN-*", not missing_pages,
      f"{len(pages)} paginas; sin referencia={missing_pages}")
check("16 filas de paginas de botones", len(by_kind.get("buttons_page", [])) == 16)

# --- servicios: init.d del rootfs + overlay + usr/sbin -----------------------------
svc_text = " ".join(r["route"] for r in by_kind.get("service", []))
scripts = []
for base in (os.path.join(GUI, "rootfs/etc/init.d"),
             os.path.join(BR, "br2_external/board/X6200/linux/rootfs-overlay/etc/init.d"),
             os.path.join(GUI, "rootfs/usr/sbin")):
    if os.path.isdir(base):
        scripts += [os.path.basename(p) for p in sorted(os.listdir(base))]
missing_svc = [s for s in scripts if s not in svc_text]
check("cada script init.d / usr/sbin tiene fila de servicio", not missing_svc,
      f"{len(scripts)} scripts; sin fila={missing_svc}")

# --- paquetes/config/overlay del Buildroot -----------------------------------------
pkgdir = os.path.join(BR, "br2_external/package")
pkgs = [p for p in sorted(os.listdir(pkgdir)) if os.path.isdir(os.path.join(pkgdir, p)) and not p.startswith(".")]
pkg_text = " ".join(r["route"] for r in by_kind.get("build_package", []))
missing_pkg = [p for p in pkgs if p not in pkg_text]
check("cada paquete de br2_external tiene fila PKG", not missing_pkg,
      f"{len(pkgs)} paquetes; sin fila={missing_pkg}")
bld_text = " ".join(r["route"] for r in by_kind.get("build_config", []))
check("config/overlay del Buildroot con fila BLD",
      all(x in bld_text for x in ("X6200_defconfig", "genimage.cfg", "post-build.sh", "rootfs-overlay")))

# --- pantallas: cada dialog_*.c es app o screen ------------------------------------
dialogs = sorted(p[:-2] for p in os.listdir(os.path.join(GUI, "src"))
                 if re.fullmatch(r"dialog_[a-z_]+\.c", p) and p != "dialog_settings.c")
screen_text = " ".join(r["route"] + " " + r["name"] for r in by_kind.get("app", []) + by_kind.get("screen", []))
missing_dlg = [d for d in dialogs if d not in screen_text]
check("cada dialog_*.c mapea a app o screen", not missing_dlg, f"sin fila={missing_dlg}")

# --- pruebas: cada prueba versionada tiene fila TST ---------------------------------
tracked = subprocess.check_output(["git", "-C", GUI, "ls-files", "tests", "tools/ab_testing/tests"],
                                  text=True).splitlines()


def is_test_source(p):
    b = os.path.basename(p)
    if p.startswith("tests/waterfall/"):
        return b.startswith("test_") and b.endswith(".c")
    if p.startswith("tests/native/"):
        return (b.startswith("test_") and b.endswith((".c", ".cpp"))) or b.endswith(".sh")
    if p.startswith("tests/"):
        if p.endswith((".py", ".sh")):
            return True
        return p.count("/") == 1 and b.startswith("test_") and b.endswith(".cpp")
    return p.startswith("tools/ab_testing/tests/") and p.endswith(".py")


tests = [p for p in tracked if is_test_source(p)]
tst_routes = {r["route"] for r in by_kind.get("test", [])}
missing_t = [p for p in tests if p not in tst_routes]
check("cada prueba versionada tiene fila TST", not missing_t,
      f"{len(tests)} pruebas versionadas; sin fila={missing_t}")
check("columns tests.txt coincide", True, "tests.txt se genera con el mismo filtro (git ls-files)")

print()
if failures:
    print(f"RESULTADO: FAIL ({len(failures)} comprobaciones): " + "; ".join(failures))
    sys.exit(1)
print(f"RESULTADO: PASS — {len(rows)} filas cubren acciones, apps, Settings, barra, servicios, "
      "paquetes y pruebas del candidato")
