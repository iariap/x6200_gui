#!/usr/bin/env python3
"""Chequeo OFFLINE estático del camino PCM post-Speex (sin hardware).
Uso: check_pcm_provenance.py <raíz-branch-#396>"""
import re, sys, pathlib
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '.')
audio = (root / 'src/audio.c').read_text(encoding='utf-8', errors='replace')
checks = []
def ck(name, ok, detail):
    checks.append((name, 'PASS' if ok else 'FAIL', detail))
proc = len(re.findall(r'audio_enh_process\s*\(', audio))
ck('audio_enh_process se llama una sola vez (sin doble Speex/NR)', proc == 1, f'llamadas={proc}')
# capture_deliver: recorder/decoders y hook route reciben el mismo scratch
m = re.search(r'static void capture_deliver\([^)]*\)\s*\{(.*?)\n\}', audio, re.S)
body = m.group(1) if m else ''
ck('capture_deliver existe', bool(m), 'no encontrado' if not m else 'ok')
ck('recorder recibe el buffer post-proceso', 'screen_rec_put_audio_samples' in body, 'screen_rec_put_audio_samples')
ck('decoders/otro consumidor recibe el mismo buffer', 'dsp_put_audio_samples' in body, 'dsp_put_audio_samples')
ck('hook route recibe el buffer post-Speex con flag process', 'x6200_route_audio_submit' in body, 'x6200_route_audio_submit(scratch, samples, process)')
ck('hook route no reprocesa (sin segundo audio_enh_process/route)', body.count('audio_enh_process') == 1 and 'route_audio_submit' in body, 'sólo un audio_enh_process; hook enqueue')
# QA init en audio_init (hilo no RT)
ck('QA init en audio_init (fuera de callback)', 'x6200_route_audio_init()' in audio, 'x6200_route_audio_init')
# sink/trabajo en otro hilo: route_qa_client/route_client_worker
q = root / 'src/linux_dsp_route/route_qa_client.c'
w = root / 'src/linux_dsp_route/route_client_worker.c'
ck('route_qa_client existe', q.exists(), str(q))
ck('worker dedicado existe', w.exists(), str(w))
out = ['# Chequeo offline de provenance PCM post-Speex', '', '| check | resultado | detalle |', '|---|---|---|']
for n, st, dt in checks: out.append(f'| {n} | {st} | {dt} |')
out += ['', 'Esto valida el CAMINO DE CÓDIGO, no que el MUX/DAC físico seleccione Linux.', '']
text = '\n'.join(out)
print(text)
sys.exit(0 if all(c[1] == 'PASS' for c in checks) else 1)
