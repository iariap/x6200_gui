# Build reproducible + paquete QA (task-30)

Herramienta: `tools/linux-dsp-integration/reproducible-build.sh` — **no instala/deploya nada**;
trabaja en copia temporal (`WORK=/tmp/linux-dsp-repro`), **nunca modifica `refs/X6200Control` in place**.

## Prerrequisitos (verificados por el script)

- `refs/X6200Control` (fuente 0.15.3; `REFS_SRC` configurable).
- Toolchain Buildroot: `rom-build/clone-buildroot/build/host/share/buildroot/toolchainfile.cmake`
  (`TOOLCHAIN_FILE`).
- Sysroot ARM: `rom-build/clone-buildroot/build/host/arm-buildroot-linux-gnueabihf/sysroot`
  (`SYSROOT`, pkg-config con speexdsp/libpulse/ALSA/etc.).
- Headers ABI correctos `refs/X6200Control/include` (samples[512]/flow 552).
- `gcc` host, `cmake`, `nm`, `file`, `sha256sum`.

Si falta algo, el script imprime `SKIP: falta ... (VAR=...)` y sale 0 sin fake PASS.

## Secuencia exacta

```
cd /home/pablo/desa/x6200/x6200-linux-dsp-integration
tools/linux-dsp-integration/reproducible-build.sh            # full: patch+lib+GUI/service+OFF+tests+paquete
tools/linux-dsp-integration/reproducible-build.sh patch-only # rápido: sólo sha/aplicación/idempotencia
```

Pasos del script:
1. Verifica **sha256 del patch** contra `...patches/x6200-control-snapshot-bulk.patch.sha256`.
2. Copia la fuente a temporal y aplica el patch; comprueba símbolos y que una segunda aplicación es
   rechazada (**idempotencia**).
3. Compila/instala **libaether parcheada** (shadow_update + bulk_write 218 B) con toolchain Buildroot.
4. Configura/build **feature-ON ARM**: `x6200_gui` + `x6200_route_service`
   (`X6200_LINUX_DSP_ROUTE=ON`, `X6200_CTRL_HAVE_BULK_WRITE=ON`, lib parcheada); verifica ELF ARM
   EABI5 y símbolo `x6200_control_bulk_write` en el servicio.
5. Configura/build **normal OFF ARM** `x6200_gui`; verifica que **no** existe
   `src/linux_dsp_route`, ni `x6200_route_service`, ni símbolos `route_*` en la GUI (BASE exacto).
6. Corre `tests/linux-dsp-integration/run_tests.sh` (host/ARM/QEMU) y exige `0 fallos`.
7. Empaqueta en `$WORK/pkg`: `x6200_gui`, `x6200_route_service`, `MANIFEST.txt` (commit, sha256 patch/lib,
   aviso de no instalar) y `SHA256SUMS`.

## Resultado real (2026-11-04)

`tools/linux-dsp-integration/reproducible-build.sh` → **REPRO_RC=0, PASS reproducible build completo**
(log en `logs/repro-build.log`):
- patch sha256 `0d6dd1c73371ec6aa4d7c01f1c1ed92d32dff323de6343a717d2ae0fda876193`, idempotente;
- libaether parcheada OK; **GUI+servicio feature-ON ARM EABI5 OK** (bulk referenciado);
- **OFF BASE OK** (sin ruta/servicio/símbolos);
- tests OK (614 checks, 0 fallos);
- artefactos QA en `/tmp/linux-dsp-repro/pkg` (efímero, **no** versionados), **sin deploy/instalación**.

## Uso de artefactos QA (sólo build de prueba, autorización del owner)

1. NO copiar a la radio ni flashear en esta fase; requiere autorización explícita y protocolo físico.
2. Verificar `SHA256SUMS` y `MANIFEST.txt`.
3. Recordar: activación QA = feature ON + `X6200_CTRL_HAVE_BULK_WRITE` + gate
   (`X6200_LINUX_DSP_ROUTE_QA=1` y `/etc/x6200_route_qa.enable`), **RX_LINUX sigue bloqueado** hasta
   verificación física/readback; build normal sigue BASE.
4. Rollback: reponer GUI/firmware oficiales (1.0.8) y feature OFF; no hay cambios en MCU/FPGA.

## Gates

`SINGLE_WRITER_CONTROL=PASS offline` · `SHADOW_COHERENCE=PASS offline` ·
`MUX_PHYSICAL_STATE_VERIFICATION=BLOCKED` · `TX_PTT_ATU_ISOLATION=PASS offline` ·
`ARBITER_FAILURE_RECOVERY=PASS offline` · `INDEPENDENT_BASE_RESTORE=PASS offline` ·
`RESTORE_250MS_PHYSICAL=BLOCKED/PENDING` · `PCM_TO_SPEAKER_PHYSICAL=PENDING` ·
`PRODUCTION_READY=FAIL`. Sin PASS físico.
