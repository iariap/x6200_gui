# Plan del primer experimento físico (NO EJECUTADO)

Precondición: #363 `GATE_A=PASS` (verdad física del MUX), PR #368 rebasado/integrado con B y C
(restore independiente ≤250 ms verificado), y autorización explícita del propietario para la sesión.

## Preparación

- Radio con BASE 1.0.8 + nuestra ROM estable de `main`.
- Backup de binario/configuración y rollback ya probado.
- Operador presente; alimentación segura; sin TX/ATU.
- Instrumentación: S/P o interfaz de audio para medir la salida audible (no `pactl RUNNING`).
- Documentar frecuencias/modos/volumen y hash de ROM.

## Secuencia (2–3 minutos, sólo RX)

1. Estado inicial BASE; verificar audio original audible.
2. Iniciar captura/medición S/P; confirmar que el router ve PCM fresco y 2 bloques.
3. Ejecutar el coordinador único para seleccionar DAC; verificar por fuente física (readback/sonda)
   que el MUX cambió.
4. Medir audio procesado por el parlante; comparar tono/ruido y confirmar ausencia de loopback.
5. Inyectar un fallo controlado del escritor/GUI; medir retorno a BASE y verificar audio original en
   ≤250 ms.
6. Al primer fallo: volver a BASE por el coordinador/rollback, registrar `NO_GO` y no insistir.

## Criterios de PASS

- MUX seleccionado y **verificado físicamente**; audio procesado audible por parlante.
- Audio original vuelve y se verifica en ≤250 ms ante fallo del escritor.
- PTT/ATU/micrófonos intactos; sin TX durante la prueba.
- Bit `0x8` es el único modificado; registro real verificado antes/después.

## Estado actual

No ejecutado: faltan A y C. El router en el target está fail-closed; el experimento no se inicia.
