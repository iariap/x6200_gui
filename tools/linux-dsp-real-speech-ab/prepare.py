#!/usr/bin/env python3
"""Prepara corpus (CMU ARCTIC 16k) a 44.1k mono y genera degradaciones simuladas reproducibles.
No versiona audio; escribe manifest con hashes. Fuente: CMU ARCTIC (festvox)."""
import argparse, csv, hashlib, os, wave, numpy as np
RATE=44100; DUR=1.2; N=int(RATE*DUR)
def sha(p):
    h=hashlib.sha256(); h.update(open(p,'rb').read()); return h.hexdigest()
def read16(p):
    w=wave.open(p,'rb'); assert w.getnchannels()==1 and w.getsampwidth()==2
    n=w.getnframes(); x=np.frombuffer(w.readframes(n),dtype='<i2').astype(np.float64); sr=w.getframerate(); w.close(); return x,sr
def resample(x,sr,to=RATE):
    n=int(round(len(x)*to/sr)); idx=np.linspace(0,len(x)-1,n); return np.interp(idx,np.arange(len(x)),x)
def write16(p,x):
    w=wave.open(p,'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
    w.writeframes(np.clip(np.round(x),-32768,32767).astype('<i2').tobytes()); w.close()
def noise(n, rng): return rng.standard_normal(n)
def bandnoise(n, rng):
    f=np.fft.rfft(rng.standard_normal(n)); freqs=np.fft.rfftfreq(n,1/RATE); f[(freqs<300)|(freqs>3000)]=0; return np.fft.irfft(f,n)
def adjacent(n, rng, f):
    t=np.arange(n)/RATE; return np.sin(2*np.pi*(f+rng.uniform(-20,20))*t)+0.3*np.sin(2*np.pi*2*f*t)
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--corpus',default='corpus'); ap.add_argument('--out',default='out'); a=ap.parse_args()
    os.makedirs(a.out,exist_ok=True); os.makedirs(os.path.join(a.out,'clean'),exist_ok=True); os.makedirs(os.path.join(a.out,'degraded'),exist_ok=True)
    rng=np.random.default_rng(20261104); man=[]
    for fn in sorted(os.listdir(a.corpus)):
        if not fn.endswith('.wav'): continue
        x,sr=read16(os.path.join(a.corpus,fn)); y=resample(x,sr)[:N]
        if len(y)<N: y=np.pad(y,(0,N-len(y)))
        sp,ph=fn[:-4].split('_'); base=f'{sp}_{ph}'
        clean=os.path.join(a.out,'clean',base+'.wav'); write16(clean,y)
        man.append(dict(kind='clean',id=base,speaker=sp,phrase=ph,condition='clean',level=0,file=clean,sha256=sha(clean),license='CMU ARCTIC (festvox); ver corpus/manifest.md'))
        rms=np.sqrt(np.mean(y*y))+1e-9
        def add(cond,lvl,sig):
            z=y+sig; p=os.path.join(a.out,'degraded',f'{base}__{cond}_{lvl}.wav'); write16(p,z)
            man.append(dict(kind='degraded',id=f'{base}__{cond}_{lvl}',speaker=sp,phrase=ph,condition=cond,level=lvl,file=p,sha256=sha(p),license='CMU ARCTIC + degradacion sintetica (numpy seed 20261104)'))
        for lvl,db in enumerate([-12,-6,0]):
            add('white_noise',lvl+1,noise(N,rng)*(rms*10**(db/20)/max(1e-9,np.std(noise(N,np.random.default_rng(1))))))
        for lvl,db in enumerate([-12,-6,0]):
            z=bandnoise(N,rng); z=z/max(1e-9,np.std(z)); add('band_noise',lvl+1,z*rms*10**(db/20))
        for lvl,db in enumerate([-15,-8,-2]):
            z=adjacent(N,rng,1800.0); add('adjacent_qrm',lvl+1,z*rms*10**(db/20))
        for lvl,amp in enumerate([0.3,0.6,1.0]):
            z=np.zeros(N); step=max(1,int(N//20))
            for k in range(0,N,step): z[k:k+max(1,step//6)]=amp*rms*4
            add('impulses',lvl+1,z)
        for lvl,g in enumerate([0.15,0.4,0.8]):
            z=y.copy(); z[:N//2]*=g; z[N//2:]*=1.0/g; add('level_steps',lvl+1,z)
    with open(os.path.join(a.out,'manifest.csv'),'w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(man[0].keys())); w.writeheader(); w.writerows(man)
    print('files',len(man),'out',a.out)
if __name__=='__main__': main()
