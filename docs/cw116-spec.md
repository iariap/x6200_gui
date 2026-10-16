# Spec DRAFT — #116: SIGSEGV (fallo=NULL) en el código de CW, radio sin audio

Fecha: 2026-10-16. Rama: `factory/cw-sigsegv-116` (base `223b30ae`, trunk tip).
Creado por: orquestador (spec stage). Implementación: delegada al coder.

## Contexto (del issue #116, medido en radio)

- `CRASHLOG sig=11 fallo=(nil) pc=0xb5de0064 lr=0xb5ddf714 sp=0xbeb89ae0 base=0x44ea74`
  en el log `x6200_log_2026-10-16_22-01-44.txt` (CRASHLOG=1). La GUI crasheó (pid 311 → 1119;
  el daemon 298 la relanzó — no hubo reboot del equipo) y **la radio quedó sin audio** (la GUI maneja pulse).
- Contexto inmediato: líneas `CWDBG show o=... on=...`; secuencia del owner:
  `filter_high=4500` → `vfo 1 mode=7` → `vfo 1 mode=0` → `filter_high=3000`.
- Audio descartado por partes: PulseAudio OK, mixer OK, squelch abierta → el silencio está antes del mixer.
- El QA no pudo resolver la pila: el ELF del volumen (`69c0f6e6`) no coincide con el que corre la radio
  (`3604cece0e43`). **Pero ese binario es reproducible**: es el build del trunk `23926241` (la época
  del #112; md5 fijado en la evidencia `docs/evidencia/2026-10-16-year110-banco/`).

## Objetivo (en orden — fundar antes de tocar)

1. **Reconstruir el ELF exacto** de la radio: build del commit `23926241` y **verificar `md5 ==
   3604cece0e43`** (el determinismo del build es la casa; si no coincide, NO resolver con un binario
   parecido — declararlo y parar la resolución de pila).
   **Aislamiento obligatorio**: NO usar el volumen docker compartido `x6200-build-output` ni el chroot
   `.vfix3` del banco (los usan los otros flujos); build en directorio propio del worktree.
2. **Resolver la pila** con el método del crash handler: `enlace = main + (palabra - base)`
   (mirá el handler en el código: `src/` o la lib de crash) contra `pc`/`lr`. Identificar **función +
   línea** exacta del deref NULL (objdump/nm/addr2line sobre el ELF con símbolos).
3. **Fundar el bug**: en ese sitio, por qué el puntero es NULL con la secuencia modo 7 → modo 0 /
   cambios de filtro / camino CW. Mirar el `CWDBG show` — el objeto que loguea (`o=0x...`) es candidato.
4. **Fix** sólo si el sitio es **GUI-side** (no RF/TX/DSP, no `lvgl/`): guard NULL con **TDD** (test
   nativo que reproduce el camino NULL con stubs: RED pegado con salida real → GREEN). Si el sitio
   vive en DSP/RF o requiere la radio para validar → **propuesta, no implementación** (invariante).
5. **Banco QEMU**: evidencia de humo (la GUI corre sin crash en el camino alcanzable desde el arnés;
   si el camino CW es ejercitable en algún escenario del harness, hacelo). El crash real bajo radio
   es `requires-radio` — declararlo.

## Invariantes (no romper)

- No tocar RF/TX/DSP ni `lvgl/` (pinned 74d0a816); una sola escritor por worktree; commits
  `Pablo Iaria <iariap@gmail.com>`; sin push; `.asf-active` sin commitear.
- Gates completos con salida real (nativos con `-fanalyzer`, estáticos, determinismo, A/B, contracts,
  audit, themes, ruff/ty/pytest); TODO en foreground.

## Criterios de aceptación

- ELF reconstruido con md5 verificado **o** declaración honesta de por qué no.
- Pila resuelta a función+línea con el binario correcto (o justificación).
- Fix con TDD RED→GREEN (o propuesta fundada si es territorio prohibido).
- Evidencia de banco + `requires-radio` declarados; spec commiteada `docs/cw116-spec.md`; reporte
  final con TDD, gates, HEAD+base y PENDIENTES.
