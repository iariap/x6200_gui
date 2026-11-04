#!/usr/bin/env python3
"""Paquete A/B ciego C/D1/D2/bypass, orden aleatorio + answer key + planilla. Sin operador: PENDING."""
import argparse, csv, os, random, wave, numpy as np
MODES=['B','C','D1','D2']
def read16(p):
    w=wave.open(p,'rb'); x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float64); w.close(); return x
def write16(p,x):
    w=wave.open(p,'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100)
    w.writeframes(np.clip(np.round(x),-32768,32767).astype('<i2').tobytes()); w.close()
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--out',default='out'); ap.add_argument('--seed',type=int,default=20261104); a=ap.parse_args()
    man=list(csv.DictReader(open(os.path.join(a.out,'manifest.csv')))); rng=random.Random(a.seed)
    os.makedirs(os.path.join(a.out,'blind'),exist_ok=True)
    key=[]; rows=[]
    for m in man:
        if m['kind']!='degraded' or m['condition'] not in ('white_noise','adjacent_qrm','level_steps'): continue
        outs={mode:os.path.join(a.out,'processed',mode,m['id']+'.wav') for mode in MODES}
        if not all(os.path.exists(p) for p in outs.values()): continue
        raw={mode:read16(outs[mode]) for mode in MODES}
        # Nivel perceptual: objetivo = mínimo RMS de los candidatos => ninguno se amplifica/clipea;
        # mismo factor aplicado por muestra y documentado (no oculta pumping/NR, sólo iguala nivel).
        target=min(float(np.sqrt(np.mean(x*x))) for x in raw.values()) or 1.0
        items=[]
        for mode in MODES:
            x=raw[mode]*(target/max(1e-9,float(np.sqrt(np.mean(raw[mode]*raw[mode])))))
            items.append((mode,x))
        rng.shuffle(items); d=os.path.join(a.out,'blind',m['id']); os.makedirs(d,exist_ok=True)
        for i,(mode,x) in enumerate(items):
            label=chr(ord('A')+i); write16(os.path.join(d,f'{label}.wav'),x); key.append(dict(sample=m['id'],label=label,true_mode=mode))
        rows.append(dict(sample=m['id'],speaker=m['speaker'],condition=m['condition'],level=m['level'],segments=len(items)))
    with open(os.path.join(a.out,'blind','answer-key.csv'),'w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=['sample','label','true_mode']); w.writeheader(); w.writerows(key)
    with open(os.path.join(a.out,'blind','listening-results-template.csv'),'w',newline='') as f:
        w=csv.writer(f); w.writerow(['sample','label','comprehension_1_5','clarity_1_5','naturalness_1_5','artifacts_1_5','effort_1_5','global_1_5','no_difference','notes'])
        for r in rows:
            for i in range(r['segments']): w.writerow([r['sample'],chr(ord('A')+i),'','','','','','','',''])
    print('blind samples',len(rows),'segments',len(key),'->',os.path.join(a.out,'blind'))
if __name__=='__main__': main()
