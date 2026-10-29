#!/bin/sh
# Descarga y verifica LibriSpeech test-clean FUERA del repositorio.
# Uso: OUT=/ruta/fuera/del/repo sh fetch_corpus.sh
set -eu
OUT=${OUT:?falta OUT}
URL=https://www.openslr.org/resources/12/test-clean.tar.gz
MD5=32fa31d27d2e1cad72775fee3f4849a9

mkdir -p "$OUT"
if [ ! -f "$OUT/test-clean.tar.gz" ]; then
    curl -L --retry 3 --fail -o "$OUT/test-clean.tar.gz" "$URL"
fi
echo "$MD5  $OUT/test-clean.tar.gz" | md5sum -c -
if [ ! -d "$OUT/LibriSpeech/test-clean" ]; then
    tar -xzf "$OUT/test-clean.tar.gz" -C "$OUT"
fi
echo "corpus listo en $OUT/LibriSpeech/test-clean"
