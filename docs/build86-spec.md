# Spec: build-artifact-86 — construyendo desde el `main` del remoto sale un binario de 776 KB que NO es la ROM

**Fecha**: 2026-10-15 · Corrida: `factory/build-artifact-86` · Base: trunk actual
**Issue**: #86 (QA) — build limpio desde `fork/main` (e1476ec, 267 commits detrás del port)
deja `x6200_gui` de **776.884 bytes** (PIE) con exit 0; el del port son **13.737.784 bytes**
(non-PIE). El riesgo: alguien despliegue eso creyendo que es el firmware válido.

## Tarea (investigar la RAÍZ del build y hacer el gate fail-closed)
1. **Por qué el port no compila desde main**: los archivos del port no están (`src/remote_server.cpp`,
   etc.) — el build del X6100 llega a exit 0 con 0 errores y dejan **4 archivos `.files-list.*before`**
   faltantes (`comm: No such file` en el log). MIRAR el repo de build:
   - ¿Qué es el "build x6200-gui-custom" y cuál es el paquete que compila `x6200-gui-rebuild`
     en `main`? El `x6200-gui.mk` del external de Buildroot o el del vendor con otro nombre
     (`x6100-gui.mk`)? El commit `e1476ec` Bump version line, mis compras de build avzhe la
     del X6100 776K: INS tom en la VEN (**el vendor instala un binario melodía del propio
     X6100 con el tamaño ~776K**). Comparar: es el binary del build limpio un archivo
     de composición distinta? (es **PIE**: el port es **non-PIE**), by 187 `x6200` strings
     → es un mix: **fuerte candidato: un C wrapper**.
2. **Investigar el perfil de make**: TODOS los builds tuvieron exit 0 en `main` sin
   atar aquí: el `make x6200-gui-rebuild` corre un build inférieur a `x6200-gui-custom`
   (copia del repo) y `make` no se queja si no hay target — el resultado puede ser el
   **x6200_gui VIEJO del branch (buffer cacheado)** (el build quedó con un binario vencido
   del branch default). El fallable: **el `exit 0` es del copy y no compila nada nuevo**
   (el log no muestra a compilers). Documentar TODO con evidencia reproducible.
3. **El fix: un guard determinista en el `make` / script del banco** (clase del issue:
   no es el build lo que repara; es el GATE que evita desplegar un binario invalazio):
   - Verificación de ROM (determinista): el binario `x6200_gui` construido es del PRODUCTO
     port: tamaño **min** (no 776K: hay un floor sobre — calibrarlo con una build del port
     generando el guard), y **string port-marker** (`grep -c x6200_daemon` / el strings del
     manifest) y **non-PIE** (el del port) — buscar una señal de probe robusta al build de
     cualquier branch y documentarlas con datos antes de fijarlas (comparar los binarios
     válidos del port con este de main).
   - Puede ser **CHECK en el GATE access del banco que solo vale si el binario es el
     firmware** (deterministic checks).
4. **Test TDD**: un test del guard (runtime# del fixture: entregan un亲近二进制 del test
   (esto va a host linux — el test puede crear un fake check (simulados) — el test del
   guard (shell/python estático en `tools/`, testeable con salida real).
5. **CI**: si el guard entra al banco de QA radio, el flujo de QA lo usa → documentarlo
   en el skill de QA radio de esta CORRIDA.

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/build-artifact-86
bash -n touto o nouveau script
tests/check_harness_determinism.sh
cd tools/ab_testing && uv run x6200-ab contracts --revision HEAD
uv run pytest -m "not e2e" -q
tests/native/run_native_tests.sh
estáticos 7/7
```

## Invariantes
- NO tocar los HOSES de build del OWNER (volúmenes compartidos — si el banco es otra vez
  el recurso compartido: los builds de main al chroot read-only, throwaway).
- Commits `Pablo Iaria <iariap@gmail.com>` — **NO push/PR**.

## Reporte final
1. La RAÍZ del build desde main (compare los dois builds y qué falla silenciosamente).
2. El guard (diff + fail-closed-TDD + el fds de señales calibrados).
3. Los tests (nuevos) + gates completos con salida real.
4. PENDIENTES honestos (revisión física/dcl delay del QA).
