#!/bin/sh
# capturar.sh -- capturas 800x480 de HOME para la auditoria de UI (#270).
#
# Corre el arnes de host REAL (`tools/ui_harness`, LVGL 8.3.11 a 800x480
# headless) una vez por estado, con el comando documentado de
# tools/ui_harness/README.md, y deja:
#
#   png/<out>.png          la captura (800x480, sin escalar, PNG)
#   json/<out>.json        el JSON del arnes (cobertura + inspection del arbol LVGL)
#   logs/<out>.log         stdout+stderr de esa corrida
#   logs/comandos.log      el comando exacto de cada corrida
#   logs/capturas.tsv      out, fixture, args, exit code, sha256 y bytes del PNG
#
# El binario se construye FUERA del worktree (scratch propio) para no escribir
# nada fuera de este directorio de evidencia:
#
#   docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
#     -v "$WT:/src" -v /tmp/opencode/ui-audit-home:/scratch \
#     -w /src/tools/ui_harness x6200-uiharness \
#     -lc "cmake -S /src/tools/ui_harness -B /scratch/build-a \
#              -DHARNESS_REVISION=<sha congelado> -DCMAKE_BUILD_TYPE=Release &&
#          cmake --build /scratch/build-a -j 6"
#
# Uso:  sh capturar.sh <worktree> [<scratch>]
#   <worktree> raiz del worktree de la corrida (montada en /src)
#   <scratch>  directorio del build (por defecto /tmp/opencode/ui-audit-home)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

WT=${1:?falta el worktree}
SCRATCH=${2:-/tmp/opencode/ui-audit-home}
IMG=x6200-uiharness
EV=docs/evidencia/2026-10-23-ui-audit/home
TSV="$WT/$EV/logs/capturas.tsv"
CMDLOG="$WT/$EV/logs/comandos.log"
FIX_DIR=/src/tools/ui_harness/fixtures
OUT_DIR=/src/$EV
SCN_DIR=$OUT_DIR/scenarios

: > "$CMDLOG"
printf 'out\tfixture\targs\texit_code\tpng_sha256\tpng_bytes\tpng_wh\n' > "$TSV"

# out<TAB>fixture<TAB>args extra. Un fixture que empieza con `scn/` vive en
# esta carpeta de evidencia (los cuatro estados de bateria derivados de
# home-usb-20m); el resto son los fixtures del arnes.
CAPTURAS='
home-rx-base	home-usb-20m.scn	
home-rx-tema-aurora	main-tema-aurora.scn	
home-rx-tema-cathode	main-tema-cathode.scn	
home-rx-tema-redshift	main-tema-redshift.scn	
home-rx-tema-sprint	main-tema-sprint.scn	
home-rx-tema-sunlit	main-tema-sunlit.scn	
home-rx-tema-trail	main-tema-trail.scn	
home-tx-tema-aurora	main-tema-aurora-tx.scn	
home-tx-tema-cathode	main-tema-cathode-tx.scn	
home-tx-tema-redshift	main-tema-redshift-tx.scn	
home-tx-tema-sprint	main-tema-sprint-tx.scn	
home-tx-tema-sunlit	main-tema-sunlit-tx.scn	
home-tx-tema-trail	main-tema-trail-tx.scn	
home-rx-fuente-externa-cargando	home-bateria-externa.scn	
home-rx-bateria-7	main-bateria-baja.scn	
home-rx-bateria-112-sin-fuente	main-natural-eco.scn	
home-rx-frecuencia-extrema	home-frecuencia-extrema.scn	
home-rx-espectro-agua	main-espectro-agua.scn	
home-rx-digitos-dseg7	main-frecuencia-dseg.scn	
home-rx-bateria-0	scn/ui-audit-bateria-0.scn	
home-rx-bateria-10	scn/ui-audit-bateria-10.scn	
home-rx-bateria-11	scn/ui-audit-bateria-11.scn	
home-rx-bateria-20	scn/ui-audit-bateria-20.scn	
home-rx-bateria-30	scn/ui-audit-bateria-30.scn	
home-rx-bateria-36	scn/ui-audit-bateria-36.scn	
home-rx-bateria-50	scn/ui-audit-bateria-50.scn	
home-rx-bateria-85	scn/ui-audit-bateria-85.scn	
home-rx-bateria-99	scn/ui-audit-bateria-99.scn	
home-rx-bateria-100	scn/ui-audit-bateria-100.scn	
home-rx-bateria-desconocida	scn/ui-audit-bateria-desconocida.scn	
home-rx-bateria-85-cathode	scn/ui-audit-bateria-85-cathode.scn	
home-rx-bateria-85-redshift	scn/ui-audit-bateria-85-redshift.scn	
home-rx-bateria-85-sprint	scn/ui-audit-bateria-85-sprint.scn	
home-rx-bateria-85-sunlit	scn/ui-audit-bateria-85-sunlit.scn	
home-rx-bateria-85-trail	scn/ui-audit-bateria-85-trail.scn	
home-rx-natural	main-natural.scn	--bench-natural
home-rx-base-sin-inspect	home-usb-20m.scn	
'

echo "$CAPTURAS" | while IFS='	' read -r out fixture extra; do
    [ -n "${out:-}" ] || continue

    case "$fixture" in
        scn/*) real="$SCN_DIR/$(basename "$fixture")" ;;
        *)     real="$FIX_DIR/$fixture" ;;
    esac

    case "$out" in
        home-rx-base-sin-inspect)
            # control de bytes: sin --inspect y sin --json
            args="--scenario $real --png $OUT_DIR/png/$out.png"
            ;;
        *)
            args="--scenario $real --png $OUT_DIR/png/$out.png --json $OUT_DIR/json/$out.json --inspect"
            ;;
    esac
    if [ -n "${extra:-}" ]; then
        args="$args $extra"
    fi

    printf 'docker run --rm --entrypoint bash --user %s:%s -v %s:/src -v %s:/scratch -w /scratch/build-a %s -lc "./ui_harness %s"\n' \
        "$(id -u)" "$(id -g)" "$WT" "$SCRATCH" "$IMG" "$args" >> "$CMDLOG"

    if docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
        -v "$WT:/src" -v "$SCRATCH:/scratch" -w /scratch/build-a "$IMG" \
        -lc "./ui_harness $args" > "$WT/$EV/logs/$out.log" 2>&1
    then
        code=0
    else
        code=$?
    fi

    if [ -f "$WT/$EV/png/$out.png" ]; then
        sha=$(sha256sum "$WT/$EV/png/$out.png" | cut -d' ' -f1)
        bytes=$(stat -c %s "$WT/$EV/png/$out.png")
        wh=$(identify -format '%wx%h' "$WT/$EV/png/$out.png")
    else
        sha=NA
        bytes=NA
        wh=NA
    fi

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$out" "$fixture" "${extra:-}" "$code" "$sha" "$bytes" "$wh" >> "$TSV"
    printf '%s -> exit %s (%s %s bytes)\n' "$out" "$code" "$wh" "$bytes"
done

echo "== capturas.tsv =="
cat "$TSV"
