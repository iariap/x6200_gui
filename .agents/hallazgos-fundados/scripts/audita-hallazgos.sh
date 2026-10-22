#!/bin/sh
#
# Chequeo del registro de hallazgos. Ayuda a encontrar los problemas, no reemplaza la auditoria:
# leer y decidir sigue siendo de quien escribe.
#
#   scripts/audita-hallazgos.sh [ruta/al/HALLAZGOS.md]
#
# Revisa:
#   1. ESTRUCTURA  el indice del principio y las secciones "## N." tienen que coincidir.
#   2. REVISAR     afirmaciones que huelen a estimacion/recuerdo (con "~", "estimad", "casi",
#                  "probablemente", "deberia", "creo", "parece", "supuest", "de memoria") fuera
#                  del anexo de hipotesis.
#   3. REVISAR     secciones que traen numeros en negrita pero no dicen de donde salen
#                  (ni "medido", ni "codigo:", ni un comando entre backticks o "$ ").
#
# Sale 0 si la estructura esta bien (los REVISAR son informativos) y 1 si hay problemas de
# estructura o si se pasa --estricto y hay algo para revisar.
#
# SPDX-License-Identifier: LGPL-2.1-or-later

set -u

ESTRICTO=no
[ "${1:-}" = "--estricto" ] && { ESTRICTO=si; shift; }
DOC="${1:-docs/HALLAZGOS.md}"

[ -f "$DOC" ] || { echo "no existe el documento: $DOC" >&2; exit 2; }

ERR=0
REV=0

# ---------- 1. indice vs secciones ----------
IDX=$(mktemp); SEC=$(mktemp)
trap 'rm -f "$IDX" "$SEC"' EXIT

sed -n '1,/^---$/p' "$DOC" | grep -E '^[0-9]+\. \[' \
    | sed 's/^\([0-9][0-9]*\)\. \[\([^]]*\)\].*/\1|\2/' > "$IDX"

grep -E '^## ' "$DOC" \
    | sed -n 's/^## \([0-9][0-9]*\)\..*/\1|/p' > "$SEC"
grep -E '^## ' "$DOC" | grep -qi 'anexo' && echo 'anexo|' >> "$SEC"

echo "== estructura: indice vs secciones =="
while IFS='|' read -r n texto; do
    [ -z "$n" ] && continue
    if grep -q "^$n|" "$SEC"; then
        continue
    fi
    if echo "$texto" | grep -qi 'anexo' && grep -q '^anexo|' "$SEC"; then
        continue
    fi
    echo "  indice: '$n. $texto' no tiene seccion '## $n.' (¿se renombro o se borro?)"
    ERR=$((ERR+1))
done < "$IDX"

while IFS='|' read -r n _; do
    [ -z "$n" ] && continue
    [ "$n" = anexo ] && continue
    grep -q "^$n|" "$IDX" || { echo "  seccion '## $n.' no figura en el indice"; ERR=$((ERR+1)); }
done < "$SEC"

# ---------- 2. afirmaciones que huelen a estimacion (fuera del anexo) ----------
ANEXO=$(grep -nE '^## .*[Aa]nexo' "$DOC" | head -1 | cut -d: -f1)
[ -z "$ANEXO" ] && ANEXO=999999

echo "== revisar: estimaciones o recuerdos fuera del anexo (linea $ANEXO en adelante es el anexo) =="
awk -v anexo="$ANEXO" '
    NR >= anexo { next }
    /no est[aá] medido|no medido|no se cronometr|no se midi[oó]|Correcci[oó]n|estimaci[oó]n|no cronometrado|sin medir/ { next }
    /~[0-9]|estimad|[^a-z]casi |probablemente|deberia|debería|[^a-z]creo |[^a-z]parece|supuest|de memoria|aprox/ {
        printf "  linea %d: %s\n", NR, $0
        n++
    }
    END { exit (n > 0) ? 0 : 1 }
' "$DOC" | head -40 || true
REV=$(awk -v anexo="$ANEXO" '
    NR >= anexo { next }
    /no est[aá] medido|no medido|no se cronometr|no se midi[oó]|Correcci[oó]n|estimaci[oó]n|no cronometrado|sin medir/ { next }
    /~[0-9]|estimad|[^a-z]casi |probablemente|deberia|debería|[^a-z]creo |[^a-z]parece|supuest|de memoria|aprox/ { n++ }
    END { print n+0 }
' "$DOC")

# ---------- 3. secciones con numeros y sin origen ----------
echo "== revisar: secciones con numeros y sin decir de donde salen =="
awk '
    /^## / { sec=$0; tiene_evidencia=0; tiene_numero=0; next }
    {
        if ($0 ~ /\*\*[0-9]/ || $0 ~ /[0-9],[0-9] *s/) tiene_numero=1
        if ($0 ~ /medido|c[oó]digo:|`|\$ |dmesg|boot\.log|debugfs|readelf|du -sh|grep /) tiene_evidencia=1
        if (tiene_numero && !tiene_evidencia && !visto[sec] && sec != "") {
            printf "  %s  (numeros sin origen a la vista)\n", sec
            visto[sec]=1
            n++
        }
    }
    END { exit (n > 0) ? 0 : 1 }
' "$DOC" | head -20 || true
SIN_ORIGEN=$(awk '
    /^## / { sec=$0; ev=0; num=0; next }
    { if ($0 ~ /\*\*[0-9]/ || $0 ~ /[0-9],[0-9] *s/) num=1
      if ($0 ~ /medido|c[oó]digo:|`|\$ |dmesg|boot\.log|debugfs|readelf|du -sh|grep /) ev=1
      if (num && !ev && !v[sec] && sec != "") { v[sec]=1; n++ } }
    END { print n+0 }
' "$DOC")

echo
echo "resumen: $ERR problemas de estructura, $REV lineas para revisar, $SIN_ORIGEN secciones con numeros sin origen"

[ "$ERR" -gt 0 ] && exit 1
[ "$ESTRICTO" = si ] && [ "$REV" -gt 0 ] && exit 1
exit 0
