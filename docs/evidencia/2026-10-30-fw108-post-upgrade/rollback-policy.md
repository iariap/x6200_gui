# Rollback de #378

1. Apagar la radio.
2. Retirar la SD de prueba (ROM experimental).
3. Insertar la SD OEM 1.0.8.
4. Encender y verificar RX OEM.
5. No intentar downgrade del BASE/MCU: el firmware persistente quedó en 1.0.8;
   cambiar la SD no lo revierte.

El rollback fue **verificado** en la sesión anterior: apagar, retirar la SD y
encender sin SD devolvió el OEM 1.0.8 con RX y TX OK. El BASE no se tocó.
Si el OEM no recupera RX, STOP y documentar la anomalía sin improvisar.
