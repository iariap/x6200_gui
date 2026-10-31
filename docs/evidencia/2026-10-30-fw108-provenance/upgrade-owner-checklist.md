# Checklist del dueño — actualización/rollback FW X6200 1.0.8 (documentado, **NO ejecutado**)

> **Estado de esta checklist: `UPGRADE_NOT_READY`.**
>
> Motivos (bloqueantes, por orden):
> 1. **El paquete 1.0.8 descargado NO es apto para instalar** mientras el fabricante no aclare o reemita el paquete: el `sdcard.img` del ZIP oficial **no** coincide con el `Image File SHA256` que el propio fabricante declara en el release note del mismo paquete (ver `README.md` §2.4). `ARTIFACT_UNAVAILABLE` / `INTEGRITY_MISMATCH_VS_VENDOR_PUBLISHED_HASH`.
> 2. **No hay backup verificado del dueño** (versiones actuales, SD/DATA/params) ni alimentación/ventana de recuperación confirmadas.
> 3. No existe procedimiento oficial de **rollback del BASE** publicado; una microSD con firmware anterior **no** es un downgrade del BASE.
>
> Esta checklist es **documentación para el dueño**. Ningún agente del Epic #374 instala, flashea, resetea ni ejecuta nada. Los pasos se listan para que el **operador** los ejecute bajo su control, después de satisfacer las precondiciones.

## 0. Fuentes documentales usadas (citadas, no ejecutadas)

| Fuente | Qué aporta | Hash/evidencia |
|---|---|---|
| Tutorial oficial XIEGU «X6200 Firmware upgrade tutorial», WS-JSZL-2407-01 Rev 1.0, fecha 2024-07-20 — <https://xiegu.eu/?sdm_process_download=1&download_id=9241> | Procedimiento general en 2 pasos (APP por microSD; BASE por menú `FIRMWARE UPGRADE`), 13,8 V ≥3 A, batería fuera, `FACTORY RESET` al final | PDF 300 348 B, SHA256 `68ecc5a434e12080473807f5a6312fcf1f317960dd6d41a284e300d4bc877fbf` |
| Release note del paquete 1.0.8 `ReleaseNote-260820001(EN_CHS).txt` | Declara APP 1.0.8 (17-ago-2026 16:21:06), BASE 1.0.8 (19-ago-2026 16:13:08) y el `Image File SHA256` que **no** coincide con el payload | 759 B, SHA256 `2e16486969366bb42c6e9665a7f6f8981911343c723f2087e60ec0e505f9504c` |
| PDF histórico Radioddity incluido en el paquete 1.0.6 (`2025-01-15 Xiegu X6200 update V1.0.6.pdf`) | Procedimiento detallado en dos pasos, advertencias de alimentación, `FACTORY RESET` final; **es de 1.0.6**, no de 1.0.8 | 901 623 B, SHA256 `86589c152d16876d014eb1d34e94cf5d07dda41520b4efdd18459a88c45a44d3` |
| Página oficial de descargas <https://xiegu.eu/downloads/> | Distribución autorizada; entrada `X6200_Update_1.0.8 (2026-08-20)`; el paquete 1.0.8 **no** incluye PDF/readme propio | — |

Advertencias documentales: el tutorial 2024-07 menciona `x6200udimg.img` en la raíz del ZIP, mientras que el paquete actual trae `sdcard.img`; es un anacronismo del documento. El PDF detallado es de 1.0.6. **No hay instrucciones específicas de 1.0.8 publicadas en el paquete**; diferencias operativas no documentadas son `UNKNOWN`.

## 1. Precondiciones que el dueño debe cumplir ANTES de cualquier paso

- [ ] **1.1 Aclarar el paquete 1.0.8**: no continuar hasta que exista un paquete cuyo `sdcard.img` coincida con el hash declarado, o una aclaración/reemisión oficial (condiciones de desbloqueo en `README.md` §10). Verificar el SHA256 del ZIP y del `sdcard.img` **antes de escribir la tarjeta**: en el 1.0.6 el hash oficial coincide y sirve de control; en este 1.0.8 no.
- [ ] **1.2 Capturar la versión actual**: `[GEN]` → `SYSTEM` → `SYSTEM INFO`; anotar **las cuatro** cadenas (APP, FIRMWARE/BASE, FPGA, HARDWARE) y fecha/hora de la captura. (Documentado en el PDF 1.0.6 §2.2; el tutorial oficial habla de actualizar APP y baseband.)
- [ ] **1.3 Backup verificado** (fuera de la radio, con herramientas propias del dueño; no ejecutando scripts del paquete):
  - [ ] copia de la microSD/DATA actual (imagen o copia de archivos) y verificación de que **se puede leer**;
  - [ ] copia de los datos de usuario relevantes: `/usr/app_qt/xparam.db`, `/usr/app_qt/voice`, `/etc/xgradio`, `/etc/NetworkManager` (son los que el instalador de APP preserva según la lectura estática de `install_emmc.sh`; **eso es evidencia del script, no una garantía oficial de la documentación** — el backup es responsabilidad del dueño);
  - [ ] anotar si hay configuraciones WLAN/BT/red que se quieran conservar: el PDF 1.0.6 documenta que `FACTORY RESET` **borra las settings de WLAN** en 1.0.6; para 1.0.8 es `UNKNOWN`.
- [ ] **1.4 Alimentación y ventana**: fuente externa **13,8 V con ≥3 A** (documentado: 12 V mínimo en el PDF 1.0.6; el tutorial oficial pide 13,8 V y ≥3 A), batería desmontada durante todo el proceso, sin cortes de energía previstos, y tiempo para esperar ≥3 minutos de apagado/estabilización.
- [ ] **1.5 MicroSD**: ≥4 GB, lector de tarjetas, PC con Windows/Linux/macOS y herramienta de escritura de imágenes (Rufus/balenaEtcher u otra equivalente, según el tutorial/PDF).
- [ ] **1.6 Paquete y hash**: el ZIP descargado del sitio oficial debe ser el paquete a instalar; recalcular SHA256 del ZIP y del `sdcard.img` extraído y compararlos con los declarados. **Si el hash no coincide, NO escribir la tarjeta.**

**Si 1.1–1.6 no están completas y verificadas: `UPGRADE_NOT_READY` y no se avanza.**

## 2. Paso APP — actualización del sistema por microSD (documentado, no ejecutado)

Fuente: tutorial oficial 2024-07 + PDF 1.0.6 §2.1–§2.2 + notas del tutorial.

- [ ] Escribir el `sdcard.img` del paquete en la microSD con Rufus/Etcher (imagen a la **tarjeta**, no un archivo dentro de la tarjeta). Verificar que la escritura terminó y expulsar la tarjeta correctamente.
- [ ] Radio **apagada**, sin batería, con fuente 13,8 V ≥3 A conectada; insertar la microSD en la ranura del lateral derecho (contactos hacia la pantalla).
- [ ] Encender manteniendo `POWER` (hasta que aparezca el logo). El sistema **actualiza automáticamente sin intervención**; NO quitar alimentación ni pulsar teclas durante el proceso.
- [ ] Esperar el mensaje documentado de fin (**«[INFO] Update finished!»** / «update finished»), el apagado automático y que el LED junto a `POWER` vuelva a **parpadear en rojo** (puede tardar ≥3 minutos; paciencia).
- [ ] **Quitar la microSD** antes de volver a encender (si no se quita, el proceso de actualización puede repetirse en el siguiente arranque).
- [ ] Volver a encender y comprobar `[GEN]` → `SYSTEM` → `SYSTEM INFO` → APP esperada.
- [ ] **Advertencia de recuperación documentada**: si tras la actualización la pantalla queda negra pero el LED del logo queda encendido, esperar ~3 minutos a que reinicie sola; si no se quiere esperar, el tutorial indica cortar la alimentación, quitar/poner la batería y arrancar manualmente. (Esto es lo **documentado**; no se prueba aquí.)

## 3. Paso BASE/FIRMWARE — actualización del baseband (documentado, no ejecutado)

Fuente: tutorial oficial 2024-07 «Step 2» + PDF 1.0.6 §2.3.

- [ ] Sin microSD insertada; batería aún fuera; 13,8 V ≥3 A conectada.
- [ ] Encender y entrar a `[GEN]` → `SYSTEM` → **`FIRMWARE UPGRADE`**.
- [ ] En la lista de versiones disponibles, seleccionar la versión de baseband del paquete (`PREV`/`NEXT`) y pulsar **`UPGRADE`**. NO interrumpir.
- [ ] Esperar a que la barra llegue al 100 % y aparezca el fin («Firmware Upgrade: Done» según el PDF 1.0.6; ~20 s en el tutorial). Pulsar `QUIT`/`EXIT`.
- [ ] **`FACTORY RESET`** documentado como paso final (PDF 1.0.6 §2.3 paso 20; tutorial oficial paso 5): `[GEN]` → `SYSTEM` → `FACTORY RESET` y confirmar. **Esto borra configuración de usuario** (en 1.0.6 el fabricante documenta que además borra WLAN; para 1.0.8 `UNKNOWN`).
- [ ] Apagar, reconectar alimentación y arrancar; verificar en `SYSTEM INFO` que APP y FIRMWARE corresponden a lo instalado (el PDF 1.0.6 afirma que HARDWARE y FPGA no cambian durante este proceso).
- [ ] Reinsertar la batería al finalizar, según el procedimiento documentado.

## 4. Verificación posterior

- [ ] `SYSTEM INFO` muestra APP **y** FIRMWARE esperados (y FPGA/HARDWARE sin cambios respecto de la captura de 1.2).
- [ ] El equipo arranca, recibe RX, muestra waterfall y responde a la UI (verificación básica del dueño; **sin TX/PTT/ATU en esta fase del Epic** salvo que el dueño lo decida fuera de este plan).
- [ ] Anotar resultados con fecha/hora, versiones leídas y cualquier anomalía. Si algo no coincide con lo documentado: **detener y consultar al fabricante**, no repetir a ciegas.

## 5. Rollback — qué está documentado y qué no

- **No existe procedimiento oficial de downgrade del BASE** publicado por Xiegu/Radioddity en las fuentes consultadas. La actualización de BASE escribe el firmware del baseband de forma **persistente**; no se documenta cómo volver a una versión anterior.
- **Una microSD con el paquete 1.0.6 NO es un rollback del BASE.** El paso APP por microSD actualiza la parte Linux/APP (eMMC) y, como mucho, **deja el BASE 1.0.8 intacto**; no revierte el baseband. Presentar una SD alternativa como «rollback» sería incorrecto.
- El `FACTORY RESET` **no** revierte versiones: sólo deja configuración de fábrica.
- Por lo tanto: la **única forma segura** de volver atrás sería un paquete/procedimiento de downgrade **publicado o indicado por el fabricante**; no existe en las fuentes consultadas. Si el dueño necesita rollback, debe pedirlo al fabricante **antes** de actualizar.
- Riesgo explícito: actualizar APP y BASE por separado puede dejar combinaciones APP/BASE no probadas; el fabricante y el Epic no acreditan compatibilidad de combinaciones distintas a las de un paquete completo.

## 6. Riesgos (resumen operativo)

| Riesgo | Detalle | Mitigación documentada |
|---|---|---|
| Paquete no autenticado | Hash oficial ≠ hash del `sdcard.img` de este paquete 1.0.8 | No instalar hasta aclaración/reemisión (§1.1) |
| Corte de energía durante el flasheo | Puede dejar APP o BASE a medio escribir | 13,8 V ≥3 A, batería fuera, esperar ≥3 min de apagado |
| Tarjeta olvidada en la ranura | Repite la actualización de APP en el siguiente arranque | Quitarla tras «update finished» |
| `FACTORY RESET` borra configuración | En 1.0.6 documenta borrado de WLAN; 1.0.8 `UNKNOWN` | Backup + anotar configuración (§1.3) |
| BASE persistente sin downgrade | La SD/APP no revierte el BASE | Aceptar el riesgo antes de actualizar; pedir rollback al fabricante |
| Sin backup verificado | No se puede recuperar datos/config | Backup comprobado (§1.3) antes de cualquier paso |
| Instrucciones no específicas de 1.0.8 | Tutorial 2024-07 genérico + PDF de 1.0.6 | Seguir el procedimiento oficial de 2 pasos; no improvisar |

## 7. Criterio para cambiar `UPGRADE_NOT_READY` → `UPGRADE_PLAN_READY`

- [ ] Paquete 1.0.8 **reemitido/aclarado** por el fabricante con `sdcard.img` que coincida con el hash declarado (o copia auténtica del dueño con hash coincidente).
- [ ] Versiones actuales capturadas y backup verificado (§1.2–1.3).
- [ ] Fuente 13,8 V ≥3 A, batería fuera y ventana de recuperación confirmadas (§1.4).
- [ ] Decisión explícita del dueño de aceptar los riesgos §6 y de ejecutar el procedimiento **él mismo**.

Sólo entonces esta checklist puede usarse como plan de ejecución por el dueño. **Nada de esto fue ejecutado por agentes.**
