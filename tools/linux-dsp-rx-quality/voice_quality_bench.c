/* QA-LINUX-DSP-RX-QUALITY — banco OFFLINE con el DSP REAL (audio_enhancement.c + SpeexDSP real).
 * Fixtures sintéticas generadas y redistribuibles (no voz de terceros). NO es audio de la radio. */
#define _POSIX_C_SOURCE 200809L
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "audio_enhancement.h"

#define RATE AUDIO_ENH_SAMPLE_RATE
#define BLOCK AUDIO_ENH_FRAME_SIZE
#define NBLOCK 120          /* 1.2 s */
#define NS (BLOCK * NBLOCK)
uint32_t lv_tick_get(void) { static uint32_t t; t += 10; return t; }
static uint32_t rng = 0x12345678u;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static float frnd(void) { return (float)((rnd() >> 8) & 0xFFFF) / 32768.0f - 1.0f; }

static void synth_voice(int16_t *o, int n, float f0, float gain, bool syllabic) {
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)RATE; float v = 0;
        for (int h = 1; h <= 12; h++) {
            float f = f0 * (float)h;
            if (f > 3500.0f) break;
            float formant = 0;
            formant += 1.0f / (1.0f + powf((f - 700.0f) / 180.0f, 2));
            formant += 0.7f / (1.0f + powf((f - 1220.0f) / 250.0f, 2));
            formant += 0.35f / (1.0f + powf((f - 2600.0f) / 350.0f, 2));
            v += sinf(2.0f * (float)M_PI * f * t + (float)h * 0.1f) * formant / (float)h;
        }
        float env = syllabic ? (0.55f + 0.45f * sinf(2.0f * (float)M_PI * 4.0f * t)) : 0.8f;
        o[i] = (int16_t)fmaxf(-32767.0f, fminf(32767.0f, v * env * gain * 9000.0f));
    }
}
static void add_noise(int16_t *o, int n, float amp) { for (int i = 0; i < n; i++) { int32_t v = o[i] + (int32_t)(frnd() * amp); o[i] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); } }
static void add_tone(int16_t *o, int n, float f, float amp) { for (int i = 0; i < n; i++) { int32_t v = o[i] + (int32_t)(sinf(2*(float)M_PI*f*(float)i/(float)RATE)*amp); o[i] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); } }
static void make_fixture(int id, int16_t *clean, int16_t *mix, int n) {
    memset(clean, 0, sizeof(int16_t)*(size_t)n); memset(mix, 0, sizeof(int16_t)*(size_t)n);
    switch (id) {
        case 0: synth_voice(clean, n, 120.0f, 0.9f, true); memcpy(mix, clean, sizeof(int16_t)*(size_t)n); break; /* male */
        case 1: synth_voice(clean, n, 210.0f, 0.85f, true); memcpy(mix, clean, sizeof(int16_t)*(size_t)n); break; /* female */
        case 2: synth_voice(clean, n, 130.0f, 0.35f, true); add_noise(clean, n, 900); memcpy(mix, clean, sizeof(int16_t)*(size_t)n); break; /* weak+noise */
        case 3: for (int i=0;i<n;i++) clean[i]=(int16_t)(frnd()*2500); memcpy(mix, clean, sizeof(int16_t)*(size_t)n); break; /* band-ish noise */
        case 4: synth_voice(clean, n, 140.0f, 0.8f, true); add_noise(clean,n,300); add_tone(clean,n,1800.0f,2500); memcpy(mix,clean,sizeof(int16_t)*(size_t)n); break; /* QRM */
        case 5: for (int k=0;k<12;k++){int p=(k*997)%n; if(p+40<n) for(int j=0;j<40;j++) clean[p+j]=(int16_t)((j&1)?-20000:20000);} memcpy(mix,clean,sizeof(int16_t)*(size_t)n); break; /* impulses */
        case 6: for (int k=0;k<n;k+=BLOCK){int g=(k/BLOCK%4); int16_t a=(int16_t)(g==0?500:g==1?4000:g==2?16000:200); for(int j=0;j<BLOCK;j++) clean[k+j]=a; synth_voice(mix+k, BLOCK, 150.0f, 1.0f, false); (void)a;} break; /* level steps */
        case 7: memset(clean,0,sizeof(int16_t)*(size_t)n); memcpy(mix,clean,sizeof(int16_t)*(size_t)n); break; /* silence */
        case 8: for (int i=0;i<n;i++) clean[i]=(i&1)?32767:-32768; memcpy(mix,clean,sizeof(int16_t)*(size_t)n); break; /* max/clip */
        default: break;
    }
}
typedef struct { double rms, peak, clip_pct, snr_db, env_corr, bright, pump; } metrics_t;
static double rms16(const int16_t *x, int n) { double s=0; for (int i=0;i<n;i++) s+=(double)x[i]*x[i]; return sqrt(s/(double)(n?n:1)); }
static metrics_t measure(const int16_t *in, const int16_t *clean, const int16_t *out, int n) {
    metrics_t m = {0}; m.rms=rms16(out,n); int clip=0; double peak=0;
    for (int i=0;i<n;i++){ double a=fabs((double)out[i]); if(a>peak)peak=a; if(out[i]==32767||out[i]==-32768)clip++; }
    m.peak=peak; m.clip_pct=100.0*(double)clip/(double)(n?n:1);
    /* SNR descriptivo: out ~= g*clean + residuo (fixtures sintéticas conocidas) */
    double num=0, den=0, cres=0;
    for (int i=0;i<n;i++){ num+=(double)out[i]*(double)clean[i]; den+=(double)clean[i]*(double)clean[i]; }
    double g = den>0 ? num/den : 0; double resid=0;
    for (int i=0;i<n;i++){ double e=(double)out[i]-g*(double)clean[i]; resid+=e*e; cres+=pow(g*(double)clean[i],2); }
    m.snr_db = resid>0 ? 10.0*log10((cres+1e-9)/(resid+1e-9)) : 99.0;
    /* correlación de envolventes por bloques de 10 ms */
    int nb=n/BLOCK; double ea=0,eb=0,eab=0,ea2=0,eb2=0;
    for (int b=0;b<nb;b++){ double a=rms16(in+b*BLOCK,BLOCK), c=rms16(out+b*BLOCK,BLOCK); ea+=a; eb+=c; eab+=a*c; ea2+=a*a; eb2+=c*c; }
    double ma=ea/(nb?nb:1), mb=eb/(nb?nb:1);
    m.env_corr = (ea2-nb*ma*ma>1e-9 && eb2-nb*mb*mb>1e-9) ? (eab-nb*ma*mb)/sqrt((ea2-nb*ma*ma)*(eb2-nb*mb*mb)) : 0.0;
    /* brillo: diferencia |x[i]-x[i-1]| / rms; pumping: std/mean de envolvente de salida */
    double d=0; for (int i=1;i<n;i++) d+=fabs((double)out[i]-(double)out[i-1]);
    m.bright = m.rms>1 ? (d/(double)(n-1))/m.rms : 0; double ev=0, ev2=0, cnt=0;
    for (int b=0;b<nb;b++){ double e=rms16(out+b*BLOCK,BLOCK); ev+=e; ev2+=e*e; cnt++; }
    if(cnt>1){double me=ev/cnt; double var=ev2/cnt-me*me; m.pump=(me>1.0 && var>0)?sqrt(var)/me:0.0;}
    return m;
}
static void run_mode(int mode, const int16_t *mix, const int16_t *clean, int n, int16_t *out, metrics_t *m, double *cpu_ms) {
    audio_enh_deinit(); audio_enh_init();
    switch (mode) {
        case 0: break; /* BASE passthrough */
        case 1: audio_enh_preset_load(AUDIO_ENH_PRESET_OFF); break; /* bypass Linux */
        case 2: audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE); break; /* current */
        case 3: audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
                audio_enh_af_leveler_set_enabled(true); audio_enh_af_leveler_set_target(-20.0f);
                audio_enh_af_leveler_set_mode(AF_LEVELER_MEDIUM); audio_enh_nr_set_level(30); break; /* D1 conservative */
        case 4: audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE); audio_enh_nr_set_level(55); break; /* D2 NR+ */
    }
    memcpy(out, mix, sizeof(int16_t)*(size_t)n);
    struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
    for (int p=0;p<n;p+=BLOCK) { int cnt = (n-p<BLOCK)?(n-p):BLOCK; audio_enh_process(out+p,(size_t)cnt); }
    clock_gettime(CLOCK_MONOTONIC,&t1);
    *cpu_ms = (double)(t1.tv_sec-t0.tv_sec)*1000.0 + (double)(t1.tv_nsec-t0.tv_nsec)/1e6;
    *m = measure(mix, clean, out, n);
}
int main(int argc, char **argv) {
    const char *outdir = argc > 1 ? argv[1] : ".";
    const char *names[] = {"male","female","weak_noise","bandnoise","qrm","impulses","level_steps","silence","max_level"};
    const char *modes[] = {"A_base","B_linux_bypass","C_current_ssb","D1_conservative","D2_nr55"};
    char path[512]; FILE *jp = fopen(strcat(strcpy(path, outdir), "/bench-results.json"), "w");
    if (!jp) return 1;
    fprintf(jp, "{\"note\":\"synthetic fixtures; QEMU ARM estimate, not physical\",\"modes\":[");
    for (int x=0;x<5;x++) fprintf(jp, "%s\"%s\"", x?",":"", modes[x]); fprintf(jp,"],\"rows\":[");
    int first=1;
    for (int id=0; id<9; id++) {
        int16_t *clean=malloc(sizeof(int16_t)*NS), *mix=malloc(sizeof(int16_t)*NS), *out=malloc(sizeof(int16_t)*NS);
        make_fixture(id, clean, mix, NS);
        /* fixture WAV/raw + hash evidence */
        snprintf(path, sizeof(path), "%s/fixture_%s.wav", outdir, names[id]); FILE *w=fopen(path,"wb");
        if (w){ uint32_t datasz=(uint32_t)(NS*2), riff=36+datasz; uint16_t pcm=1, ch=1, bps=16; uint32_t sr=RATE, br=RATE*2; uint16_t ba=2;
            fwrite("RIFF",1,4,w); fwrite(&riff,4,1,w); fwrite("WAVEfmt ",1,8,w); uint32_t sz=16; fwrite(&sz,4,1,w); fwrite(&pcm,2,1,w); fwrite(&ch,2,1,w); fwrite(&sr,4,1,w); fwrite(&br,4,1,w); fwrite(&ba,2,1,w); fwrite(&bps,2,1,w); fwrite("data",1,4,w); fwrite(&datasz,4,1,w); fwrite(mix,2,(size_t)NS,w); fclose(w); }
        for (int mode=0; mode<5; mode++) {
            metrics_t m; double cpu=0; run_mode(mode, mix, clean, NS, out, &m, &cpu);
            if(!first) fprintf(jp,","); first=0;
            fprintf(jp, "{\"fixture\":\"%s\",\"mode\":\"%s\",\"rms\":%.2f,\"peak\":%.0f,\"clip_pct\":%.4f,\"snr_db\":%.2f,\"env_corr\":%.4f,\"brightness\":%.4f,\"pumping\":%.4f,\"cpu_ms_per_1p2s\":%.3f,\"cpu_rtf\":%.4f}",
                    names[id], modes[mode], m.rms, m.peak, m.clip_pct, m.snr_db, m.env_corr, m.bright, m.pump, cpu, cpu/1200.0);
        }
        free(clean); free(mix); free(out);
    }
    fprintf(jp, "]}\n"); fclose(jp);
    /* integridad/partición: SSB voice (nivelador off) debe ser invariante a partición */
    int16_t *in=malloc(sizeof(int16_t)*NS), *o1=malloc(sizeof(int16_t)*NS), *o2=malloc(sizeof(int16_t)*NS);
    make_fixture(0, in, o1, NS); audio_enh_deinit(); audio_enh_init(); audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
    for (int p=0;p<NS;p+=BLOCK) audio_enh_process(o1+p,BLOCK);
    memcpy(o2,in,sizeof(int16_t)*NS); audio_enh_deinit(); audio_enh_init(); audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
    int chunks[] = {441, 2205, 1764, 4410, 882, 4410}; int pos=0, ci=0;
    while(pos<NS){int c=chunks[ci++%6]; if(c>NS-pos)c=NS-pos; audio_enh_process(o2+pos,(size_t)c); pos+=c;}
    int diff=0; for(int i=0;i<NS;i++) if(o1[i]!=o2[i]) diff++;
    FILE *ic=fopen(strcat(strcpy(path,outdir), "/integrity.txt"),"w");
    if(ic){ fprintf(ic,"partition_mismatch_samples=%d\n",diff); fprintf(ic,"length_input=%d length_output=%d\n",NS,NS); fclose(ic);}
    printf("bench done diff=%d\n", diff);
    free(in); free(o1); free(o2);
    return 0;
}
