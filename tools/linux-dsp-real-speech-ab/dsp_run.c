/* Corrida real-DSP sobre WAV (CMU ARCTIC + degradaciones simuladas). Offline, sin radio. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_enhancement.h"
uint32_t lv_tick_get(void) { static uint32_t t; t += 10; return t; }

static int16_t *read_wav(const char *p, int *n, uint32_t *rate) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    uint8_t h[44]; if (fread(h,1,44,f)!=44) { fclose(f); return NULL; }
    if (memcmp(h,"RIFF",4)||memcmp(h+8,"WAVE",4)) { fclose(f); return NULL; }
    int ch = h[22] | (h[23]<<8); *rate = (uint32_t)(h[24]|(h[25]<<8)|(h[26]<<16)|((uint32_t)h[27]<<24));
    int bits = h[34] | (h[35]<<8); uint32_t datasz = (uint32_t)(h[40]|(h[41]<<8)|(h[42]<<16)|((uint32_t)h[43]<<24));
    if (ch != 1 || bits != 16) { fclose(f); return NULL; }
    int16_t *buf = malloc(datasz); if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf,1,datasz,f); fclose(f); *n = (int)(rd/2); return buf;
}
static int write_wav(const char *p, const int16_t *buf, int n, uint32_t rate) {
    FILE *f = fopen(p,"wb"); if(!f) return -1;
    uint32_t datasz=(uint32_t)n*2, riff=36+datasz; uint16_t pcm=1,ch=1,bps=16,ba=2; uint32_t br=rate*2;
    fwrite("RIFF",1,4,f); fwrite(&riff,4,1,f); fwrite("WAVEfmt ",1,8,f); uint32_t sz=16; fwrite(&sz,4,1,f);
    fwrite(&pcm,2,1,f); fwrite(&ch,2,1,f); fwrite(&rate,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bps,2,1,f);
    fwrite("data",1,4,f); fwrite(&datasz,4,1,f); fwrite(buf,2,(size_t)n,f); fclose(f); return 0;
}
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr,"uso: %s in.wav out.wav mode(B|C|D1|D2)\n",argv[0]); return 2; }
    int n=0; uint32_t rate=0; int16_t *x=read_wav(argv[1],&n,&rate);
    if(!x){ fprintf(stderr,"WAV invalido\n"); return 1; }
    audio_enh_init();
    if (strcmp(argv[3],"B")==0) audio_enh_preset_load(AUDIO_ENH_PRESET_OFF);
    else if (strcmp(argv[3],"C")==0) audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
    else if (strcmp(argv[3],"D1")==0) { audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE); audio_enh_af_leveler_set_enabled(true); audio_enh_af_leveler_set_target(-20.0f); audio_enh_af_leveler_set_mode(AF_LEVELER_MEDIUM); audio_enh_nr_set_level(30); }
    else if (strcmp(argv[3],"D2")==0) { audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE); audio_enh_nr_set_level(55); }
    else { fprintf(stderr,"modo invalido\n"); free(x); return 2; }
    for (int p=0;p<n;p+=AUDIO_ENH_FRAME_SIZE) { int c=(n-p<AUDIO_ENH_FRAME_SIZE)?(n-p):AUDIO_ENH_FRAME_SIZE; audio_enh_process(x+p,(size_t)c); }
    int rc=write_wav(argv[2],x,n,rate); audio_enh_deinit(); free(x); return rc;
}
