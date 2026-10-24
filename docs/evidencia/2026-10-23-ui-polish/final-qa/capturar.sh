#!/bin/bash
#
# QA final de la epica #284 (issue #287): captura la matriz COMPLETA del arnes de
# host de fuente real (LVGL 8.3.11 -> PNG 800x480) sobre el SHA candidato.
#
#   sh docs/evidencia/2026-10-23-ui-polish/final-qa/capturar.sh [WT] [SCRATCH]
#
# WT        worktree de la corrida (default: la raiz del repo de este script)
# SCRATCH   directorio del build out-of-source (default: /tmp/opencode/ui-polish-qa-284)
# EV_DIR    donde escribir png/json/logs (default: <WT>/docs/evidencia/.../final-qa)
# SOLO_CORPUS=1  captura solo el corpus del repo (205 fixtures + 13 gaps): es el
#                modo del baseline, para que las dos corridas comparen lo mismo.
#
# Que captura (una corrida = una lista, un contenedor efimero, cero recursos
# compartidos):
#   1. las 205 fixtures del corpus del arnes (tools/ui_harness/fixtures/),
#   2. las 13 de tools/ui_harness/fixtures-gaps/ (BT/WiFi inyectados, barra
#      legacy, VISION y los 3 dialogos por tecla, que necesitan
#      `ui_harness_dialogs`),
#   3. las fixtures de estado de esta carpeta (`scenarios/`: bateria --/0/100/
#      cargando/sin-externa y el reloj en TX),
#   4. la matriz de la familia segmentada de 44 px del reloj (`--digits`).
#
# Cada render escribe su PNG, su JSON con `--inspect` (arbol LVGL vivo, no
# cambia un pixel) y una fila en logs/capturas.tsv con el exit code real.
#
# El build NO se hace aca: lo hace `construir.sh` (mismo directorio), que deja
# `$SCRATCH/build/ui_harness` y `ui_harness_dialogs` con
# -DHARNESS_REVISION=<SHA candidato>.
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=${1:-$(CDPATH= cd -- "$AQUI/../../../.." && pwd)}
SCRATCH=${2:-/tmp/opencode/ui-polish-qa-284}
EV_DIR=${EV_DIR:-$WT/docs/evidencia/2026-10-23-ui-polish/final-qa}
IMAGEN=${UI_HARNESS_IMAGE:-x6200-uiharness}
CONT=${CONT:-x6200-qa284-harness}
SOLO_CORPUS=${SOLO_CORPUS:-0}

mkdir -p "$EV_DIR/png" "$EV_DIR/json" "$EV_DIR/logs" "$SCRATCH"
EV_REL=$(realpath --relative-to="$WT" "$EV_DIR")

LISTA=$SCRATCH/lista.tsv
: > "$LISTA"

# 1. corpus del arnes: nombre de salida == nombre de la fixture
for f in "$WT"/tools/ui_harness/fixtures/*.scn; do
    n=$(basename "$f" .scn)
    printf '%s\tui_harness\t/src/tools/ui_harness/fixtures/%s.scn\t\n' "$n" "$n" >> "$LISTA"
done

# 2. huecos de la auditoria #269 (BT/WiFi SYNTHETIC, legacy/VISION, 3 dialogos reales)
for f in "$WT"/tools/ui_harness/fixtures-gaps/*.scn; do
    n=$(basename "$f" .scn)
    bin=ui_harness
    case "$n" in
        dialog-freq|dialog-msg-cw|dialog-msg-voice) bin=ui_harness_dialogs ;;
    esac
    printf '%s\t%s\t/src/tools/ui_harness/fixtures-gaps/%s.scn\t\n' "gaps-$n" "$bin" "$n" >> "$LISTA"
done

if [ "$SOLO_CORPUS" = "0" ]; then
    # 3. estados propios de este QA (bateria / reloj TX)
    for f in "$EV_DIR"/scenarios/*.scn; do
        n=$(basename "$f" .scn)
        printf '%s\tui_harness\t/src/%s/scenarios/%s.scn\t\n' "$n" "$EV_REL" "$n" >> "$LISTA"
    done

    # 4. reloj de 44 px con las tres familias segmentadas (dseg7, dseg7mini,
    #    dseg14) en los seis temas + el estado TX. `--digits` fija la familia de
    #    los paneles de digito (fila de frecuencia y reloj), que es donde el
    #    reloj toma su slot de 44 px (src/clock.c: font_digit(&sony_44)).
    for d in dseg7 dseg7mini dseg14; do
        for t in aurora cathode redshift sprint sunlit trail; do
            printf 'finalqa-reloj-%s-tema-%s\tui_harness\t/src/tools/ui_harness/fixtures/main-tema-%s.scn\t--digits %s\n' \
                "$d" "$t" "$t" "$d" >> "$LISTA"
        done
    done
    printf 'finalqa-reloj-dseg7-tx\tui_harness\t/src/%s/scenarios/finalqa-reloj-tx.scn\t--digits dseg7\n' "$EV_REL" >> "$LISTA"
fi

TOTAL=$(wc -l < "$LISTA")
echo "== $TOTAL capturas, imagen $IMAGEN, contenedor $CONT (efimero) =="

docker run --rm --name "$CONT" --entrypoint bash \
    --user "$(id -u):$(id -g)" \
    -e TOTAL="$TOTAL" \
    -e EV_REL="$EV_REL" \
    -v "$WT:/src" -v "$SCRATCH:/scratch" \
    -w /scratch/build "$IMAGEN" -lc '
set -u
ev="/src/$EV_REL"
tsv="$ev/logs/capturas.tsv"
: > "$tsv"
n=0; fallos=0
while IFS=$(printf "\t") read -r out bin scn extra; do
    n=$((n + 1))
    # shellcheck disable=SC2086
    ./"$bin" --scenario "$scn" \
        --png "$ev/png/$out.png" \
        --json "$ev/json/$out.json" \
        --inspect $extra > "$ev/logs/$out.log" 2>&1
    rc=$?
    sha=$(sha256sum "$ev/png/$out.png" 2>/dev/null | cut -d" " -f1)
    printf "%s\t%s\t%s\t%s\t%s\n" "$out" "$bin" "$scn" "$rc" "${sha:-sin-png}" >> "$tsv"
    if [ "$rc" -ne 0 ]; then
        fallos=$((fallos + 1))
        echo "  FALLO $out (exit $rc)"
    fi
    if [ $((n % 40)) -eq 0 ]; then
        echo "  $n/$TOTAL"
    fi
done < /scratch/lista.tsv
echo "== $n capturas, $fallos con exit != 0 =="
' 2>&1 | tail -20

echo "== tsv: $EV_DIR/logs/capturas.tsv =="
awk -F'\t' '{print $4}' "$EV_DIR/logs/capturas.tsv" | sort | uniq -c
