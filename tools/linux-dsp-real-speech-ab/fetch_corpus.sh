#!/bin/sh
# Adquisición reproducible del corpus CMU ARCTIC (voz natural, uso libre según Festvox).
# No se versionan los WAV; se verifican hashes contra corpus.sha256.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); DIR="$HERE/corpus"; mkdir -p "$DIR"
for sp in bdl slt rms clb; do for ph in a0001 a0002; do
  f="$DIR/${sp}_${ph}.wav"; [ -s "$f" ] && continue
  curl -sSL -o "$f" "http://festvox.org/cmu_arctic/cmu_arctic/cmu_us_${sp}_arctic/wav/arctic_${ph}.wav"
done; done
( cd "$HERE" && sha256sum -c "$HERE/corpus.sha256" )
echo "corpus OK ($DIR)"
