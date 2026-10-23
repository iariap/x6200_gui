#!/usr/bin/env python3
"""Construye coverage-final.csv a partir del inventario unico de #195.

No mide ni inventa: adjudica documentalmente cada fila del inventario (334
items) con el veredicto que la unidad duena del item dejo en su README /
results.csv. Las reglas estan declaradas en README.md (seccion "Cobertura
consolidada") y en coverage.md.

Entradas (relativas a 05-summary/):
  ../01-baseline/coverage.csv
  ../03-ui-ux/ui-coverage.csv
Salida:
  ../coverage-final.csv
"""
from __future__ import annotations

import csv
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parent
BASE = OUT.parent

# Un solo item por cov_id: el inventario de #195 es la fuente y #197 sólo
# materializa 142 de esas filas. No se agregan items nuevos (evita doble conteo).
FIELDS = [
    "cov_id", "kind", "name", "route", "owner_unit", "layer",
    "coverage_195", "status_195", "evidence_unit", "evidence_ref",
    "status_final", "status_basis", "limitation",
]

# ---------------------------------------------------------------------------
# Overrides documentados. Cada valor es (status_final, evidence_ref, basis).
# Los items sin override conservan status_195 (PASS/PENDING/etc.) y se
# adjudican con la regla general de la unidad.
# ---------------------------------------------------------------------------
OVR: dict[str, tuple[str, str, str]] = {}

# #196 -- ROM/confiabilidad (02-rom/README.md y results.csv)
OVR.update({
    "APP-03": ("PENDING_HARDWARE", "02-rom/README.md#rom-03", "ROE real requiere TX; curva sintetica en host"),
    "APP-04": ("BLOCKED", "02-rom/README.md#71-banco-compartido-blocked-dependencia-exacta", "barrido de apps BLOCKED por banco-apps ocupado"),
    "APP-05": ("BLOCKED", "02-rom/README.md#71-banco-compartido-blocked-dependencia-exacta", "alias FT4 no barrido; mismo BLOCKED que FT8"),
    "APP-22": ("BLOCKED", "02-rom/results.csv#TST-089", "CTest QTH BLOCKED por ft8lib/runtime sanitizers"),
    "SYS-05": ("REVIEW_REQUIRED", "02-rom/README.md#7-rom-050607--banco-estres-y-reposo", "memorias/VFO/split no ejercitados por el barrido"),
    "SYS-17": ("REVIEW_REQUIRED", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "wfview server no medido; solo socket remoto"),
    "SYS-18": ("BLOCKED", "01-baseline/README.md#7-paso-5--webserver", "webserver SKIP: no se uso el volumen compartido"),
    "SYS-24": ("UNSUPPORTED", "01-baseline/coverage.csv", "QSO log/ADIF sin instrumento ni fixture"),
    "SYS-25": ("FAIL", "02-rom/README.md#82-db-corrupta--read-only--faltante-fail-crash-determinista", "DB corrupta/RO/faltante = SIGSEGV determinista; ruta normal PASS"),
    "SYS-26": ("PENDING_HARDWARE", "02-rom/results.csv#ROM-02-BOOT", "kernel/init real no ejecutado (sin QEMU system)"),
    "SYS-27": ("REVIEW_REQUIRED", "02-rom/README.md#9-hallazgos-sin-abrir-tickets", "sin watchdog; wrapper while true sin backoff (crashloop medido)"),
    "SYS-28": ("PENDING_HARDWARE", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "apagado/reinicio fisico no ejecutado"),
    "SYS-29": ("FAIL", "02-rom/README.md#6-rom-04--update-y-rollback", "empaquetador exit 1, owner 1000, sin checksum/rollback"),
    "SYS-30": ("FAIL", "02-rom/README.md#52-lo-que-realmente-pasa-evidencia-ejecutada", "tarjeta nueva crea vfat y primer boot no monta; contrato DATA ext4"),
    "SYS-39": ("PENDING_HARDWARE", "02-rom/results.csv#ROM-03-NEW-CARD", "barrido ROE real transmite; no autorizado"),
    "SYS-42": ("BLOCKED", "02-rom/results.csv#TST-089", "QTH/CTest BLOCKED por ft8lib y sanitizers"),
    "SRV-01": ("PENDING_HARDWARE", "02-rom/README.md#42-sintaxis-con-el-interprete-real-de-la-imagen-pass-2626", "solo sintaxis/orden estatico; arranque real no ejecutado"),
    "SRV-02": ("PENDING_HARDWARE", "02-rom/results.csv#ROM-02-BOOT", "requiere arranque real"),
    "SRV-03": ("PENDING_HARDWARE", "02-rom/results.csv#ROM-02-BOOT", "requiere arranque real"),
    "SRV-04": ("PENDING_HARDWARE", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "BT real no disponible en QEMU"),
    "SRV-05": ("PENDING_HARDWARE", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "BT CAT real no disponible en QEMU"),
    "SRV-06": ("FAIL", "02-rom/README.md#52-lo-que-realmente-pasa-evidencia-ejecutada", "crea p3 vfat y mount resuelve ext4: primer boot sin /mnt"),
    "SRV-07": ("FAIL", "02-rom/README.md#6-rom-04--update-y-rollback", "primera update sin integridad/rollback y backup 706/5466"),
    "SRV-10": ("PENDING_HARDWARE", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "helper Field Ops no ejercitado contra hardware"),
    "SRV-12": ("REVIEW_REQUIRED", "02-rom/README.md#82-db-corrupta--read-only--faltante-fail-crash-determinista", "daemon reinicia sin backoff; 34 inicios/10 s medidos con DB corrupta"),
    "SRV-13": ("PENDING_HARDWARE", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "sysctl vm.* no verificado en kernel real"),
    "SRV-14": ("PENDING_HARDWARE", "02-rom/README.md#6-rom-04--update-y-rollback", "restore/backup de DATA no probado; requiere medio de prueba"),
    "PKG-02": ("BLOCKED", "02-rom/results.csv#TST-088", "ft8lib no disponible en host; CTest BLOCKED"),
    "PKG-03": ("PENDING_HARDWARE", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "control de radio real no medido"),
    "BLD-02": ("REVIEW_REQUIRED", "02-rom/README.md#52-lo-que-realmente-pasa-evidencia-ejecutada", "imagen solo p1/p2 por diseno; p3 depende de procedimiento externo (F-18)"),
    "BLD-03": ("PASS", "02-rom/README.md#41-prune-del-rootfs-pass", "prune ejecutado; post-build verificado"),
    "BLD-04": ("FAIL", "02-rom/README.md#52-lo-que-realmente-pasa-evidencia-ejecutada", "fstab ext4 vs S01create_data que crea vfat"),
    "ENV-02": ("PASS", "02-rom/README.md#41-prune-del-rootfs-pass", "workflow de prune ejecutado literal"),
    "TST-019": ("REVIEW_REQUIRED", "02-rom/README.md#10-capas-cobertura-y-limitaciones", "e2e_chroot legado: inventario parcial y comprobaciones permisivas"),
    "TST-088": ("BLOCKED", "02-rom/results.csv#TST-088", "falta ft8lib/constants.h y runtime ASan/UBSan en host"),
    "TST-089": ("BLOCKED", "02-rom/results.csv#TST-089", "falta ft8lib/constants.h y runtime ASan/UBSan en host"),
    "TST-099": ("BLOCKED", "02-rom/results.csv#ROM-06-SWEEPS", "e2e de apps bloqueado por banco ocupado"),
})

# #198 -- performance/eficiencia (04-performance/README.md y results.csv)
OVR.update({
    "APP-18": ("PENDING_HARDWARE", "04-performance/README.md#11-pendientes-de-hardware-para-199", "audio real/mixer no disponible; captura sintetica"),
    "SYS-11": ("BLOCKED", "04-performance/results.csv#P05-DSP-FFT", "sin radio no llegan paquetes x6200_flow_*; FFT no medible"),
    "SYS-12": ("PASS", "04-performance/README.md#7-perf-05--audio-del-camino-digital-qemu_user-synthetic", "audio digital voz/control PASS en null-sink; no audio analogico"),
    "SYS-23": ("PENDING_HARDWARE", "04-performance/README.md#11-pendientes-de-hardware-para-199", "grabacion/AVI real no medida"),
    "SYS-34": ("PENDING_HARDWARE", "04-performance/README.md#11-pendientes-de-hardware-para-199", "espectro/waterfall reales requieren radio; modelo host medido"),
    "SYS-45": ("REVIEW_REQUIRED", "04-performance/README.md#9-hallazgos-principales-sin-abrir-tickets", "41,8 wakeups/s estaticos; sin medidor no se convierte a mA"),
})

# #199 -- pendientes de hardware (05-summary/hardware-plan.csv)
HW = {
    "APP-16": ("HW-07", "GPS"),
    "APP-19": ("HW-07", "WiFi"),
    "IN-10": ("HW-04", "PTT/microfono"),
    "SYS-01": ("HW-03", "RX/sintonia"),
    "SYS-02": ("HW-04", "TX/PTT/keyer"),
    "SYS-14": ("HW-07", "CAT"),
    "SYS-15": ("HW-07", "USB"),
    "SYS-19": ("HW-07", "WiFi"),
    "SYS-21": ("HW-07", "GPS"),
    "SYS-40": ("HW-04", "ATU"),
    "SRV-08": ("HW-07", "BT audio"),
    "SRV-09": ("HW-07", "BT CAT"),
    "SRV-11": ("HW-07", "POTA spots"),
    "SRV-15": ("HW-07", "radio_daemon/BASE"),
    "SRV-16": ("HW-03", "ALSA"),
    "SRV-17": ("HW-03", "PulseAudio"),
    "PKG-06": ("HW-07", "rtw88"),
}
for cid, (hw, why) in HW.items():
    OVR[cid] = ("PENDING_HARDWARE", f"05-summary/hardware-plan.csv#{hw}", f"no ejecutado; {why} requiere hardware/instrumento ({hw})")

OVR.update({
    "SYS-48": ("PASS", "01-baseline/README.md#5-paso-3--revision-de-documentacion-fixtures-y-pruebas", "auditoria documental ejecutada; hallazgos registrados"),
    "SRV-18": ("BLOCKED", "01-baseline/README.md#7-paso-5--webserver", "webserver SKIP por aislamiento del volumen compartido"),
    "PKG-04": ("BLOCKED", "01-baseline/README.md#7-paso-5--webserver", "webserver no ejecutado"),
    "PKG-07": ("BLOCKED", "01-baseline/README.md#7-paso-5--webserver", "dependencia del webserver no ejecutada"),
})

# #195 -- baseline (01-baseline/README.md)
OVR.update({
    "SYS-46": ("PASS", "01-baseline/README.md#9-paso-7--identidad-de-los-artefactos", "gate #86 PASS, fuente-md5 y hashes revalidados"),
    "SYS-49": ("PASS", "01-baseline/README.md#8-paso-6--artefactos-armrom-en-sandbox-privado", "build reproducible con desviacion declarada de .config"),
    "PKG-01": ("PASS", "01-baseline/README.md#8-paso-6--artefactos-armrom-en-sandbox-privado", "override local demostrado; el .mk vendor no se edito"),
    "ENV-03": ("REVIEW_REQUIRED", "01-baseline/README.md#3-paso-1--estado-refs-toolchain-y-ci", "no hay corrida CI del commit de merge; referencia PR #193"),
    "TST-020": ("REVIEW_REQUIRED", "01-baseline/README.md#6-paso-4--baseline-host-una-vez-y-complementos", "codec_docker no ejecutado como tal; codecs corridos con imagen full"),
    "TST-086": ("PASS", "01-baseline/README.md#6-paso-4--baseline-host-una-vez-y-complementos", "17 OK, 0 fallos, 5 SKIP"),
    "TST-087": ("PASS", "01-baseline/README.md#6-paso-4--baseline-host-una-vez-y-complementos", "8 pruebas de curva ROE OK"),
    "TST-090": ("PASS", "01-baseline/README.md#6-paso-4--baseline-host-una-vez-y-complementos", "8 pruebas de curva ROE OK"),
})

# #197 -- reglas para las 142 filas materializadas en 03-ui-ux/ui-coverage.csv
UI_PHYS = {
    "APP-15", "SYS-20", "SYS-38",
    "BTN-P01", "BTN-P02", "BTN-P03", "BTN-P04", "BTN-P05", "BTN-P06",
    "BTN-P07", "BTN-P08", "BTN-P09", "BTN-P10", "BTN-P11", "BTN-P12",
    "BTN-P13", "BTN-P14", "BTN-P15", "BTN-P16",
    "IN-01", "IN-02", "IN-03", "IN-04", "IN-05", "IN-06", "IN-07",
    "IN-08", "IN-09", "PKG-05",
}
UI_HOST_GAP = {
    "APP-01", "APP-14", "APP-17", "APP-20", "APP-23",
    "SCR-01", "SCR-02", "SCR-03", "SYS-06", "SYS-07",
    "SET-I01", "SET-I02", "SET-I03", "SET-I04", "SET-I05", "SET-I06",
    "SET-I07", "SET-I08", "SET-I09", "SET-I10", "SET-I11", "SET-I12",
    "SET-I13", "SET-I14", "SET-I15", "SET-I16", "SET-I17", "SET-I18",
    "SET-I19", "SET-I20", "SET-I21", "SET-I22", "SET-I23", "SET-I24",
    "SET-I25", "SET-I26", "SET-I27", "SET-I28", "SET-I29", "SET-I30",
    "SET-I31",
}
# Los items #197 con status PASS conservan PASS (render/contrato host); se
# agrega la limitacion de capa en el texto. Los PENDING se clasifican segun
# el motivo ya documentado por #197.
for cid in UI_PHYS:
    OVR[cid] = ("PENDING_HARDWARE", "03-ui-ux/ui-coverage.csv", "entrada/salida o periferico real; HOST solo inyecta eventos")
for cid in UI_HOST_GAP:
    OVR[cid] = ("REVIEW_REQUIRED", "03-ui-ux/README.md#11-limitaciones-y-pendientes-fisicos", "sin fixture/efecto runtime; no hay fallo de contrato demostrado")

def ui_row(cid: str) -> dict | None:
    for r in UI_ROWS:
        if r["cov_id"] == cid:
            return r
    return None


def adjudicate(row: dict) -> tuple[str, str, str]:
    cid = row["cov_id"]
    owner = row["owner_unit"]
    if cid in OVR:
        return OVR[cid]
    if owner == "#197":
        ui = ui_row(cid)
        ev = f"03-ui-ux/ui-coverage.csv#{cid}"
        if ui and ui.get("captures"):
            ev += " capturas=" + ui["captures"].split(";")[0] + ("…" if ";" in ui["captures"] else "")
        if row["status"] == "PASS":
            return ("PASS", ev, "HOST: fixture/contrato/estatico; no prueba input fisico ni LCD")
        if row["status"] == "PENDING_HARDWARE":
            return ("PENDING_HARDWARE", ev, "requiere hardware")
        if row["status"] == "UNSUPPORTED":
            return ("UNSUPPORTED", ev, "fuera del alcance/instrumento")
        return ("REVIEW_REQUIRED", ev, "sin veredicto de unidad; no se infiere PASS")
    if owner == "#198":
        return ("REVIEW_REQUIRED", "04-performance/results.csv", "item #198 sin mapeo 1:1; ver resultados por metrica")
    if owner == "#196":
        return ("REVIEW_REQUIRED", "02-rom/results.csv", "item #196 sin mapeo 1:1; ver resultados por escenario")
    if owner == "#199":
        return ("PENDING_HARDWARE", "05-summary/hardware-plan.csv", "hardware no ejecutado en fase A")
    if owner == "#195":
        return (row["status"] if row["status"] != "PENDING" else "REVIEW_REQUIRED",
                "01-baseline/results.csv", "inventario/baseline #195")
    return ("REVIEW_REQUIRED", "", "sin regla de adjudicacion")


def main() -> int:
    global UI_ROWS
    base = list(csv.DictReader(open(BASE / "01-baseline/coverage.csv", newline="", encoding="utf-8")))
    UI_ROWS = list(csv.DictReader(open(BASE / "03-ui-ux/ui-coverage.csv", newline="", encoding="utf-8")))

    seen = set()
    out = []
    for r in base:
        if r["cov_id"] in seen:
            raise SystemExit(f"cov_id duplicado en inventario: {r['cov_id']}")
        seen.add(r["cov_id"])
        status, ev, basis = adjudicate(r)
        out.append({
            "cov_id": r["cov_id"],
            "kind": r["kind"],
            "name": r["name"],
            "route": r["route"],
            "owner_unit": r["owner_unit"],
            "layer": r["layer"],
            "coverage_195": r["coverage"],
            "status_195": r["status"],
            "evidence_unit": r["owner_unit"],
            "evidence_ref": ev,
            "status_final": status,
            "status_basis": basis,
            "limitation": r["notes"],
        })

    with open(OUT / "coverage-final.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, lineterminator="\n")
        w.writeheader()
        w.writerows(out)

    # Resumen para stdout (se guarda en logs/build-coverage.log)
    from collections import Counter
    print("items", len(out))
    print("coverage", dict(Counter(x["coverage_195"] for x in out)))
    print("owner", dict(Counter(x["owner_unit"] for x in out)))
    print("status_final", dict(Counter(x["status_final"] for x in out)))
    print("tests_final", dict(Counter(x["status_final"] for x in out if x["kind"] == "test")))
    print("ui_rows", len(UI_ROWS), "ui_in_inventory", sum(1 for x in out if x["owner_unit"] == "#197"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
