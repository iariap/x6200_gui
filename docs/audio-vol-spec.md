# Spec: audio-vol-recgain — issues #26 (Rec gain mueve salida) y #30 (vol no aplica)

**Fecha**: 2026-10-15 · **Corrida**: `factory/audio-vol-recgain` · **Base**: trunk `5f07ac6` (menú 22, 24 fixtures)
**Issues**: #26 (`slider Rec gain mueve volumen de salida; el gain de entrada nunca se aplica`) y
#30 (`vol no se aplica: params tiene 8, GUI cree 0, el sink queda en silencio digital`).

## Estado heredado
- `mixer_setup()` usa 10 llamadas `system("amixer ...")` (el intento de API ALSA del `fb640dc`
  se hizo y se REVERTEO con datos: "made things worse - five of ten apps died instead of one;
  written down so it is not attempted again"). No reintentes esa vía sin entender por qué.
- Los sliders de Ajustes → Audio: `play_gain_update_cb` (correcta) y `rec_gain_update_cb`
  (tabla del #26: llama a `audio_set_play_vol` — copiar/pegar de línea 1259). El warning de
  C del build host ya dice `unused variable 'obj'` en `make_line_gain`/`make_audio_gain`
  (pre-existente, documentado en el PR #43).

## Tarea #26 (causa ya acreditada en el issue)
1. Fix: `rec_gain_update_cb` → `audio_set_rec_vol(...)` (y que el docs del slider diga la verdad).
2. **Cobertura**: fixture nueva `settings-audio.scn` (página Ajustes → Audio con los dos
   sliders + el boot del estado). Región/expected_changes propias + golden (mechanism de la
   casa). Este fixture hace doble servicio: pantalla de audio ahora sí medida (semilla UI-005
   de #11 se acerca).
3. Chequeo estático si aplica: que ningún callback de slider apunte a la función de
   otro slider (si lo podés codificar en `tests/check_settings_focus.py` o un mini-checker
   sin introducir dependencia suelta: si el checker no es natural, PENDIENTE con análisis;
   no fuerces).

## Tarea #30 (repro-first, banco)
1. **Repro en el banco** (esto decide el alcance):
   - `{"cmd":"vol","val":N}` + `{"cmd":"get"}` → ¿el estado queda 0? (el issue dice que sí).
   - Vol de la GUI por los widgets (perilla no hay; el socket/btn o ajustes-vol representado).
   - `params.db` del chroot: ¿queda `vol=8` y la GUI cree 0? (latin: ¿la lectura DB→GUI está
     rota, o el set desde eliventario?)
   - Si el chroot corre Pulse/ALSA reales: medí el sink con `parecord`/`pactl` como el issue.
     Si el banco NO tiene el stack de audio real, declará qué medible sí tenés y dejá la parte
     sink como PENDIENTE física (canal #15), con el repro interno (params↔GUI state).
2. **Cadencia del fix**: el camino `params.vol` → `audio_set_play_vol` (mixer setup + al
   cambiar por UI/remoto) → el output del panel. Un fix por parte admitida del camino, con
   test nativo donde la pieza sea pure (los setters de audio hoy tocan amixer: stubs del
   test nativo registran llamadas como los de backlight).
3. El comando `vol` del socket remoto debe terminar encontrando lo mismo que el widget
   (una sola puerta: `audio_set_play_vol`), y `update_cb` observable por get.

## Gates (salida real)
```bash
tests/check_harness_determinism.sh          # 25 fixtures x2 (con la audio nueva)
cd tools/ab_testing
uv run x6200-ab compare --a 5f07ac6 --b HEAD --experiment-id AB-AUDIO2630   # PASS
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh            # + los tests que sumes
estáticos 6/6
# + repro banco del vol (antes/después) y, si the Pulse existe, el RMS re-medido
```

## Fuera de alcance
- DSP/RF/TX; los demás sliders de audio de otro tipo (line/audio gain quedan).
- `lvgl/` pinned `74d0a816`. No retoques umbrales ni defaults de ingeniería.
- El `backlight`/profile de batería ya consolidado.

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- PENDIENTE fundado > asumido: cada pieza del camino con su evidencia; lo no reproducible
  en el banco queda para la física (#15) con el análisis del paso siguiente.
- El banco es recurso único; liberá el contenedor al cierre.

## Reporte final
1. #26: fix + fixture/golden + la salida del tocar-slider (antes/después por el banco o el USB).
2. #30: la repro (lixiones exactas: `vol` remoto, params.db vs state de la GUI, sink si cabe) y
   el fix por partes con el chequeo nativo; lo microscopic que quede para la física, fundado.
3. El fixture de audio: geoemtría (sliders) y por qué es fiel a la pantalla real.
4. Gates completos con salida real.
5. PENDIENTES honestos.
