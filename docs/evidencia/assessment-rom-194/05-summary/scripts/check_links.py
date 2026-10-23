#!/usr/bin/env python3
"""Verifica enlaces locales relativos citados en los .md de 05-summary.

No valida anchors (solo existencia del archivo destino); los enlaces http(s)
se omiten. Sale 1 si falta algun destino.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
PAT = re.compile(r"\[[^\]]*\]\(([^)]+)\)")


def main() -> int:
    missing = []
    checked = 0
    for md in sorted(HERE.rglob("*.md")):
        text = md.read_text(encoding="utf-8", errors="ignore")
        # quitar bloques de codigo para no confundir ejemplos
        text = re.sub(r"```.*?```", "", text, flags=re.S)
        for raw in PAT.findall(text):
            target = raw.strip().split()[0]
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            if target.startswith("#"):
                continue
            path = target.split("#", 1)[0]
            if not path:
                continue
            dest = (md.parent / path).resolve()
            checked += 1
            if not dest.exists():
                missing.append(f"{md.relative_to(HERE)} -> {target}")
    print(f"enlaces locales verificados: {checked}")
    if missing:
        print("FALTAN:")
        for m in missing:
            print(" ", m)
        return 1
    print("OK: todos los destinos existen")
    return 0


if __name__ == "__main__":
    sys.exit(main())
