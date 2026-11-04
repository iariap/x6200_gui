#!/usr/bin/env python3
"""Medición OFFLINE de recovery (≤250 ms) con referencia de tiempo independiente.
Entradas: --events CSV (t_ms,event=command|detection) y --audible CSV (t_ms,audio=base)
  con timestamps de una grabadora externa. Sin referencia externa NO se mide: BLOCKED.
Modo --self-test: sintético, sin hardware."""
import argparse, csv, io, sys
TARGET=250.0
def read_csv(path):
    rows=[]
    with open(path, newline='', encoding='utf-8') as f:
        for r in csv.DictReader(f): rows.append(r)
    return rows
def evaluate(events, audible, external_reference):
    if not external_reference: return 'BLOCKED', ['sin referencia de tiempo independiente'], []
    det=[float(r['t_ms']) for r in events if r.get('event')=='detection']
    cmd=[float(r['t_ms']) for r in events if r.get('event')=='command']
    aud=[float(r['t_ms']) for r in audible if r.get('audio')=='base']
    if not cmd or not aud: return 'BLOCKED', ['faltan command/audible'], []
    rows=[]
    for c in cmd:
        nxt=[a for a in aud if a>=c]
        if not nxt: rows.append((c,None,None)); continue
        a=nxt[0]; d=[x for x in det if x<=c]
        rows.append((c, a-c, (c-d[-1]) if d else None))
    measured=[r[1] for r in rows if r[1] is not None]
    if not measured: return 'BLOCKED', ['sin transición audible posterior'], rows
    worst=max(measured); ok=worst<=TARGET
    notes=[f'medido command→audible worst={worst:.1f} ms (n={len(measured)}), objetivo {TARGET:.0f} ms']
    if rows and rows[0][2] is not None: notes.append(f'detección→command ejemplo={rows[0][2]:.1f} ms')
    return ('PASS' if ok else 'FAIL'), notes, rows
def self_test():
    events=[{'t_ms':'1000','event':'detection'},{'t_ms':'1010','event':'command'}]
    audible=[{'t_ms':'1180','audio':'base'}]
    st,notes,_=evaluate(events,audible,True)
    print('self-test external:',st,notes)
    st2,notes2,_=evaluate(events,audible,False)
    print('self-test sin ref:',st2,notes2)
    return 0 if st=='PASS' and st2=='BLOCKED' else 1
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--events'); ap.add_argument('--audible'); ap.add_argument('--external-reference',action='store_true')
    ap.add_argument('--self-test',action='store_true'); a=ap.parse_args()
    if a.self_test: return self_test()
    if not a.events or not a.audible: return 2
    st,notes,rows=evaluate(read_csv(a.events),read_csv(a.audible),a.external_reference)
    print('status:',st)
    for n in notes: print('-',n)
    for r in rows: print('row:',r)
    return 0 if st=='PASS' else 1
if __name__=='__main__': sys.exit(main())
