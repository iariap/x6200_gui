#!/usr/bin/env python3
"""Auditoría OFFLINE de la sesión física 2026-11-04 (no activa hardware).
Uso: audit_physical_session.py <dir-prior-session> [--out informe.md]
Separa activación (logs), sink-input (claim sin captura), audible (relato del owner),
provenance Linux (no aislada) y MUX/restore físico (no observado)."""
import argparse, json, os, re, sys

def read(p):
    try: return open(p, encoding='utf-8', errors='replace').read()
    except OSError: return ''

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('dir'); ap.add_argument('--out')
    a = ap.parse_args(); d = a.dir
    md = read(os.path.join(d, '..', 'physical-session-2026-11-04.md')) or read(os.path.join(d, 'physical-session-2026-11-04.md'))
    act = read(os.path.join(d, 'gui-activation.log'))
    svc = read(os.path.join(d, 'service.log'))
    shutdown = read(os.path.join(d, 'service-shutdown.log'))
    gui = read(os.path.join(d, 'gui.log'))
    state = read(os.path.join(d, 'state-prephysical.json'))
    rows = []
    def row(item, status, evidence, gap):
        rows.append((item, status, evidence, gap))
    act_ok = bool(re.search(r'activation err=0 state=ACTIVE', act))
    gate_ok = 'gate OK' in act and 'wiring READY' in act and 'PCM post-Speex fresco' in act
    row('Activación lógica QA (gate→IPC→ENABLE→ACTIVE)', 'CONFIRMADO (log)',
        'gui-activation.log: gate OK, IPC conectado, PCM fresco, activation err=0 state=ACTIVE', 'sólo lógico; no prueba MUX')
    row('Gate runtime QA', 'CONFIRMADO (log)' if gate_ok else 'NO CONFIRMADO',
        'gui-activation.log líneas 1-2, 185-186', 'build QA owner-authorized (no reproducido aquí)')
    row('Sink-inputs (PCM QA en sink PulseAudio)', 'NO INSTRUMENTADO',
        'claim en physical-session-2026-11-04.md; no hay salida `pactl` capturada en logs', 'falta log/comando; 2 sink-inputs no auditables')
    row('Audio audible durante la prueba', 'RELATO DEL OWNER (no instrumentado)',
        'physical-session-2026-11-04.md', 'sin grabación externa; no distingue bypass vs preset')
    row('Provenance Linux (DSP Linux vs BASE)', 'NO AISLADO',
        'no hay A/B OFF/ON con misma fuente/preset; no hay parámetros DSP nativos congelados', 'no se puede atribuir diferencia al DSP Linux')
    row('BASE fw tras cierre', 'CONFIRMADO (log)' if 'BASE fw version: 1.0.8' in svc else 'NO CONFIRMADO',
        'service.log: BASE fw version: 1.0.8', 'lectura MCU; no es estado MUX observado')
    row('Shutdown seguro / restore lógico', 'PARCIAL',
        'service.log/service-shutdown.log: shutdown_safe physical_confirmed=0', 'sin timestamp físico ni efecto audible medido; logs duplicados')
    row('MUX físico / state verification', 'BLOCKED',
        'sin readback MCU (R′ #370); sin instrumentación del MUX', 'requiere observación no invasiva o readback')
    row('Restore ≤250 ms físico', 'PENDING/NO MEDIDO',
        'no hay reloj externo ni transición audible grabada', 'protocolo de medición independiente pendiente')
    row('Rollback operador', 'PARCIAL',
        'GUI original sha256 en md, gate eliminado, sin QA/service, CB-D 27.635 USB', 'state-prephysical.json vol=20 vs md "volumen 0"; contradicción')
    if state:
        try:
            st = json.loads(state)
            if st.get('vol') != 0:
                row('Contradicción estado pre-sesión', 'DETECTADA', f'state-prephysical.json vol={st.get("vol")}',
                    'el md dice volumen restaurado 0; no coincide con snapshot pre-sesión')
        except Exception as e:
            row('state-prephysical.json', 'ILEGIBLE', str(e), '')
    out = ['# Auditoría offline de la sesión física 2026-11-04', '']
    out += ['| ítem | estado | evidencia | límite/falta |', '|---|---|---|---|']
    for r in rows: out.append('| ' + ' | '.join(x.replace('|','/') for x in r) + ' |')
    out += ['', '## Decisión', 'Activación lógica y sink-input **declarado** no son provenance Linux ni MUX físico.',
            'No se promueve ningún PARTIAL a PASS. `PRODUCTION_READY=FAIL`.', '']
    text = '\n'.join(out)
    if a.out: open(a.out, 'w', encoding='utf-8').write(text)
    print(text)
    return 0

if __name__ == '__main__':
    sys.exit(main())
