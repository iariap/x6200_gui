# Integración #367: MUX/guardian + variante QA RX speaker

Fecha: 2026-11-02. Worktree ASF `factory/rx-speaker-integration-367`, base PR #384 (`b46eba0b`).

## Alcance

- Se parte del router validado de #384, default OFF y fail-closed.
- Se integra el código verificado de #368 (`rx_audio_mux_guard.*` + helper) como **INACTIVO**:
  se compila y prueba en host, no se instala ni se lanza en producción. Sigue
  `R_BLOCKED_NO_HARDWARE_TRUTH`; no se finge readback.
- Se agrega una **variante QA explícita** (`-DX6200_RX_SPEAKER_QA=ON`) con:
  - `radio_audio_route_set()` serializado con el dueño GUI;
  - comandos remotos `rx_speaker_test` (0,7 s Off + 0,7 s SSB voice) y `rx_speaker_soak`
    (máximo 30 s, sólo tras dos short PASS en el mismo build);
  - preflight RX/TX/generated/captura, rechazo de reentrada y restore incierta, restauración
    explícita a BASE y snapshots antes/después.
- La build de producción no define `X6200_RX_SPEAKER_QA`: los endpoints y la ruta QA no existen
  en ella. No hay auto-start ni switch persistente DAC ON.

## Correcciones de seguridad

- El guardián #368 se conserva con G1/G2/G3 host: 9188 comprobaciones, 0 fallos; G2 mide restore
  p50=101,8 ms / p99=103,1 ms / max=103,1 ms con bus mock, **no hardware**.
- La ruta QA mantiene la obligación de `MUX_OFF` independiente de la autorización y bloquea nuevas
  activaciones si `BASE_RESTORE_UNCONFIRMED`.
- El router de producción no cambia de comportamiento: `mux_ready=false`, `NOT_ENTERED`.

## Tests host

- `run_native_tests.sh`: exit 0.
  - Router: 124/0.
  - Audio callback: 178/0 (incluye igualdad PCM recorder/router).
  - Guardián #367: 9188/0; G2 restore mock p50/p99/max 101,8/103,1/103,1 ms.
- `--ci-parity`: exit 0; `ci-parity: OK`; guardián 9188/0.
- `run_all_tests --quick`: exit 0; **23 OK, 0 fallos**; QEMU/wspr/sstv/wefax SKIP por entorno/imagen.
- ASF `check-matrix.sh`: exit 0; checks ok; QEMU **BLOCKED** por chroot/x6200-remote faltantes.
- ASan/UBSan: no disponibles en el toolchain host (falta libasan/libubsan); no se declaran PASS.
- QEMU: SKIP si no hay entorno privado.

## Build ARM QA

- `-DX6200_RX_SPEAKER_QA=ON`, Buildroot vigente, SDK X6200Control v0.1.1 + patch: **exit 0**.
- `x6200_gui` QA: 14.016.852 B, SHA256
  `b4714114da636accbf843aa50158be7f39afebc1eb496bfd080d7f3c366d1daa`,
  md5 `c9ccdae1eb4c4f986226ecab008cce6c`; gate `verifica-firmware-port.py` OK.
- Endpoints QA presentes por `strings`; build de producción no los incluye.

## Despliegue — BLOQUEADO por conectividad SSH

Preflight de la radio: GUI original `e32a81f3…`, un solo proceso, RX, `tx=false`; backup remoto
exacto en `/mnt/backups/x6200_gui.pre-qa-367-20261102T213413Z` (md5 `e32a81f3…`) y params
`params.db.pre-qa-367-…`; backup host local disponible. **No se reemplazó el binario.**

La transferencia de respaldo/subida por `scp` quedó colgada y luego el sshd de la radio empezó a
resetear la conexión durante `kex_exchange_identification` (puerto 22 abierto, ping OK). Se
reintentó con stream SSH y tras esperas; el sshd siguió reseteando. Se suspendió el despliegue para
no dejar la radio sin acceso. **No se ejecutó `rx_speaker_test`, `rx_speaker_soak` ni rollback
físico.** La radio quedó/estaba con la GUI original; se requiere power-cycle para recuperar SSH y
continuar.

## Resultado

- `INTEGRATION_CODE_READY = PASS` (host/ARM QA).
- `QA_DEPLOY = BLOCKED_SSHD_RESET`.
- `AUDIO_TESTS_PHYSICAL = NOT_EXECUTED_THIS_RUN` (la evidencia histórica del 2/11 sigue siendo la
  prueba audible).
- `CRASH_RECOVERY = NOT_VERIFIED`.
- `PRODUCTION_READY = NO`.
