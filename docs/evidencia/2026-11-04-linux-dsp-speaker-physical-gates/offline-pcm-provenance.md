# Chequeo offline de provenance PCM post-Speex

| check | resultado | detalle |
|---|---|---|
| audio_enh_process se llama una sola vez (sin doble Speex/NR) | PASS | llamadas=1 |
| capture_deliver existe | PASS | ok |
| recorder recibe el buffer post-proceso | PASS | screen_rec_put_audio_samples |
| decoders/otro consumidor recibe el mismo buffer | PASS | dsp_put_audio_samples |
| hook route recibe el buffer post-Speex con flag process | PASS | x6200_route_audio_submit(scratch, samples, process) |
| hook route no reprocesa (sin segundo audio_enh_process/route) | PASS | sólo un audio_enh_process; hook enqueue |
| QA init en audio_init (fuera de callback) | PASS | x6200_route_audio_init |
| route_qa_client existe | PASS | /home/pablo/desa/x6200/x6200-linux-dsp-recovery-hardening/src/linux_dsp_route/route_qa_client.c |
| worker dedicado existe | PASS | /home/pablo/desa/x6200/x6200-linux-dsp-recovery-hardening/src/linux_dsp_route/route_client_worker.c |

Esto valida el CAMINO DE CÓDIGO, no que el MUX/DAC físico seleccione Linux.
