#!/usr/bin/env bash
# Regenera la prueba de sensibilidad del handoff_test (criterio deterministico).
set -euo pipefail
cd "$(dirname "$0")/../../.."
test -f src/boot_splash.c
EV=docs/evidencia/2026-10-22-bootanimflake
FINAL="$(mktemp)"
cp src/boot_splash.c "$FINAL"

run_test() {
    local w
    w="$(mktemp -d)"
    cc -std=c11 -Wall -Wextra -Werror -DBOOTANIM_TEST -I src -I src/bootanim -I tests/bootanim \
        -o "$w/handoff_test" tests/bootanim/handoff_test.c src/boot_splash.c
    cc -std=c11 -Wall -Wextra -Werror -I src -I src/bootanim -I tests/bootanim \
        -o "$w/fake_animator" tests/bootanim/fake_animator.c
    "$w/handoff_test" "$w/fake_animator" 2>&1 || true
    rm -rf "$w"
}

# --- Sensibilidad A: espera espuria en el camino sin PIDFILE/corrupto ---
python3 - <<'PY'
import pathlib
p = pathlib.Path('src/boot_splash.c')
s = p.read_text()
old = """    if (!bs_read_pid(&pid)) {
        return true; /* sin PIDFILE o PID invalido: no hay nada que detener */
    }"""
new = """    if (!bs_read_pid(&pid)) {
        bs_sleep_ms(BOOT_SPLASH_POLL_MS); /* INYECCION DE SENSIBILIDAD A */
        return true; /* sin PIDFILE o PID invalido: no hay nada que detener */
    }"""
assert old in s
p.write_text(s.replace(old, new))
PY
{
    echo "# SENSIBILIDAD A: se inyecta una espera espuria en el camino 'sin PIDFILE / PID corrupto'"
    echo "# (bug simulado: el codigo sondea cuando no debe). El assert nuevo TIENE que fallar."
    echo "\$ cc ... -DBOOTANIM_TEST handoff_test.c boot_splash.c && ./handoff_test fake_animator"
    run_test
} > "$EV/sensibilidad-A-espera-espuria.log"
cp "$FINAL" src/boot_splash.c

# --- Sensibilidad B: no respeta la ventana de SIGUSR1 ---
python3 - <<'PY'
import pathlib
p = pathlib.Path('src/boot_splash.c')
s = p.read_text()
old = "    if (bs_wait_gone(pid, BOOT_SPLASH_USR1_WAIT_MS)) {"
new = "    if (bs_wait_gone(pid, 0)) { /* INYECCION DE SENSIBILIDAD B: no respeta la ventana de USR1 */"
assert old in s
p.write_text(s.replace(old, new, 1))
PY
{
    echo "# SENSIBILIDAD B: se inyecta un bug que NO respeta la ventana de 100 ms de SIGUSR1"
    echo "# (escala a SIGTERM enseguida). Los asserts nuevos de la espera TIENEN que fallar."
    echo "\$ cc ... -DBOOTANIM_TEST handoff_test.c boot_splash.c && ./handoff_test fake_animator"
    run_test
} > "$EV/sensibilidad-B-ventana-salteada.log"
cp "$FINAL" src/boot_splash.c
rm -f "$FINAL"

echo "=== A ==="
cat "$EV/sensibilidad-A-espera-espuria.log"
echo
echo "=== B ==="
cat "$EV/sensibilidad-B-ventana-salteada.log"
