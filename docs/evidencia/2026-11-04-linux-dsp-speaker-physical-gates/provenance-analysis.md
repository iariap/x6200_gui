# Provenance Linux-DSP vs audio BASE

## Qué se observó (y qué no prueba)
| hecho | evidencia | NO prueba |
|---|---|---|
| gate QA OK, IPC conectado, wiring READY | gui-activation.log L1-2 | nada sobre el MUX físico |
| PCM post-Speex fresco y ENABLE | gui-activation.log L185-186 | que el audio audible venga del DSP Linux |
| `activation err=0 state=ACTIVE` | gui-activation.log L186 | que BASE no siga sonando en paralelo |
| 2 sink-inputs (claim del md) | session md; **sin salida `pactl` capturada** | que correspondieran al flujo esperado |
| audio audible | relato del owner | diferencia DSP vs native, ni nivel/spectro |
| BASE fw 1.0.8, `shutdown_safe physical_confirmed=0` | service logs | estado MUX observado; timing de restore |

## Gaps de provenance
1. Sin A/B OFF/ON con la **misma fuente**, mismo VFO/modo/volumen y **parámetros DSP nativos
   congelados** ⇒ la diferencia audible podría ser bypass/native, no Linux DSP.
2. Sin grabación externa ni medición espectral/nivel ⇒ "audio audible" no es diferencia atribuible.
3. Sin verificación de que el recorder/router compartan exactamente la misma etapa post-Speex en la
   sesión (sólo chequeo estático del código, `offline-pcm-provenance.md`).
4. Persisted state: `state-prephysical.json` (vol=20, nr=0) vs el md dice "volumen 0": el estado
   previo/rollback no está consistente ⇒ riesgo de confundir preset/persistencia con efecto Linux.
5. Sink-inputs no instrumentados: no hay evidencia de que fueran 2 ni de la ruta QA.
6. MUX físico no observable: sin readback ni observación independiente.

## Clasificación
`LINUX_PCM_PROVENANCE = PARTIAL` (OFFLINE+ACTIVATION evidence; falta aislamiento y observación del
flujo físico). `LINUX_DSP_AUDIBLE_DIFFERENCE = PENDING/BLOCKED`. No se promueve PARTIAL a PASS.
