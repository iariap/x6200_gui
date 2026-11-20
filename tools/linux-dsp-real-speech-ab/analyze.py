#!/usr/bin/env python3
"""Métricas offline por muestra/condición/modo. STOI/ESTOI: BLOCKED si pystoi no está."""
import argparse, csv, json, os, wave, numpy as np
RATE=44100; BLOCK=441
def read16(p):
    w=wave.open(p,'rb'); x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float64); w.close(); return x
def rms(x): return float(np.sqrt(np.mean(x*x))) if len(x) else 0.0
def clip(x): return 100.0*float(np.mean((x>=32767)|(x<=-32768))) if len(x) else 0.0
def snr(ref,out):
    g=float(np.dot(out,ref)/max(1e-9,np.dot(ref,ref))); resid=out-g*ref
    return 10*np.log10(max(1e-9,np.sum((g*ref)**2))/max(1e-9,np.sum(resid**2)))
def env(x):
    n=len(x)//BLOCK; return np.array([rms(x[i*BLOCK:(i+1)*BLOCK]) for i in range(n)])
def corr(a,b):
    if len(a)<2 or np.std(a)<1e-9 or np.std(b)<1e-9: return 0.0
    return float(np.corrcoef(a,b)[0,1])
def bright(x):
    return float(np.mean(np.abs(np.diff(x)))/max(1e-9,rms(x)))
def pump(x):
    e=env(x); return float(np.std(e)/max(1e-9,np.mean(e)))
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--out',default='out'); ap.add_argument('--modes',default='B,C,D1,D2'); a=ap.parse_args()
    stoi_state='BLOCKED'
    try:
        import pystoi  # noqa
        stoi_state='AVAILABLE'
    except Exception:
        pass
    rows=[]; man=list(csv.DictReader(open(os.path.join(a.out,'manifest.csv'))))
    for m in man:
        if m['kind']!='degraded': continue
        base=m['id'].split('__')[0]; clean=read16(os.path.join(a.out,'clean',base+'.wav')); mid=read16(m['file'])
        for mode in a.modes.split(','):
            op=os.path.join(a.out,'processed',mode,m['id']+'.wav')
            if not os.path.exists(op): continue
            out=read16(op); n=min(len(clean),len(mid),len(out))
            rows.append(dict(id=m['id'],speaker=m['speaker'],condition=m['condition'],level=m['level'],mode=mode,
                len_in=len(mid),len_out=len(out),rms_gain_db=20*np.log10(max(1e-9,rms(out))/max(1e-9,rms(mid))),
                clip_pct=clip(out),snr_in_db=snr(clean[:n],mid[:n]),snr_out_db=snr(clean[:n],out[:n]),
                snr_improve_db=snr(clean[:n],out[:n])-snr(clean[:n],mid[:n]),
                env_corr=corr(env(clean[:n]),env(out[:n])),brightness=bright(out),pumping=pump(out),stoi=stoi_state))
    os.makedirs(os.path.join(a.out,'metrics'),exist_ok=True)
    p=os.path.join(a.out,'metrics','per_sample.csv')
    with open(p,'w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
    # resumen por modo/condición
    def avg(rs,k): return sum(r[k] for r in rs)/max(1,len(rs))
    keys=sorted(set((r['mode'],r['condition']) for r in rows))
    with open(os.path.join(a.out,'metrics','summary.csv'),'w',newline='') as f:
        w=csv.writer(f); w.writerow(['mode','condition','n','snr_improve_db','env_corr','clip_pct','brightness','pumping'])
        for mode,cond in keys:
            rs=[r for r in rows if r['mode']==mode and r['condition']==cond]
            w.writerow([mode,cond,len(rs),round(avg(rs,'snr_improve_db'),3),round(avg(rs,'env_corr'),4),round(avg(rs,'clip_pct'),4),round(avg(rs,'brightness'),4),round(avg(rs,'pumping'),4)])
    print('rows',len(rows),'stoi',stoi_state,'->',p)
if __name__=='__main__': main()
