# QA-LINUX-DSP-INTEGRATION — herramientas/tests

- Infraestructura integrada: `src/linux_dsp_route/` (árbitro único, sombra, IPC proxy, supervisor).
- Tests host/ARM/QEMU: `tests/linux-dsp-integration/run_tests.sh`.
- Documentación y gates: `docs/evidencia/2026-11-04-linux-dsp-integration/`.
- **DEFAULT OFF**: sólo se compila con `cmake -DX6200_LINUX_DSP_ROUTE=ON` (branch-local, QA); no se
  instala, no se arranca, no toca I2C/ALSA/radio. El build normal queda BASE.
