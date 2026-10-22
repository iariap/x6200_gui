#include "decoder.h"
#include "vis.h"
#include "linesync.h"

#include <sndfile.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR SSTV_SAMPLE_RATE
#define PI 3.14159265358979323846f
#define HILBERT_N 9
#define HILBERT_M (HILBERT_N/2)
#define SMOOTH_N 11
#define MAX_SYNCS 300

typedef struct { int vis; sstv_mode_t mode; const char *name; int w, h; } mode_desc_t;
static const mode_desc_t modes[] = {
    { 2, SSTV_MODE_ROBOT8_BW, "Robot 8 BW", 160, 120 },
    { 8, SSTV_MODE_ROBOT36,   "Robot 36",  320, 240 },
    {12, SSTV_MODE_ROBOT72,   "Robot 72",  320, 240 },
    {40, SSTV_MODE_MARTIN_M2, "Martin M2", 320, 256 },
    {44, SSTV_MODE_MARTIN_M1, "Martin M1", 320, 256 },
    {56, SSTV_MODE_SCOTTIE_S2,"Scottie S2",320, 256 },
    {60, SSTV_MODE_SCOTTIE_S1,"Scottie S1",320, 256 },
    {76, SSTV_MODE_SCOTTIE_DX,"Scottie DX",320, 256 },
};

static uint8_t clamp8(float v)
{
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (uint8_t)(v + 0.5f);
}

static float sample_level(const float *freq, long n, float pos)
{
    if (pos < 0) pos = 0;
    if (pos >= n - 1) pos = (float)(n - 2);
    long i = (long)pos;
    float t = pos - (float)i;
    float f = freq[i] + (freq[i + 1] - freq[i]) * t;
    float v = (f - 1500.0f) * (255.0f / 800.0f);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return v;
}

static void segment(const float *freq, long n, long a, long b, float *dst, int count)
{
    float step = (float)(b - a) / (float)count;
    for (int i = 0; i < count; ++i)
        dst[i] = sample_level(freq, n, (float)a + ((float)i + 0.5f) * step);
}

static int robot36_decode_wav(const char *path, uint8_t *rgb)
{
    SF_INFO info;
    memset(&info, 0, sizeof(info));
    SNDFILE *sf = sf_open(path, SFM_READ, &info);
    if (!sf) {
        fprintf(stderr, "SSTV TEST: cannot open %s: %s\n", path, sf_strerror(NULL));
        return -1;
    }
    if (info.channels != 1 || info.samplerate != SR) {
        fprintf(stderr, "SSTV TEST: WAV must be mono 11025 Hz\n");
        sf_close(sf);
        return -2;
    }

    long n = (long)info.frames;
    float *x = malloc((size_t)n * sizeof(float));
    float *freq = malloc((size_t)n * sizeof(float));
    if (!x || !freq) {
        free(x); free(freq); sf_close(sf);
        return -3;
    }
    sf_count_t got = sf_readf_float(sf, x, info.frames);
    sf_close(sf);
    if (got != info.frames) {
        free(x); free(freq);
        return -4;
    }

    /* 31-tap windowed Hilbert transformer.  Together with the delayed
       real signal this gives an analytic signal for the phase discriminator. */
    float h[HILBERT_N];
    for (int k = 0; k < HILBERT_N; ++k) {
        int m = k - HILBERT_M;
        float ideal = (m != 0 && (m & 1)) ? (2.0f / (PI * (float)m)) : 0.0f;
        float win = 0.54f - 0.46f * cosf(2.0f * PI * (float)k / (float)(HILBERT_N - 1));
        h[k] = ideal * win;
    }

    float prev_phase = 0.0f;
    float ring[SMOOTH_N] = {0};
    float sum = 0.0f;
    int rp = 0;
    for (long i = 0; i < n; ++i) {
        float q = 0.0f;
        if (i >= HILBERT_N - 1) {
            for (int k = 0; k < HILBERT_N; ++k)
                q += h[k] * x[i - k];
        }
        float re = (i >= HILBERT_M) ? x[i - HILBERT_M] : 0.0f;
        float phase = atan2f(q, re);
        float d = phase - prev_phase;
        prev_phase = phase;
        while (d > PI) d -= 2.0f * PI;
        while (d < -PI) d += 2.0f * PI;
        float f = fabsf(d) * ((float)SR / (2.0f * PI));
        if (f > 4000.0f) f = 4000.0f;
        sum -= ring[rp]; ring[rp] = f; sum += f;
        rp = (rp + 1) % SMOOTH_N;
        freq[i] = sum / (float)SMOOTH_N;
    }

    /* Find 1200 Hz horizontal sync pulses (nominally 9 ms). */
    long cand[MAX_SYNCS];
    int nc = 0;
    long i = SR / 2;
    while (i < n && nc < MAX_SYNCS) {
        if (freq[i] > 1080.0f && freq[i] < 1320.0f) {
            long a = i;
            while (i < n && freq[i] > 1080.0f && freq[i] < 1320.0f) ++i;
            float ms = (float)(i - a) * 1000.0f / (float)SR;
            if (ms >= 5.5f && ms <= 14.5f) cand[nc++] = a;
        } else ++i;
    }

    /* Select the longest chain spaced by one Robot36 line (~150 ms). */
    long sync[SSTV_ROBOT36_H];
    int ns = 0, best_start = -1, best_len = 0;
    for (int s = 0; s < nc; ++s) {
        int len = 1; long last = cand[s];
        for (int j = s + 1; j < nc && len < SSTV_ROBOT36_H; ++j) {
            float dt = (float)(cand[j] - last) / (float)SR;
            if (dt > 0.135f && dt < 0.165f) { ++len; last = cand[j]; }
            else if (dt >= 0.165f) break;
        }
        if (len > best_len) { best_len = len; best_start = s; }
    }
    if (best_start < 0 || best_len < 20) {
        fprintf(stderr, "SSTV TEST: Robot36 sync not found (%d candidates)\n", nc);
        free(x); free(freq); return -5;
    }
    long last = cand[best_start]; sync[ns++] = last;
    for (int j = best_start + 1; j < nc && ns < SSTV_ROBOT36_H; ++j) {
        float dt = (float)(cand[j] - last) / (float)SR;
        if (dt > 0.135f && dt < 0.165f) { last = cand[j]; sync[ns++] = last; }
        else if (dt >= 0.165f) break;
    }

    float *Y = calloc((size_t)ns * 320, sizeof(float));
    float *C = calloc((size_t)ns * 320, sizeof(float));
    float c160[160];
    if (!Y || !C) { free(Y); free(C); free(x); free(freq); return -6; }

    for (int r = 0; r < ns; ++r) {
        long s = sync[r];
        segment(freq, n, s + (long)(0.012f*SR), s + (long)(0.100f*SR), &Y[r*320], 320);
        segment(freq, n, s + (long)(0.106f*SR), s + (long)(0.150f*SR), c160, 160);
        for (int p = 0; p < 320; ++p) {
            float u = (float)p * 159.0f / 319.0f;
            int j = (int)u; float t = u - j;
            float v = (j < 159) ? c160[j] + (c160[j+1]-c160[j])*t : c160[159];
            C[r*320+p] = v - 128.0f;
        }
    }

    /* Robot36 alternates B-Y / R-Y lines.  Interpolate the missing component
       vertically; parity here matches the standard Robot36 test WAV. */
    for (int r = 0; r < ns; ++r) {
        for (int p = 0; p < 320; ++p) {
            float cr, cb;
            if (r & 1) {
                cr = C[r*320+p];
                if (r == 0) cb = C[r*320+p];
                else if (r + 1 < ns) cb = 0.5f*(C[(r-1)*320+p] + C[(r+1)*320+p]);
                else cb = C[(r-1)*320+p];
            } else {
                cb = C[r*320+p];
                if (r + 1 < ns && r > 0) cr = 0.5f*(C[(r-1)*320+p] + C[(r+1)*320+p]);
                else if (r + 1 < ns) cr = C[(r+1)*320+p];
                else cr = C[r*320+p];
            }
            float y = Y[r*320+p];
            size_t o = ((size_t)r*320 + p)*3;
            rgb[o+0] = clamp8(y + 1.402f*cr);
            rgb[o+1] = clamp8(y - 0.714136f*cr - 0.344136f*cb);
            rgb[o+2] = clamp8(y + 1.772f*cb);
        }
    }
    if (ns < SSTV_ROBOT36_H)
        memset(rgb + (size_t)ns*320*3, 0, (size_t)(SSTV_ROBOT36_H-ns)*320*3);

    fprintf(stderr, "SSTV TEST: decoded %d Robot36 rows\n", ns);
    free(Y); free(C); free(x); free(freq);
    return ns;
}


/* Robot 72 sends a complete Y, R-Y and B-Y set on every line.
 * Nominal line timing is exactly 300 ms:
 *   sync 9, porch 3, Y 138,
 *   separator 4.5 + porch 1.5, R-Y 69,
 *   separator 4.5 + porch 1.5, B-Y 69. */
static int robot72_decode_wav(const char *path, uint8_t *rgb)
{
    const int height = 240;
    const float line_ms = 300.000f;
    fprintf(stderr, "SSTV: Robot 72 decoder start: line=%.3f ms\n", line_ms);

    SF_INFO info = {0};
    SNDFILE *sf = sf_open(path, SFM_READ, &info);
    if (!sf) { fprintf(stderr, "SSTV: Robot72: cannot open %s: %s\n", path, sf_strerror(NULL)); return -1; }
    if (info.channels != 1 || info.samplerate != SR) {
        fprintf(stderr, "SSTV: Robot72: WAV must be mono 11025 Hz\n"); sf_close(sf); return -2;
    }
    long n = (long)info.frames;
    float *x = malloc((size_t)n*sizeof(float));
    float *freq = malloc((size_t)n*sizeof(float));
    if (!x || !freq) { free(x); free(freq); sf_close(sf); return -3; }
    if (sf_readf_float(sf, x, info.frames) != info.frames) { sf_close(sf); free(x); free(freq); return -4; }
    sf_close(sf);

    float h[HILBERT_N];
    for (int k=0;k<HILBERT_N;++k) {
        int m=k-HILBERT_M;
        float ideal=(m!=0 && (m&1)) ? (2.0f/(PI*(float)m)) : 0.0f;
        float win=0.54f-0.46f*cosf(2.0f*PI*(float)k/(float)(HILBERT_N-1));
        h[k]=ideal*win;
    }
    float prev_phase=0.0f, ring[SMOOTH_N]={0}, sum=0.0f; int rp=0;
    for (long i=0;i<n;++i) {
        float q=0.0f;
        if (i>=HILBERT_N-1) for (int k=0;k<HILBERT_N;++k) q+=h[k]*x[i-k];
        float re=(i>=HILBERT_M)?x[i-HILBERT_M]:0.0f;
        float phase=atan2f(q,re), d=phase-prev_phase; prev_phase=phase;
        while(d>PI)d-=2.0f*PI; while(d<-PI)d+=2.0f*PI;
        float f=fabsf(d)*((float)SR/(2.0f*PI)); if(f>4000.0f)f=4000.0f;
        sum-=ring[rp]; ring[rp]=f; sum+=f; rp=(rp+1)%SMOOTH_N; freq[i]=sum/(float)SMOOTH_N;
    }

    long cand[MAX_SYNCS]; int nc=0; long i=SR/2;
    while(i<n && nc<MAX_SYNCS) {
        if(freq[i]>1080.0f && freq[i]<1320.0f) {
            long a=i; while(i<n && freq[i]>1080.0f && freq[i]<1320.0f) ++i;
            float ms=(float)(i-a)*1000.0f/(float)SR;
            if(ms>=6.0f && ms<=13.0f) cand[nc++]=a;
        } else ++i;
    }
    fprintf(stderr,"SSTV: Robot72: %d sync candidates\n",nc);

    long sync[240]; int ns=0,best_start=-1,best_len=0;
    const float nominal=0.300f, lo=nominal*0.94f, hi=nominal*1.06f;
    for(int st=0;st<nc;++st) {
        int len=1; long last=cand[st];
        for(int j=st+1;j<nc && len<height;++j) {
            float dt=(float)(cand[j]-last)/(float)SR;
            if(dt>lo && dt<hi){++len;last=cand[j];} else if(dt>=hi)break;
        }
        if(len>best_len){best_len=len;best_start=st;}
    }
    if(best_start<0 || best_len<20){fprintf(stderr,"SSTV: Robot72: sync chain NOT found (best=%d)\n",best_len);free(x);free(freq);return -5;}
    long last=cand[best_start]; sync[ns++]=last;
    for(int j=best_start+1;j<nc && ns<height;++j){
        float dt=(float)(cand[j]-last)/(float)SR;
        if(dt>lo && dt<hi){last=cand[j];sync[ns++]=last;} else if(dt>=hi)break;
    }
    fprintf(stderr,"SSTV: Robot72: selected sync chain rows=%d, first=%.3f s\n",ns,(float)sync[0]/SR);

    float Y[320], Cr[320], Cb[320];
    for(int row=0;row<ns;++row){
        long base=sync[row];
        long ya=base+(long)(12.0f*SR/1000.0f), yb=base+(long)(150.0f*SR/1000.0f);
        long cra=base+(long)(156.0f*SR/1000.0f), crb=base+(long)(225.0f*SR/1000.0f);
        long cba=base+(long)(231.0f*SR/1000.0f), cbb=base+(long)(300.0f*SR/1000.0f);
        if(cbb>=n){fprintf(stderr,"SSTV: Robot72: EOF at row %d\n",row);ns=row;break;}
        segment(freq,n,ya,yb,Y,320); segment(freq,n,cra,crb,Cr,320); segment(freq,n,cba,cbb,Cb,320);
        for(int px=0;px<320;++px){
            float cr=Cr[px]-128.0f, cb=Cb[px]-128.0f, y=Y[px]; size_t o=((size_t)row*320+px)*3;
            rgb[o]=clamp8(y+1.402f*cr); rgb[o+1]=clamp8(y-0.714136f*cr-0.344136f*cb); rgb[o+2]=clamp8(y+1.772f*cb);
        }
        if((row%32)==31 || row==ns-1) fprintf(stderr,"SSTV: Robot72: decoded row %d/%d\n",row+1,ns);
    }
    if(ns<height) memset(rgb+(size_t)ns*320*3,0,(size_t)(height-ns)*320*3);
    free(x); free(freq);
    fprintf(stderr,"SSTV: Robot 72 image decode finished, rows=%d\n",ns);
    return ns;
}


/* Martin family decoder. Martin M1/M2 use the same scan order:
 * 1200 Hz sync, short 1500 Hz porch, then G, B, R with 1500 Hz
 * separators. Only the channel time differs between M1 and M2. */


/* Robust horizontal-sync detector shared by all RAM decoders.
 *
 * The old detector required the phase discriminator to remain continuously
 * inside 1080..1320 Hz for several milliseconds.  Real RF can punch tiny
 * holes into an otherwise perfectly usable 1200-Hz sync pulse; one such
 * recording reduced every Martin sync to ~1-ms fragments and yielded zero
 * candidates.
 *
 * Detect the sync directly in the audio instead.  For short overlapping
 * windows, measure normalized coherent energy at 1200 Hz.  This tolerates
 * brief frequency-estimator glitches, fading and impulse noise without
 * changing the protocol timing rules.  Adjacent positive windows are merged
 * and only the strongest position is emitted.  The existing robust timing
 * grid then decides whether those candidates form a physically credible
 * line-period chain, so isolated 1200-Hz picture content cannot establish
 * timing by itself.
 *
 * pulse_ms is mode-specific (Martin 4.862 ms, Robot/Scottie about 9-10 ms);
 * the detection mechanism and threshold are intentionally common. */
static int detect_sync_1200_robust(const float *samples, long n,
                                   float pulse_ms, long *cand, int max_cand)
{
    if (!samples || n <= 0 || !cand || max_cand <= 0 || pulse_ms <= 0.0f)
        return 0;

    int win = (int)lroundf(pulse_ms * (float)SR / 1000.0f);
    if (win < 24) win = 24;
    if (win > 160) win = 160;

    /* ~1 ms hop.  Small enough to locate a 4.862-ms Martin sync accurately,
     * while keeping the scan inexpensive on the X6100. */
    const int hop = (SR + 500) / 1000;
    const float threshold = 0.45f;

    float cs[160], sn[160];
    for (int k=0; k<win; ++k) {
        float ph = 2.0f * PI * 1200.0f * (float)k / (float)SR;
        cs[k] = cosf(ph);
        sn[k] = sinf(ph);
    }

    int nc=0;
    int in_group=0;
    float best_score=0.0f;
    long best_pos=0;

    for (long a=SR/2; a+win<n; a+=hop) {
        double re=0.0, im=0.0, energy=0.0;
        for (int k=0; k<win; ++k) {
            float v=samples[a+k];
            re += (double)v * (double)cs[k];
            im -= (double)v * (double)sn[k];
            energy += (double)v * (double)v;
        }

        float score=0.0f;
        if (energy > 1e-12) {
            double mag=sqrt(re*re+im*im);
            score=(float)(mag/sqrt(energy*(double)win));
        }

        if (score >= threshold) {
            if (!in_group) {
                in_group=1;
                best_score=score;
                best_pos=a;
            } else if (score > best_score) {
                best_score=score;
                best_pos=a;
            }
        } else if (in_group) {
            if (nc < max_cand) cand[nc++]=best_pos;
            in_group=0;
            best_score=0.0f;
            if (nc >= max_cand) break;
        }
    }

    if (in_group && nc < max_cand)
        cand[nc++]=best_pos;

    return nc;
}

/* Second-pass global slant refinement.
 * The first robust chain gives us a safe clock/phase estimate.  Once that is
 * known, look at every detected horizontal-sync candidate again and assign it
 * to the nearest predicted image row.  This can extend a short but good chain
 * over a much longer baseline, which makes the samples/line estimate markedly
 * more accurate.  One candidate per row is retained (the closest one).
 *
 * This is deliberately conservative: late chains are left to the existing
 * late-chain anchor recovery, and a new fit is accepted only when it extends
 * the timing baseline by at least 12 rows without producing a large clock
 * jump. */
static int global_sync_refine(const char *tag, const long *cand, int nc, int height,
                              double nominal_spl, int old_points, int old_span,
                              double *intercept_io, double *spl_io)
{
    if (!cand || nc <= 0 || !intercept_io || !spl_io || *spl_io <= 0.0) return 0;

    double intercept=*intercept_io, spl=*spl_io;
    if (intercept > 5.0*(double)SR) return 0; /* late sparse chain: anchor first */

    int have[300]={0};
    long pos[300]={0};
    double err[300];
    for(int r=0;r<300;++r) err[r]=1e30;

    /* Tight gate: wide enough for the residual drift we want to correct, but
     * much narrower than a line so random 1200-Hz bursts are unlikely to win. */
    const double gate=nominal_spl*0.020;
    for(int k=0;k<nc;++k){
        double rf=((double)cand[k]-intercept)/spl;
        long row=lround(rf);
        if(row<0 || row>=height) continue;
        double predicted=intercept+spl*(double)row;
        double e=fabs((double)cand[k]-predicted);
        if(e<=gate && e<err[row]){have[row]=1;pos[row]=cand[k];err[row]=e;}
    }

    int count=0,first=-1,last=-1;
    double sx=0,sy=0,sxx=0,sxy=0;
    for(int r=0;r<height;++r) if(have[r]){
        if(first<0) first=r; last=r; ++count;
        double x=(double)r,y=(double)pos[r];
        sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;
    }
    if(count<12 || first<0 || last-first < old_span+12) return 0;

    double den=(double)count*sxx-sx*sx;
    if(fabs(den)<1e-9) return 0;
    double nspl=((double)count*sxy-sx*sy)/den;
    double nintercept=(sy-nspl*sx)/(double)count;

    /* Never let this optional refinement make a dramatic change.  Its job is
     * sub-line slant correction, not mode/timing recovery. */
    double jump_ppm=fabs(nspl/spl-1.0)*1000000.0;
    if(jump_ppm>400.0) {
        fprintf(stderr,"SSTV: %s: GLOBAL sync refinement rejected: clock jump %.1f ppm (points=%d span=%d)\\n",
                tag,jump_ppm,count,last-first+1);
        return 0;
    }

    fprintf(stderr,"SSTV: %s: GLOBAL sync refinement accepted: points %d -> %d, span %d -> %d rows\\n",
            tag,old_points,count,old_span+1,last-first+1);
    fprintf(stderr,"SSTV: %s: GLOBAL refined line %.6f -> %.6f ms, clock %+.1f -> %+.1f ppm\\n",
            tag,spl*1000.0/(double)SR,nspl*1000.0/(double)SR,
            (spl/nominal_spl-1.0)*1000000.0,(nspl/nominal_spl-1.0)*1000000.0);
    *intercept_io=nintercept;
    *spl_io=nspl;
    return count;
}

static int martin_decode_samples(const float *x, long n, uint8_t *rgb, sstv_mode_t mode)
{
    const float channel_ms = (mode == SSTV_MODE_MARTIN_M1) ? 146.432f : 73.216f;
    const float line_ms    = (mode == SSTV_MODE_MARTIN_M1) ? 446.446f : 226.798f;
    const int height = 256;

    fprintf(stderr, "SSTV: Martin decoder start from RAM: %s, samples=%ld, channel=%.3f ms line=%.3f ms\n",
            mode == SSTV_MODE_MARTIN_M1 ? "M1" : "M2", n, channel_ms, line_ms);
    if (!x || n <= 0 || !rgb) return -1;
    float *freq = malloc((size_t)n * sizeof(float));
    if (!freq) return -3;

    /* Same phase-discriminator front end already proven by Robot 36. */
    float h[HILBERT_N];
    for (int k = 0; k < HILBERT_N; ++k) {
        int m = k - HILBERT_M;
        float ideal = (m != 0 && (m & 1)) ? (2.0f / (PI * (float)m)) : 0.0f;
        float win = 0.54f - 0.46f * cosf(2.0f * PI * (float)k / (float)(HILBERT_N - 1));
        h[k] = ideal * win;
    }

    float prev_phase = 0.0f;
    float ring[SMOOTH_N] = {0};
    float sum = 0.0f;
    int rp = 0;
    for (long i = 0; i < n; ++i) {
        float q = 0.0f;
        if (i >= HILBERT_N - 1)
            for (int k = 0; k < HILBERT_N; ++k) q += h[k] * x[i-k];
        float re = (i >= HILBERT_M) ? x[i-HILBERT_M] : 0.0f;
        float phase = atan2f(q, re);
        float d = phase - prev_phase;
        prev_phase = phase;
        while (d > PI) d -= 2.0f * PI;
        while (d < -PI) d += 2.0f * PI;
        float f = fabsf(d) * ((float)SR / (2.0f * PI));
        if (f > 4000.0f) f = 4000.0f;
        sum -= ring[rp]; ring[rp] = f; sum += f;
        rp = (rp + 1) % SMOOTH_N;
        freq[i] = sum / (float)SMOOTH_N;
    }

    /* Martin horizontal sync is nominally 4.862 ms at 1200 Hz. */
    long cand[MAX_SYNCS];
    int nc = detect_sync_1200_robust(x, n, 4.862f, cand, MAX_SYNCS);
    fprintf(stderr, "SSTV: Martin: %d sync candidates\n", nc);

    /* Robust Martin timing recovery, adapted from QSSTV's syncProcessor
     * strategy: consecutive detected syncs may be separated by more than one
     * image line.  Assign an integer line spacing to each accepted sync, then
     * fit sample_position = intercept + samples_per_line * line_number.
     * Missing syncs are reconstructed from that fitted timing grid. */
    const double nominal_spl = (double)line_ms * (double)SR / 1000.0;
    const double link_tol = 0.035;       /* fractional line error for initial chain */
    const int max_gap_lines = 8;         /* tolerate several lost horizontal syncs */
    int chain_idx[256];
    int chain_line[256];
    int best_idx[256];
    int best_line[256];
    int best_count = 0, best_span = 0;

    for (int st = 0; st < nc; ++st) {
        int cc = 1;
        chain_idx[0] = st;
        chain_line[0] = 0;
        long last_pos = cand[st];
        int last_line = 0;

        for (int j = st + 1; j < nc && cc < height; ++j) {
            double delta = (double)(cand[j] - last_pos);
            int gap = (int)floor(delta / nominal_spl + 0.5);
            if (gap < 1) continue;       /* duplicate/noise before next expected line */
            if (gap > max_gap_lines) break;
            double frac = fabs((double)gap - delta / nominal_spl);
            if (frac < link_tol) {
                last_line += gap;
                if (last_line >= height) break;
                chain_idx[cc] = j;
                chain_line[cc] = last_line;
                ++cc;
                last_pos = cand[j];
            }
        }

        int span = chain_line[cc - 1];
        /* Prefer a chain covering most of the image; candidate count breaks ties. */
        if (span > best_span || (span == best_span && cc > best_count)) {
            best_span = span;
            best_count = cc;
            memcpy(best_idx, chain_idx, (size_t)cc * sizeof(best_idx[0]));
            memcpy(best_line, chain_line, (size_t)cc * sizeof(best_line[0]));
        }
    }

    if (best_count < 4 || best_span < 4) {
        /* VIS already told us the mode.  With badly damaged syncs, do not
         * discard the picture: synthesize two timing anchors at the nominal
         * period.  This deliberately sacrifices slant correction but still
         * lets the demodulator render a degraded 256-line image. */
        double first = 1.93 * (double)SR;
        if (nc > 0) {
            long best=cand[0]; double bd=fabs((double)best-first);
            for (int k=1;k<nc;++k) { double d=fabs((double)cand[k]-first); if(d<bd){bd=d;best=cand[k];} }
            if (bd <= nominal_spl*0.75) first=(double)best;
        }
        cand[0]=(long)llround(first);
        cand[1]=(long)llround(first+nominal_spl);
        best_idx[0]=0; best_line[0]=0; best_idx[1]=1; best_line[1]=1;
        best_count=2; best_span=1;
        fprintf(stderr, "SSTV: Martin: FALLBACK nominal timing; rendering degraded image (sync points=%d)\n", nc);
    } else if (best_count < 20 || best_span < 20) {
        fprintf(stderr, "SSTV: Martin: weak sync chain accepted (points=%d span=%d); rendering anyway\n", best_count,best_span);
    }

    fprintf(stderr, "SSTV: Martin: robust chain points=%d, line span=%d, first=%.3f s\n",
            best_count, best_span + 1, (float)cand[best_idx[0]]/(float)SR);

    /* Linear regression exactly in the spirit of QSSTV slantAdjust(): use
     * detected sync position as Y and assigned image line as X. */
    double sx=0.0, sy=0.0, sxx=0.0, sxy=0.0;
    for (int k=0; k<best_count; ++k) {
        double lx=(double)best_line[k];
        double py=(double)cand[best_idx[k]];
        sx += lx; sy += py; sxx += lx*lx; sxy += lx*py;
    }
    double den=(double)best_count*sxx-sx*sx;
    if (fabs(den) < 1e-9) { free(freq); return -6; }
    double fitted_spl=((double)best_count*sxy-sx*sy)/den;
    double intercept=(sy-fitted_spl*sx)/(double)best_count;

    /* One robust refinement pass: reject syncs whose residual is too large,
     * then refit. This prevents an occasional 1200-Hz noise burst from
     * influencing the slant estimate. */
    const double residual_limit = nominal_spl * 0.08;
    sx=sy=sxx=sxy=0.0;
    int used=0;
    double max_resid=0.0;
    for (int k=0; k<best_count; ++k) {
        double lx=(double)best_line[k];
        double py=(double)cand[best_idx[k]];
        double resid=fabs(py-(intercept+fitted_spl*lx));
        if (resid > max_resid) max_resid=resid;
        if (resid > residual_limit) continue;
        sx += lx; sy += py; sxx += lx*lx; sxy += lx*py; ++used;
    }
    if (used >= 12) {
        den=(double)used*sxx-sx*sx;
        if (fabs(den) > 1e-9) {
            fitted_spl=((double)used*sxy-sx*sy)/den;
            intercept=(sy-fitted_spl*sx)/(double)used;
        }
    }

    /* Optional second pass over all sync candidates.  This does not replace
     * the robust chain: it only lengthens its baseline when the initial lock
     * is early and trustworthy. */
    global_sync_refine("Martin",cand,nc,height,nominal_spl,best_count,best_span,
                       &intercept,&fitted_spl);

    double fitted_ms=fitted_spl*1000.0/(double)SR;
    double clock_err_ppm=(fitted_spl/nominal_spl-1.0)*1000000.0;

    /* A very short chain can fit beautifully while still describing noise.
     * In that case do not let it invent a large clock correction.  Keep its
     * phase/anchor information, but use the VIS mode's nominal line period. */
    if ((best_count < 10 || best_span < 10) && fabs(clock_err_ppm) > 500.0) {
        double mean_intercept=0.0;
        fprintf(stderr, "SSTV: Martin: weak-chain clock %+.1f ppm is unreliable; using nominal line period\n",
                clock_err_ppm);
        fitted_spl=nominal_spl;
        for (int k=0;k<best_count;++k)
            mean_intercept += (double)cand[best_idx[k]] - fitted_spl*(double)best_line[k];
        intercept=mean_intercept/(double)best_count;
        fitted_ms=fitted_spl*1000.0/(double)SR;
        clock_err_ppm=0.0;
    }

    fprintf(stderr, "SSTV: Martin: nominal line=%.3f ms, fitted line=%.6f ms\n",
            line_ms, fitted_ms);
    fprintf(stderr, "SSTV: Martin: clock error=%+.1f ppm, regression syncs=%d/%d, max initial residual=%.2f samples\n",
            clock_err_ppm, used, best_count, max_resid);

    /* Reconstruct the complete 256-line timing grid. The chain's first sync
     * is image line zero; lines whose sync pulse was lost are still decoded
     * from the regression prediction. */
    long sync[256];
    int ns=height;
    for (int row=0; row<height; ++row)
        sync[row]=(long)llround(intercept+fitted_spl*(double)row);

    fprintf(stderr, "SSTV: Martin: reconstructed timing grid rows=%d, first=%.3f s, last=%.3f s\n",
            ns, (float)sync[0]/(float)SR, (float)sync[height-1]/(float)SR);

    /* Same late-sparse-chain recovery proven by Scottie: a chain found in
     * the second half of the picture still gives excellent clock/slant, but
     * its first member is not image row zero.  Preserve the fitted period and
     * shift the complete grid backwards by an integer number of lines. */
    {
        const double expected_first_sec = (mode == SSTV_MODE_MARTIN_M1) ? 2.35 : 2.20;
        const double expected_first = expected_first_sec*(double)SR;
        const double late_limit = 5.0*(double)SR;
        if ((double)sync[0] > late_limit && fitted_spl > 0.0) {
            long shift_lines=(long)llround(((double)sync[0]-expected_first)/fitted_spl);
            if (shift_lines>0 && shift_lines<height) {
                long shift_samples=(long)llround((double)shift_lines*fitted_spl);
                double old_first=(double)sync[0]/(double)SR;
                for(int row=0;row<height;++row) sync[row]-=shift_samples;
                fprintf(stderr,"SSTV: Martin: late sparse sync chain: first grid point %.3f s corresponds to about image row %ld\n",
                        old_first,shift_lines);
                fprintf(stderr,"SSTV: Martin: extrapolated timing grid backward %ld lines -> first=%.3f s last=%.3f s\n",
                        shift_lines,(double)sync[0]/(double)SR,(double)sync[height-1]/(double)SR);
            }
        }
    }

    const float sync_ms = 4.862f;
    const float porch_ms = 0.572f;
    const float sep_ms = 0.572f;
    const float g0_ms = sync_ms + porch_ms;
    const float b0_ms = g0_ms + channel_ms + sep_ms;
    const float r0_ms = b0_ms + channel_ms + sep_ms;

    float G[320], B[320], R[320];
    for (int row=0; row<ns; ++row) {
        long base=sync[row];
        double timing_scale=fitted_spl/nominal_spl;
        long ga=base+(long)llround(g0_ms*SR/1000.0*timing_scale);
        long gb=base+(long)llround((g0_ms+channel_ms)*SR/1000.0*timing_scale);
        long ba=base+(long)llround(b0_ms*SR/1000.0*timing_scale);
        long bb=base+(long)llround((b0_ms+channel_ms)*SR/1000.0*timing_scale);
        long ra=base+(long)llround(r0_ms*SR/1000.0*timing_scale);
        long rb=base+(long)llround((r0_ms+channel_ms)*SR/1000.0*timing_scale);
        if (rb >= n) { fprintf(stderr, "SSTV: Martin: EOF at row %d\n", row); ns=row; break; }
        segment(freq,n,ga,gb,G,320);
        segment(freq,n,ba,bb,B,320);
        segment(freq,n,ra,rb,R,320);
        for (int px=0; px<320; ++px) {
            size_t o=((size_t)row*320+px)*3;
            rgb[o+0]=clamp8(R[px]);
            rgb[o+1]=clamp8(G[px]);
            rgb[o+2]=clamp8(B[px]);
        }
        if ((row % 32) == 31 || row == ns-1)
            fprintf(stderr, "SSTV: Martin: decoded row %d/%d\n", row+1, ns);
    }
    if (ns < height)
        memset(rgb+(size_t)ns*320*3,0,(size_t)(height-ns)*320*3);

    free(freq);
    fprintf(stderr, "SSTV: Martin image decode finished, rows=%d\n", ns);
    return ns;
}

static int martin_decode_wav(const char *path, uint8_t *rgb, sstv_mode_t mode)
{
    SF_INFO info = {0};
    SNDFILE *sf = sf_open(path, SFM_READ, &info);
    if (!sf) { fprintf(stderr, "SSTV: Martin: cannot open %s: %s\n", path, sf_strerror(NULL)); return -1; }
    if (info.channels != 1 || info.samplerate != SR) {
        fprintf(stderr, "SSTV: Martin: WAV must be mono 11025 Hz\n"); sf_close(sf); return -2;
    }
    long n=(long)info.frames;
    float *x=malloc((size_t)n*sizeof(float));
    if (!x) { sf_close(sf); return -3; }
    if (sf_readf_float(sf,x,info.frames)!=info.frames) { sf_close(sf); free(x); return -4; }
    sf_close(sf);
    int rows=martin_decode_samples(x,n,rgb,mode);
    free(x);
    return rows;
}



/* Recover a complete line timing grid from imperfect horizontal-sync candidates.
 * This follows the useful part of QSSTV syncProcessor/slantAdjust: allow integer
 * multiples of the nominal line spacing, assign line numbers, then regress real
 * sync position against line number. Missing syncs are reconstructed by the fit. */
static int robust_sync_grid(const char *tag, const long *cand, int nc, int height,
                            double nominal_spl, int max_gap_lines,
                            long *grid, double *fitted_spl_out)
{
    int chain_idx[300], chain_line[300], best_idx[300], best_line[300];
    int best_count=0, best_span=0;
    const double link_tol=0.040;
    if (height > 300) return -1;

    for (int st=0; st<nc; ++st) {
        int cc=1, last_line=0;
        long last_pos=cand[st];
        chain_idx[0]=st; chain_line[0]=0;
        for (int j=st+1; j<nc && cc<height; ++j) {
            double delta=(double)(cand[j]-last_pos);
            int gap=(int)floor(delta/nominal_spl+0.5);
            if (gap<1) continue;
            if (gap>max_gap_lines) break;
            double frac=fabs((double)gap-delta/nominal_spl);
            if (frac<link_tol) {
                last_line += gap;
                if (last_line>=height) break;
                chain_idx[cc]=j; chain_line[cc]=last_line; ++cc;
                last_pos=cand[j];
            }
        }
        int span=chain_line[cc-1];
        if (span>best_span || (span==best_span && cc>best_count)) {
            best_span=span; best_count=cc;
            memcpy(best_idx,chain_idx,(size_t)cc*sizeof(int));
            memcpy(best_line,chain_line,(size_t)cc*sizeof(int));
        }
    }
    /* Do not throw an SSTV picture away merely because RF damaged most of the
     * horizontal syncs.  A sparse but coherent chain is still useful: it gives
     * us phase and (usually) enough information to estimate slant, while the
     * known VIS mode supplies the nominal line period for all missing rows. */
    if (best_count<4 || best_span<4) {
        fprintf(stderr,"SSTV: %s: robust sync chain unavailable (points=%d span=%d)\n",tag,best_count,best_span);
        return -2;
    }
    if (best_count<10 || best_span<10)
        fprintf(stderr,"SSTV: %s: weak sync chain accepted (points=%d span=%d); image will be reconstructed anyway\n",
                tag,best_count,best_span);

    double sx=0,sy=0,sxx=0,sxy=0;
    for(int k=0;k<best_count;++k){double x=best_line[k],y=cand[best_idx[k]];sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;}
    double den=(double)best_count*sxx-sx*sx;
    if(fabs(den)<1e-9) return -3;
    double spl=((double)best_count*sxy-sx*sy)/den;
    double intercept=(sy-spl*sx)/(double)best_count;

    /* A tiny number of noisy syncs must not be allowed to invent an absurd
     * clock.  If the fitted period is implausible, keep the phase information
     * from the chain but fall back to the VIS mode's nominal line period. */
    if (fabs(spl/nominal_spl - 1.0) > 0.015) {
        double mean_intercept=0.0;
        fprintf(stderr,"SSTV: %s: sparse fit line period implausible (%.6f ms); using nominal timing\n",
                tag,spl*1000.0/(double)SR);
        spl=nominal_spl;
        for(int k=0;k<best_count;++k)
            mean_intercept += (double)cand[best_idx[k]] - spl*(double)best_line[k];
        intercept=mean_intercept/(double)best_count;
    }

    const double residual_limit=nominal_spl*0.08;
    sx=sy=sxx=sxy=0; int used=0; double max_resid=0;
    for(int k=0;k<best_count;++k){
        double x=best_line[k],y=cand[best_idx[k]];
        double r=fabs(y-(intercept+spl*x)); if(r>max_resid)max_resid=r;
        if(r>residual_limit)continue; sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;++used;
    }
    if(used>=8){den=(double)used*sxx-sx*sx;if(fabs(den)>1e-9){spl=((double)used*sxy-sx*sy)/den;intercept=(sy-spl*sx)/(double)used;}}

    /* Four or five noisy pulses can form a mathematically neat chain but give
     * a useless slant estimate (we observed this on degraded RF Scottie S2).
     * For a weak chain only, reject clock corrections above 500 ppm while
     * retaining the chain's phase. Strong chains are never changed here. */
    if(best_count<10 && fabs(spl/nominal_spl-1.0)*1000000.0 > 500.0){
        double mean_intercept=0.0;
        fprintf(stderr,"SSTV: %s: weak-chain clock %+.1f ppm is unreliable; using nominal line period\n",
                tag,(spl/nominal_spl-1.0)*1000000.0);
        spl=nominal_spl;
        for(int k=0;k<best_count;++k) mean_intercept+=(double)cand[best_idx[k]]-spl*(double)best_line[k];
        intercept=mean_intercept/(double)best_count;
    }

    /* Same conservative global refinement for Robot/Scottie users of this
     * common timing engine.  Strong full-span chains normally remain exactly
     * as they are because there is no meaningful span to extend. */
    global_sync_refine(tag,cand,nc,height,nominal_spl,best_count,best_span,
                       &intercept,&spl);

    for(int row=0;row<height;++row) grid[row]=(long)llround(intercept+spl*(double)row);
    fprintf(stderr,"SSTV: %s: robust chain points=%d span=%d, regression syncs=%d/%d\n",tag,best_count,best_span+1,used,best_count);
    fprintf(stderr,"SSTV: %s: nominal line=%.6f ms, fitted line=%.6f ms, clock error=%+.1f ppm\n",
            tag,nominal_spl*1000.0/(double)SR,spl*1000.0/(double)SR,(spl/nominal_spl-1.0)*1000000.0);
    fprintf(stderr,"SSTV: %s: reconstructed timing grid rows=%d, first=%.3f s, last=%.3f s\n",
            tag,height,(double)grid[0]/SR,(double)grid[height-1]/SR);
    if(fitted_spl_out)*fitted_spl_out=spl;
    return height;
}

/* Robot 72 RAM decoder.  Uses the same timing philosophy as Scottie/Martin:
 * robust regression when sync is usable, late-chain back extrapolation when
 * the useful syncs occur deep in the picture, and nominal timing as the final
 * fallback after a valid VIS. */
static int robot72_decode_samples(const float *x,long n,uint8_t *rgb)
{
    const int width=320,height=240;
    const float line_ms=300.000f;
    if(!x||n<=0||!rgb)return -1;
    fprintf(stderr,"SSTV: Robot 72 decoder start from RAM: samples=%ld, 320x240, line=%.3f ms\n",n,line_ms);

    float *freq=malloc((size_t)n*sizeof(float)); if(!freq)return -3;
    float h[HILBERT_N];
    for(int k=0;k<HILBERT_N;++k){int m=k-HILBERT_M;float ideal=(m!=0&&(m&1))?2.0f/(PI*(float)m):0.0f;float win=.54f-.46f*cosf(2.0f*PI*(float)k/(float)(HILBERT_N-1));h[k]=ideal*win;}
    float prev=0,ring[SMOOTH_N]={0},sum=0;int rp=0;
    for(long i=0;i<n;++i){float q=0;if(i>=HILBERT_N-1)for(int k=0;k<HILBERT_N;++k)q+=h[k]*x[i-k];float re=i>=HILBERT_M?x[i-HILBERT_M]:0;float ph=atan2f(q,re),d=ph-prev;prev=ph;while(d>PI)d-=2*PI;while(d<-PI)d+=2*PI;float f=fabsf(d)*SR/(2*PI);if(f>4000)f=4000;sum-=ring[rp];ring[rp]=f;sum+=f;rp=(rp+1)%SMOOTH_N;freq[i]=sum/SMOOTH_N;}

    long cand[MAX_SYNCS];
    int nc=detect_sync_1200_robust(x,n,9.0f,cand,MAX_SYNCS);
    fprintf(stderr,"SSTV: Robot 72: %d sync candidates\n",nc);

    long sync[240];double fitted_spl=0.0,nominal_spl=(double)line_ms*(double)SR/1000.0;
    int grid_rc=robust_sync_grid("Robot 72",cand,nc,height,nominal_spl,8,sync,&fitted_spl);
    if(grid_rc<0){
        double first=2.0*(double)SR; fitted_spl=nominal_spl;
        if(nc>0){long best=cand[0];double bd=fabs((double)best-first);for(int k=1;k<nc;++k){double d=fabs((double)cand[k]-first);if(d<bd){bd=d;best=cand[k];}}if(bd<=nominal_spl*0.75)first=(double)best;}
        for(int row=0;row<height;++row)sync[row]=(long)llround(first+nominal_spl*(double)row);
        fprintf(stderr,"SSTV: Robot 72: FALLBACK nominal timing grid rows=%d, first=%.3f s; rendering degraded image\n",height,(double)sync[0]/SR);
    } else if ((double)sync[0] > 5.0*(double)SR && fitted_spl>0.0) {
        const double expected_first=2.0*(double)SR;
        long shift_lines=(long)llround(((double)sync[0]-expected_first)/fitted_spl);
        if(shift_lines>0&&shift_lines<height){long ds=(long)llround((double)shift_lines*fitted_spl);double old=(double)sync[0]/SR;for(int r=0;r<height;++r)sync[r]-=ds;fprintf(stderr,"SSTV: Robot 72: late sparse sync chain: first %.3f s corresponds to about row %ld\n",old,shift_lines);fprintf(stderr,"SSTV: Robot 72: extrapolated backward %ld lines -> first=%.3f s last=%.3f s\n",shift_lines,(double)sync[0]/SR,(double)sync[height-1]/SR);}
    }

    double scale=fitted_spl/nominal_spl;
    float Y[320],Cr[320],Cb[320];int ns=height;
    for(int row=0;row<height;++row){
        long base=sync[row];
        long ya=base+(long)llround(.012*SR*scale), yb=base+(long)llround(.150*SR*scale);
        long cra=base+(long)llround(.156*SR*scale), crb=base+(long)llround(.225*SR*scale);
        long cba=base+(long)llround(.231*SR*scale), cbb=base+(long)llround(.300*SR*scale);
        if(base<0||cbb>=n){ns=row;fprintf(stderr,"SSTV: Robot 72: bounds/EOF at row %d\n",row);break;}
        segment(freq,n,ya,yb,Y,width);segment(freq,n,cra,crb,Cr,width);segment(freq,n,cba,cbb,Cb,width);
        for(int px=0;px<width;++px){float cr=Cr[px]-128.0f,cb=Cb[px]-128.0f,y=Y[px];size_t o=((size_t)row*width+px)*3;rgb[o]=clamp8(y+1.402f*cr);rgb[o+1]=clamp8(y-.714136f*cr-.344136f*cb);rgb[o+2]=clamp8(y+1.772f*cb);}
        if((row%32)==31||row==height-1)fprintf(stderr,"SSTV: Robot 72: decoded row %d/%d\n",row+1,height);
    }
    if(ns<height)memset(rgb+(size_t)ns*width*3,0,(size_t)(height-ns)*width*3);
    free(freq);fprintf(stderr,"SSTV: Robot 72 image decode finished, rows=%d\n",ns);return ns;
}

/* Robot B/W 8: 160x120 monochrome, VIS 2, 900 lines/minute.
 * One line is nominally 66.667 ms: about 10 ms 1200 Hz sync followed by
 * 56 ms luminance (1500 Hz black .. 2300 Hz white).  Decode each row from
 * its own detected sync so small transmitter clock errors do not accumulate. */
static int robot8bw_decode_samples(const float *x, long n, uint8_t *rgb)
{
    const int width = 160, height = 120;
    const float line_ms = 1000.0f / 15.0f; /* 900 lpm */
    const float sync_ms = 10.0f;
    const float video_ms = 56.0f;
    if (!x || n <= 0 || !rgb) return -1;
    fprintf(stderr, "SSTV: Robot 8 BW decoder start from RAM: samples=%ld, 160x120, line=%.3f ms\n", n, line_ms);

    float *freq = malloc((size_t)n * sizeof(float));
    if (!freq) return -3;
    float h[HILBERT_N];
    for (int k=0;k<HILBERT_N;++k) {
        int m=k-HILBERT_M;
        float ideal=(m!=0 && (m&1)) ? (2.0f/(PI*(float)m)) : 0.0f;
        float win=0.54f-0.46f*cosf(2.0f*PI*(float)k/(float)(HILBERT_N-1));
        h[k]=ideal*win;
    }
    float prev_phase=0.0f, ring[SMOOTH_N]={0}, sum=0.0f; int rp=0;
    for (long i=0;i<n;++i) {
        float q=0.0f;
        if (i>=HILBERT_N-1) for (int k=0;k<HILBERT_N;++k) q += h[k]*x[i-k];
        float re=(i>=HILBERT_M)?x[i-HILBERT_M]:0.0f;
        float phase=atan2f(q,re), d=phase-prev_phase; prev_phase=phase;
        while(d>PI)d-=2.0f*PI; while(d<-PI)d+=2.0f*PI;
        float f=fabsf(d)*((float)SR/(2.0f*PI)); if(f>4000.0f)f=4000.0f;
        sum-=ring[rp]; ring[rp]=f; sum+=f; rp=(rp+1)%SMOOTH_N; freq[i]=sum/(float)SMOOTH_N;
    }

    long cand[180];
    int nc=detect_sync_1200_robust(x,n,sync_ms,cand,180);
    fprintf(stderr,"SSTV: Robot 8 BW: %d sync candidates\n",nc);

    long sync[120]; double fitted_spl=0.0;
    double nominal_spl=(double)line_ms*(double)SR/1000.0;
    int grid_rc=robust_sync_grid("Robot 8 BW",cand,nc,height,nominal_spl,8,sync,&fitted_spl);
    if(grid_rc<0){
        double first=2.0*(double)SR; fitted_spl=nominal_spl;
        if(nc>0){long best=cand[0];double bd=fabs((double)best-first);for(int k=1;k<nc;++k){double d=fabs((double)cand[k]-first);if(d<bd){bd=d;best=cand[k];}}if(bd<=nominal_spl)first=(double)best;}
        for(int row=0;row<height;++row)sync[row]=(long)llround(first+nominal_spl*(double)row);
        fprintf(stderr,"SSTV: Robot 8 BW: FALLBACK nominal timing grid; rendering degraded image\n");
    } else if ((double)sync[0] > 5.0*(double)SR && fitted_spl>0.0) {
        const double expected_first=2.0*(double)SR;
        long shift_lines=(long)llround(((double)sync[0]-expected_first)/fitted_spl);
        if(shift_lines>0 && shift_lines<height){long ds=(long)llround((double)shift_lines*fitted_spl);double old=(double)sync[0]/SR;for(int r=0;r<height;++r)sync[r]-=ds;fprintf(stderr,"SSTV: Robot 8 BW: late sparse sync chain %.3f s -> shift back %ld lines, first=%.3f s\n",old,shift_lines,(double)sync[0]/SR);}
    }
    int ns=height;
    double timing_scale=fitted_spl/nominal_spl;

    float Y[160];
    for(int row=0;row<ns;++row){
        long a=sync[row]+(long)llround(sync_ms*SR/1000.0*timing_scale);
        long b=a+(long)llround(video_ms*SR/1000.0*timing_scale);
        if(b>=n){ns=row;break;}
        segment(freq,n,a,b,Y,width);
        for(int px=0;px<width;++px){uint8_t y=clamp8(Y[px]);size_t o=((size_t)row*width+px)*3;rgb[o]=rgb[o+1]=rgb[o+2]=y;}
        if((row%20)==19 || row==ns-1) fprintf(stderr,"SSTV: Robot 8 BW: decoded row %d/%d\n",row+1,ns);
    }
    if(ns<height) memset(rgb+(size_t)ns*width*3,0,(size_t)(height-ns)*width*3);
    free(freq);
    fprintf(stderr,"SSTV: Robot 8 BW image decode finished, rows=%d\n",ns);
    return ns;
}


static int robot36_decode_samples(const float *x,long n,uint8_t *rgb)
{
    const int width=320,height=240; const float line_ms=150.0f;
    if(!x||n<=0||!rgb)return -1;
    fprintf(stderr,"SSTV: Robot 36 decoder start from RAM: samples=%ld, 320x240, line=%.3f ms\n",n,line_ms);
    float *freq=malloc((size_t)n*sizeof(float)); if(!freq)return -3;
    float h[HILBERT_N]; for(int k=0;k<HILBERT_N;++k){int m=k-HILBERT_M;float ideal=(m!=0&&(m&1))?2.0f/(PI*m):0;float win=.54f-.46f*cosf(2*PI*k/(HILBERT_N-1));h[k]=ideal*win;}
    float prev=0,ring[SMOOTH_N]={0},sum=0;int rp=0;
    for(long i=0;i<n;++i){float q=0;if(i>=HILBERT_N-1)for(int k=0;k<HILBERT_N;++k)q+=h[k]*x[i-k];float re=i>=HILBERT_M?x[i-HILBERT_M]:0;float ph=atan2f(q,re),d=ph-prev;prev=ph;while(d>PI)d-=2*PI;while(d<-PI)d+=2*PI;float f=fabsf(d)*SR/(2*PI);if(f>4000)f=4000;sum-=ring[rp];ring[rp]=f;sum+=f;rp=(rp+1)%SMOOTH_N;freq[i]=sum/SMOOTH_N;}
    long cand[MAX_SYNCS];
    int nc=detect_sync_1200_robust(x,n,9.0f,cand,MAX_SYNCS);
    fprintf(stderr,"SSTV: Robot 36: %d sync candidates\n",nc);
    long sync[240];double fitted_spl=0,nominal_spl=(double)line_ms*SR/1000.0;
    int grid_rc=robust_sync_grid("Robot 36",cand,nc,height,nominal_spl,8,sync,&fitted_spl);
    if(grid_rc<0){
        double first=2.0*(double)SR; fitted_spl=nominal_spl;
        if(nc>0){long best=cand[0];double bd=fabs((double)best-first);for(int k=1;k<nc;++k){double d=fabs((double)cand[k]-first);if(d<bd){bd=d;best=cand[k];}}if(bd<=nominal_spl)first=(double)best;}
        for(int row=0;row<height;++row)sync[row]=(long)llround(first+nominal_spl*(double)row);
        fprintf(stderr,"SSTV: Robot 36: FALLBACK nominal timing grid; rendering degraded image\n");
    } else if ((double)sync[0] > 5.0*(double)SR && fitted_spl>0.0) {
        const double expected_first=2.0*(double)SR;
        long shift_lines=(long)llround(((double)sync[0]-expected_first)/fitted_spl);
        if(shift_lines>0 && shift_lines<height){long ds=(long)llround((double)shift_lines*fitted_spl);double old=(double)sync[0]/SR;for(int r=0;r<height;++r)sync[r]-=ds;fprintf(stderr,"SSTV: Robot 36: late sparse sync chain %.3f s -> shift back %ld lines, first=%.3f s\n",old,shift_lines,(double)sync[0]/SR);}
    }
    double scale=fitted_spl/nominal_spl; float *Y=calloc((size_t)height*width,sizeof(float)),*C=calloc((size_t)height*width,sizeof(float)),c160[160];if(!Y||!C){free(Y);free(C);free(freq);return -6;}
    int ns=height;
    for(int r=0;r<height;++r){long base=sync[r];long ya=base+llround(.012*SR*scale),yb=base+llround(.100*SR*scale),ca=base+llround(.106*SR*scale),cb=base+llround(.150*SR*scale);if(base<0||cb>=n){ns=r;break;}segment(freq,n,ya,yb,&Y[r*width],width);segment(freq,n,ca,cb,c160,160);for(int p=0;p<width;++p){float u=p*159.0f/319.0f;int j=(int)u;float t=u-j;C[r*width+p]=((j<159)?c160[j]+(c160[j+1]-c160[j])*t:c160[159])-128;}}
    for(int r=0;r<ns;++r)for(int p=0;p<width;++p){float cr,cb;if(r&1){cr=C[r*width+p];cb=(r>0&&r+1<ns)?.5f*(C[(r-1)*width+p]+C[(r+1)*width+p]):(r>0?C[(r-1)*width+p]:C[r*width+p]);}else{cb=C[r*width+p];cr=(r>0&&r+1<ns)?.5f*(C[(r-1)*width+p]+C[(r+1)*width+p]):(r+1<ns?C[(r+1)*width+p]:C[r*width+p]);}float y=Y[r*width+p];size_t o=((size_t)r*width+p)*3;rgb[o]=clamp8(y+1.402f*cr);rgb[o+1]=clamp8(y-.714136f*cr-.344136f*cb);rgb[o+2]=clamp8(y+1.772f*cb);}
    if(ns<height)memset(rgb+(size_t)ns*width*3,0,(size_t)(height-ns)*width*3);
    fprintf(stderr,"SSTV: Robot 36 image decode finished, rows=%d\n",ns);free(Y);free(C);free(freq);return ns;
}

static int scottie_decode_samples(const float *x,long n,uint8_t *rgb,sstv_mode_t mode)
{
    const int width=320,height=256;
    const float sync_ms=9.0f,sep_ms=1.5f;
    float channel_ms; const char *mode_name;
    switch(mode){
        case SSTV_MODE_SCOTTIE_S1: channel_ms=138.240f; mode_name="Scottie S1"; break;
        case SSTV_MODE_SCOTTIE_S2: channel_ms=88.064f; mode_name="Scottie S2"; break;
        case SSTV_MODE_SCOTTIE_DX: channel_ms=345.600f; mode_name="Scottie DX"; break;
        default: return -20;
    }
    const float line_ms=sync_ms+3.0f*sep_ms+3.0f*channel_ms;
    if(!x||n<=0||!rgb)return -1;fprintf(stderr,"SSTV: %s decoder start from RAM: samples=%ld, line=%.3f ms\n",mode_name,n,line_ms);
    float *freq=malloc((size_t)n*sizeof(float));if(!freq)return -3;float h[HILBERT_N];for(int k=0;k<HILBERT_N;++k){int m=k-HILBERT_M;float ideal=(m!=0&&(m&1))?2.0f/(PI*m):0;float win=.54f-.46f*cosf(2*PI*k/(HILBERT_N-1));h[k]=ideal*win;}float prev=0,ring[SMOOTH_N]={0},sum=0;int rp=0;for(long i=0;i<n;++i){float q=0;if(i>=HILBERT_N-1)for(int k=0;k<HILBERT_N;++k)q+=h[k]*x[i-k];float re=i>=HILBERT_M?x[i-HILBERT_M]:0;float ph=atan2f(q,re),d=ph-prev;prev=ph;while(d>PI)d-=2*PI;while(d<-PI)d+=2*PI;float f=fabsf(d)*SR/(2*PI);if(f>4000)f=4000;sum-=ring[rp];ring[rp]=f;sum+=f;rp=(rp+1)%SMOOTH_N;freq[i]=sum/SMOOTH_N;}
    long cand[MAX_SYNCS];
    int nc=detect_sync_1200_robust(x,n,sync_ms,cand,MAX_SYNCS);
    fprintf(stderr,"SSTV: %s: %d sync candidates\n",mode_name,nc);
    long sync[256];
    double fitted_spl=0,nominal_spl=(double)line_ms*SR/1000.0;
    if(robust_sync_grid(mode_name,cand,nc,height,nominal_spl,8,sync,&fitted_spl)<0){
        /* Last-resort live-RX fallback.  The capture contains a two-second
         * prebuffer and starts immediately after a valid VIS code, so for the
         * Scottie family the first regular B/R sync is normally about 2.23 s
         * into this RAM block (2.229 s in our validated S1 reception).  Even
         * with no usable horizontal-sync chain, render all 256 rows using the
         * nominal VIS timing.  The result may be noisy/slanted/mis-phased, but
         * the user gets a picture instead of an empty screen. */
        double first=(2.229*(double)SR);
        fitted_spl=nominal_spl;
        if(nc>0){
            /* Prefer a real 1200-Hz candidate near the expected first sync,
             * but never jump by more than half a line. */
            long best=cand[0]; double bd=fabs((double)best-first);
            for(int k=1;k<nc;++k){double d=fabs((double)cand[k]-first);if(d<bd){bd=d;best=cand[k];}}
            if(bd <= nominal_spl*0.5) first=(double)best;
        }
        for(int row=0;row<height;++row) sync[row]=(long)llround(first+nominal_spl*(double)row);
        fprintf(stderr,"SSTV: %s: FALLBACK nominal timing grid rows=%d, first=%.3f s; rendering degraded image\n",
                mode_name,height,(double)sync[0]/(double)SR);
    } else {
        /* A sparse Scottie chain can begin well inside the picture.
         * robust_sync_grid() correctly recovers its line period but numbers
         * the first member of that chain as row 0.  Do not disturb normal
         * early/strong locks; only when the reconstructed row 0 is clearly
         * late, shift the whole timing grid backwards by an integer number
         * of fitted lines.  This preserves phase and fitted clock/slant. */
        const double expected_first = 2.229 * (double)SR;
        const double late_limit = 5.0 * (double)SR;
        if ((double)sync[0] > late_limit && fitted_spl > 0.0) {
            long shift_lines = (long)llround(((double)sync[0] - expected_first) / fitted_spl);
            if (shift_lines > 0 && shift_lines < height) {
                long shift_samples = (long)llround((double)shift_lines * fitted_spl);
                double old_first = (double)sync[0] / (double)SR;
                for (int row=0; row<height; ++row) sync[row] -= shift_samples;
                fprintf(stderr,"SSTV: %s: late sparse sync chain: first grid point %.3f s corresponds to about image row %ld\n",
                        mode_name,old_first,shift_lines);
                fprintf(stderr,"SSTV: %s: extrapolated timing grid backward %ld lines -> first=%.3f s last=%.3f s\n",
                        mode_name,shift_lines,(double)sync[0]/(double)SR,(double)sync[height-1]/(double)SR);
            }
        }
    }
    double scale=fitted_spl/nominal_spl;float G[320],B[320],R[320];int ns=height;
    for(int row=0;row<height;++row){long base=sync[row];long ba=base-llround(channel_ms*SR/1000.0*scale),bb=base;long gb=ba-llround(sep_ms*SR/1000.0*scale),ga=gb-llround(channel_ms*SR/1000.0*scale);long ra=base+llround((sync_ms+sep_ms)*SR/1000.0*scale),rb=ra+llround(channel_ms*SR/1000.0*scale);if(ga<0||rb>=n){ns=row;break;}segment(freq,n,ga,gb,G,width);segment(freq,n,ba,bb,B,width);segment(freq,n,ra,rb,R,width);for(int px=0;px<width;++px){size_t o=((size_t)row*width+px)*3;rgb[o]=clamp8(R[px]);rgb[o+1]=clamp8(G[px]);rgb[o+2]=clamp8(B[px]);}if((row%32)==31||row==height-1)fprintf(stderr,"SSTV: %s: decoded row %d/%d\n",mode_name,row+1,height);}
    if(ns<height)memset(rgb+(size_t)ns*width*3,0,(size_t)(height-ns)*width*3);fprintf(stderr,"SSTV: %s image decode finished, rows=%d\n",mode_name,ns);free(freq);return ns;
}

int sstv_decode_samples(const float *samples, long count, sstv_mode_t mode, uint8_t *rgb, sstv_info_t *info)
{
    if (!samples || count <= 0 || !rgb) return -1;
    const mode_desc_t *m=NULL;
    for (unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);++i) if (modes[i].mode==mode) { m=&modes[i]; break; }
    if (!m) return -2;
    if (info) { info->mode=m->mode; info->vis=m->vis; info->name=m->name; info->width=m->w; info->height=m->h; }
    fprintf(stderr,"SSTV LIVE: direct RAM decoder dispatch -> %s (%ld samples @ %d Hz)\n",m->name,count,SR);
    if (mode==SSTV_MODE_ROBOT8_BW)
        return robot8bw_decode_samples(samples,count,rgb);
    if (mode==SSTV_MODE_ROBOT36)
        return robot36_decode_samples(samples,count,rgb);
    if (mode==SSTV_MODE_ROBOT72)
        return robot72_decode_samples(samples,count,rgb);
    if (mode==SSTV_MODE_MARTIN_M1 || mode==SSTV_MODE_MARTIN_M2)
        return martin_decode_samples(samples,count,rgb,mode);
    if (mode==SSTV_MODE_SCOTTIE_S1 || mode==SSTV_MODE_SCOTTIE_S2 || mode==SSTV_MODE_SCOTTIE_DX)
        return scottie_decode_samples(samples,count,rgb,mode);
    fprintf(stderr,"SSTV LIVE: direct RAM decoder for %s not enabled yet\n",m->name);
    return -20;
}


int sstv_decode_cont_samples(const float *samples, long count, sstv_mode_t mode, uint8_t *rgb, sstv_info_t *info)
{
    if (!samples || count <= 0 || !rgb) return -1;

    /* Keep Auto RX completely unchanged. Its RAM decoders expect the first
     * useful image sync roughly two seconds into the block because normal
     * Auto captures include the VIS/prebuffer area. A Cont capture begins at
     * an arbitrary point, so prepend silence up to the normal per-family
     * anchor. Real sync candidates are merely shifted in time; robust sync,
     * clock regression and global refinement then run exactly as before.
     * If Start happened in the middle of a line, at worst the first rendered
     * row is partial; subsequent rows lock to the real sync grid. */
    double preroll_s;
    switch (mode) {
        case SSTV_MODE_MARTIN_M1: preroll_s = 2.350; break;
        case SSTV_MODE_MARTIN_M2: preroll_s = 2.200; break;
        case SSTV_MODE_SCOTTIE_S1:
        case SSTV_MODE_SCOTTIE_S2:
        case SSTV_MODE_SCOTTIE_DX: preroll_s = 2.229; break;
        case SSTV_MODE_ROBOT8_BW:
        case SSTV_MODE_ROBOT36:
        case SSTV_MODE_ROBOT72: preroll_s = 2.000; break;
        default: return -2;
    }

    /* The robust decoders read a first sync more than 5 s into the block as
     * a late chain inside the picture.  When the picture starts late in the
     * capture (Start pressed before the transmission), cut the audio before
     * it: a few lines before the first sync of the line-sync tracker's lock. */
    {
        sstv_linesync_t ls;
        sstv_linesync_init(&ls, mode);
        sstv_linesync_process(&ls, samples, (size_t)count);
        if (ls.st.state != SSTV_LS_SEARCH) {
            double back = 4.0 * sstv_linesync_line_seconds(mode);
            if (back > 2.5) back = 2.5;
            long a = (long)ls.st.first_sync - (long)llround((back + 0.2) * SR);
            if (a > 0) {
                fprintf(stderr, "SSTV CONT: picture starts %.1f s into the capture; skipping the audio before it\n",
                        (double)ls.st.first_sync / SR);
                samples += a;
                count -= a;
            }
        }
    }

    long pad = (long)llround(preroll_s * (double)SR);
    long total = pad + count;
    float *work = calloc((size_t)total, sizeof(float));
    if (!work) return -3;
    memcpy(work + pad, samples, (size_t)count * sizeof(float));

    fprintf(stderr,
            "SSTV CONT: manual mode=%d capture: %ld samples, synthetic pre-roll %.3f s; dispatching unchanged RAM decoder\n",
            (int)mode, count, preroll_s);
    int rows = sstv_decode_samples(work, total, mode, rgb, info);
    free(work);
    return rows;
}

/* ---- Progressive display support (see decoder.h) ---- */

float *sstv_fm_demod(const float *x, long n)
{
    float *freq = (x && n > 0) ? malloc((size_t)n * sizeof(float)) : NULL;
    if (!freq) return NULL;
    float h[HILBERT_N];
    for (int k = 0; k < HILBERT_N; ++k) {
        int m = k - HILBERT_M;
        float ideal = (m != 0 && (m & 1)) ? (2.0f / (PI * (float)m)) : 0.0f;
        h[k] = ideal * (0.54f - 0.46f * cosf(2.0f * PI * (float)k / (float)(HILBERT_N - 1)));
    }
    float prev = 0.0f, ring[SMOOTH_N] = {0}, sum = 0.0f;
    int rp = 0;
    for (long i = 0; i < n; ++i) {
        float q = 0.0f;
        if (i >= HILBERT_N - 1)
            for (int k = 0; k < HILBERT_N; ++k) q += h[k] * x[i - k];
        float re = (i >= HILBERT_M) ? x[i - HILBERT_M] : 0.0f;
        float ph = atan2f(q, re), d = ph - prev;
        prev = ph;
        while (d > PI) d -= 2.0f * PI;
        while (d < -PI) d += 2.0f * PI;
        float f = fabsf(d) * ((float)SR / (2.0f * PI));
        if (f > 4000.0f) f = 4000.0f;
        sum -= ring[rp]; ring[rp] = f; sum += f;
        rp = (rp + 1) % SMOOTH_N;
        freq[i] = sum / (float)SMOOTH_N;
    }
    return freq;
}

int sstv_row_width(sstv_mode_t mode)
{
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
        if (modes[i].mode == mode) return modes[i].w;
    return 0;
}

static double scottie_channel_ms(sstv_mode_t mode)
{
    return mode == SSTV_MODE_SCOTTIE_S1 ? 138.240 : mode == SSTV_MODE_SCOTTIE_S2 ? 88.064 :
           mode == SSTV_MODE_SCOTTIE_DX ? 345.600 : 0.0;
}

double sstv_row0_anchor_after_vis(sstv_mode_t mode)
{
    double ch = scottie_channel_ms(mode);
    if (ch > 0.0) return (9.0 + 1.5 + ch + 1.5 + ch) / 1000.0;   /* start sync, G, B, then the B/R sync */
    return sstv_row_width(mode) > 0 ? 0.0 : -1.0;
}

int sstv_render_row(sstv_mode_t mode, const float *freq, long n, long base, double scale, uint8_t *rgb_row)
{
    const double k = (double)SR / 1000.0 * scale;      /* samples per ms */
    float a[320], b[320], c[320];
    int w = sstv_row_width(mode);
    double ch = scottie_channel_ms(mode);
#define AT(ms) (base + (long)llround((ms) * k))
    switch (mode) {
    case SSTV_MODE_ROBOT8_BW:
        if (base < 0 || AT(66.0) >= n) return -1;
        segment(freq, n, AT(10.0), AT(66.0), a, w);
        for (int x = 0; x < w; ++x) rgb_row[x*3] = rgb_row[x*3+1] = rgb_row[x*3+2] = clamp8(a[x]);
        return 0;
    case SSTV_MODE_ROBOT36:
        if (base < 0 || AT(100.0) >= n) return -1;
        segment(freq, n, AT(12.0), AT(100.0), a, w);
        for (int x = 0; x < w; ++x) rgb_row[x*3] = rgb_row[x*3+1] = rgb_row[x*3+2] = clamp8(a[x]);
        return 0;
    case SSTV_MODE_ROBOT72:
        if (base < 0 || AT(300.0) >= n) return -1;
        segment(freq, n, AT(12.0), AT(150.0), a, w);
        segment(freq, n, AT(156.0), AT(225.0), b, w);
        segment(freq, n, AT(231.0), AT(300.0), c, w);
        for (int x = 0; x < w; ++x) {
            float y = a[x], cr = b[x] - 128.0f, cb = c[x] - 128.0f;
            rgb_row[x*3] = clamp8(y + 1.402f*cr);
            rgb_row[x*3+1] = clamp8(y - 0.714136f*cr - 0.344136f*cb);
            rgb_row[x*3+2] = clamp8(y + 1.772f*cb);
        }
        return 0;
    case SSTV_MODE_MARTIN_M1:
    case SSTV_MODE_MARTIN_M2: {
        double m = mode == SSTV_MODE_MARTIN_M1 ? 146.432 : 73.216, g0 = 4.862 + 0.572;
        double b0 = g0 + m + 0.572, r0 = b0 + m + 0.572;
        if (base < 0 || AT(r0 + m) >= n) return -1;
        segment(freq, n, AT(g0), AT(g0 + m), a, w);
        segment(freq, n, AT(b0), AT(b0 + m), b, w);
        segment(freq, n, AT(r0), AT(r0 + m), c, w);
        for (int x = 0; x < w; ++x) { rgb_row[x*3] = clamp8(c[x]); rgb_row[x*3+1] = clamp8(a[x]); rgb_row[x*3+2] = clamp8(b[x]); }
        return 0;
    }
    case SSTV_MODE_SCOTTIE_S1:
    case SSTV_MODE_SCOTTIE_S2:
    case SSTV_MODE_SCOTTIE_DX:
        /* anchored on the B/R sync: G and B before it, R after it */
        if (AT(-(2.0 * ch + 1.5)) < 0 || AT(10.5 + ch) >= n) return -1;
        segment(freq, n, AT(-(2.0 * ch + 1.5)), AT(-(ch + 1.5)), a, w);
        segment(freq, n, AT(-ch), base, b, w);
        segment(freq, n, AT(10.5), AT(10.5 + ch), c, w);
        for (int x = 0; x < w; ++x) { rgb_row[x*3] = clamp8(c[x]); rgb_row[x*3+1] = clamp8(a[x]); rgb_row[x*3+2] = clamp8(b[x]); }
        return 0;
    default:
        return -1;
    }
#undef AT
}

/* Scottie family decoder.  Unlike Martin, the regular 1200 Hz sync pulse
 * sits between Blue and Red.  For S1 a regular line is:
 *   1500 sep, G, 1500 sep, B, 1200 sync, 1500 porch, R
 * The image begins with one extra 9 ms starting sync before the first G.
 * We anchor each decoded row on the regular sync between B and R: G/B are
 * immediately before it and R is immediately after it. */
static int scottie_decode_wav(const char *path, uint8_t *rgb, sstv_mode_t mode)
{
    const char *mode_name;
    float channel_ms;
    switch (mode) {
        case SSTV_MODE_SCOTTIE_S1: channel_ms = 138.240f; mode_name = "S1"; break;
        case SSTV_MODE_SCOTTIE_S2: channel_ms =  88.064f; mode_name = "S2"; break;
        case SSTV_MODE_SCOTTIE_DX: channel_ms = 345.600f; mode_name = "DX"; break;
        default:
            fprintf(stderr, "SSTV: Scottie: mode %d not implemented\n", (int)mode);
            return -20;
    }

    const float sync_ms = 9.000f;
    const float sep_ms = 1.500f;
    const float line_ms = sync_ms + 3.0f*sep_ms + 3.0f*channel_ms;
    const int height = 256;

    fprintf(stderr, "SSTV: Scottie decoder start: %s, channel=%.3f ms line=%.3f ms\n",
            mode_name, channel_ms, line_ms);

    SF_INFO info;
    memset(&info, 0, sizeof(info));
    SNDFILE *sf = sf_open(path, SFM_READ, &info);
    if (!sf) {
        fprintf(stderr, "SSTV: Scottie: cannot open %s: %s\n", path, sf_strerror(NULL));
        return -1;
    }
    if (info.channels != 1 || info.samplerate != SR) {
        fprintf(stderr, "SSTV: Scottie: WAV must be mono 11025 Hz\n");
        sf_close(sf);
        return -2;
    }

    long n = (long)info.frames;
    float *x = malloc((size_t)n * sizeof(float));
    float *freq = malloc((size_t)n * sizeof(float));
    if (!x || !freq) {
        free(x); free(freq); sf_close(sf);
        return -3;
    }
    sf_count_t got = sf_readf_float(sf, x, info.frames);
    sf_close(sf);
    if (got != info.frames) {
        free(x); free(freq);
        return -4;
    }

    float h[HILBERT_N];
    for (int k = 0; k < HILBERT_N; ++k) {
        int m = k - HILBERT_M;
        float ideal = (m != 0 && (m & 1)) ? (2.0f / (PI * (float)m)) : 0.0f;
        float win = 0.54f - 0.46f * cosf(2.0f * PI * (float)k / (float)(HILBERT_N - 1));
        h[k] = ideal * win;
    }

    float prev_phase = 0.0f;
    float ring[SMOOTH_N] = {0};
    float sum = 0.0f;
    int rp = 0;
    for (long i = 0; i < n; ++i) {
        float q = 0.0f;
        if (i >= HILBERT_N - 1)
            for (int k = 0; k < HILBERT_N; ++k) q += h[k] * x[i-k];
        float re = (i >= HILBERT_M) ? x[i-HILBERT_M] : 0.0f;
        float phase = atan2f(q, re);
        float d = phase - prev_phase;
        prev_phase = phase;
        while (d > PI) d -= 2.0f * PI;
        while (d < -PI) d += 2.0f * PI;
        float f = fabsf(d) * ((float)SR / (2.0f * PI));
        if (f > 4000.0f) f = 4000.0f;
        sum -= ring[rp]; ring[rp] = f; sum += f;
        rp = (rp + 1) % SMOOTH_N;
        freq[i] = sum / (float)SMOOTH_N;
    }

    /* Find Scottie's nominal 9 ms 1200 Hz sync pulses. */
    long cand[MAX_SYNCS];
    int nc = 0;
    long i = SR / 2;
    while (i < n && nc < MAX_SYNCS) {
        if (freq[i] > 1080.0f && freq[i] < 1320.0f) {
            long a = i;
            while (i < n && freq[i] > 1080.0f && freq[i] < 1320.0f) ++i;
            float ms = (float)(i-a) * 1000.0f / (float)SR;
            if (ms >= 6.0f && ms <= 13.0f) cand[nc++] = a;
        } else ++i;
    }
    fprintf(stderr, "SSTV: Scottie: %d sync candidates\n", nc);

    const float nominal = line_ms / 1000.0f;
    const float lo = nominal * 0.94f;
    const float hi = nominal * 1.06f;
    long sync[256];
    int ns = 0, best_start = -1, best_len = 0;
    for (int st = 0; st < nc; ++st) {
        int len = 1; long last = cand[st];
        for (int j = st+1; j < nc && len < height; ++j) {
            float dt = (float)(cand[j]-last)/(float)SR;
            if (dt > lo && dt < hi) { ++len; last = cand[j]; }
            else if (dt >= hi) break;
        }
        if (len > best_len) { best_len=len; best_start=st; }
    }
    if (best_start < 0 || best_len < 20) {
        fprintf(stderr, "SSTV: Scottie: sync chain NOT found (best=%d)\n", best_len);
        free(x); free(freq); return -5;
    }

    long last = cand[best_start]; sync[ns++] = last;
    for (int j=best_start+1; j<nc && ns<height; ++j) {
        float dt=(float)(cand[j]-last)/(float)SR;
        if (dt > lo && dt < hi) { last=cand[j]; sync[ns++]=last; }
        else if (dt >= hi) break;
    }
    fprintf(stderr, "SSTV: Scottie: selected regular-sync chain rows=%d, first=%.3f s\n",
            ns, (float)sync[0]/(float)SR);

    float G[320], B[320], R[320];
    for (int row=0; row<ns; ++row) {
        long base = sync[row];
        long ba = base - (long)(channel_ms*SR/1000.0f);
        long bb = base;
        long gb = ba - (long)(sep_ms*SR/1000.0f);
        long ga = gb - (long)(channel_ms*SR/1000.0f);
        long ra = base + (long)((sync_ms+sep_ms)*SR/1000.0f);
        long rb = ra + (long)(channel_ms*SR/1000.0f);
        if (ga < 0 || rb >= n) {
            fprintf(stderr, "SSTV: Scottie: bounds/EOF at row %d (ga=%ld rb=%ld n=%ld)\n",
                    row, ga, rb, n);
            ns=row;
            break;
        }
        segment(freq,n,ga,gb,G,320);
        segment(freq,n,ba,bb,B,320);
        segment(freq,n,ra,rb,R,320);
        for (int px=0; px<320; ++px) {
            size_t o=((size_t)row*320+px)*3;
            rgb[o+0]=clamp8(R[px]);
            rgb[o+1]=clamp8(G[px]);
            rgb[o+2]=clamp8(B[px]);
        }
        if ((row % 32) == 31 || row == ns-1)
            fprintf(stderr, "SSTV: Scottie: decoded row %d/%d\n", row+1, ns);
    }
    if (ns < height)
        memset(rgb+(size_t)ns*320*3,0,(size_t)(height-ns)*320*3);

    free(x); free(freq);
    fprintf(stderr, "SSTV: Scottie %s image decode finished, rows=%d\n", mode_name, ns);
    return ns;
}


int sstv_detect_wav(const char *path, sstv_info_t *info)
{
    if (!info) return -1;
    memset(info,0,sizeof(*info));
    fprintf(stderr,"SSTV: opening %s\n",path);
    SF_INFO si={0}; SNDFILE *sf=sf_open(path,SFM_READ,&si);
    if (!sf) { fprintf(stderr,"SSTV: open failed: %s\n",sf_strerror(NULL)); return -2; }
    if (si.channels!=1 || si.samplerate!=SR) { sf_close(sf); fprintf(stderr,"SSTV: unsupported WAV format\n"); return -3; }
    float *x=malloc((size_t)si.frames*sizeof(float));
    if (!x) {sf_close(sf); return -4;}
    sf_count_t got=sf_readf_float(sf,x,si.frames); sf_close(sf);
    if (got!=si.frames) {free(x); return -5;}

    /* Same scan as Auto RX: 1-s blocks every 50 ms, VIS starting in the first 5 s */
    long n=(long)si.frames;
    for (long w=0; w<=5L*SR && w<n; w+=SR/20) {
        long len=n-w < SR ? n-w : SR;
        sstv_vis_result_t r;
        if (sstv_vis_detect(x+w,(int)len,info,&r)) {
            fprintf(stderr,"SSTV: VIS=%d -> %s (%dx%d) at %.3f s, offset %+.1f Hz\n",
                    info->vis,info->name,info->width,info->height,(double)w/SR+r.start_s,r.offset_hz);
            free(x); return 0;
        }
        if (r.unsupported_vis>=0) {
            fprintf(stderr,"SSTV: unsupported VIS %d\n",r.unsupported_vis);
            free(x); return -7;
        }
        if (len<SR) break;
    }
    fprintf(stderr,"SSTV: no valid supported VIS found\n");
    free(x); return -6;
}

int sstv_decode_wav(const char *path, uint8_t *rgb, sstv_info_t *info)
{
    sstv_info_t local;
    if (!info) info=&local;
    int rc=sstv_detect_wav(path,info);
    if (rc<0) return rc;

    fprintf(stderr,"SSTV TEST: loading WAV into RAM so TEST uses the same decoder path as LIVE RF\n");
    SF_INFO si={0};
    SNDFILE *sf=sf_open(path,SFM_READ,&si);
    if (!sf) { fprintf(stderr,"SSTV TEST: cannot reopen %s: %s\n",path,sf_strerror(NULL)); return -2; }
    if (si.channels!=1 || si.samplerate!=SR) { sf_close(sf); return -3; }
    float *samples=malloc((size_t)si.frames*sizeof(float));
    if (!samples) { sf_close(sf); return -4; }
    sf_count_t got=sf_readf_float(sf,samples,si.frames);
    sf_close(sf);
    if (got!=si.frames) { free(samples); return -5; }

    fprintf(stderr,"SSTV TEST: decoder dispatch -> %s through common RAM decoder (%lld samples @ %d Hz)\n",
            info->name,(long long)si.frames,SR);

    int rows=sstv_decode_samples(samples,(long)si.frames,info->mode,rgb,info);
    free(samples);
    fprintf(stderr,"SSTV TEST: common RAM image decode returned rows=%d\n",rows);
    return rows;
}

