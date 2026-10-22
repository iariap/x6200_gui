#ifndef SSTV_DECODER_H
#define SSTV_DECODER_H

#include <stdint.h>

#define SSTV_SAMPLE_RATE 11025
#define SSTV_MAX_W 320
#define SSTV_MAX_H 256
#define SSTV_ROBOT36_W 320
#define SSTV_ROBOT36_H 240

typedef enum {
    SSTV_MODE_UNKNOWN = 0,
    SSTV_MODE_ROBOT8_BW,
    SSTV_MODE_ROBOT36,
    SSTV_MODE_ROBOT72,
    SSTV_MODE_MARTIN_M1,
    SSTV_MODE_MARTIN_M2,
    SSTV_MODE_SCOTTIE_S1,
    SSTV_MODE_SCOTTIE_S2,
    SSTV_MODE_SCOTTIE_DX
} sstv_mode_t;

typedef struct {
    sstv_mode_t mode;
    int vis;
    int width;
    int height;
    const char *name;
} sstv_info_t;

/* Detect the SSTV VIS header in a mono 11025 Hz PCM WAV.
 * Returns 0 on success, <0 on error.  The filename is never used to
 * determine the mode.
 */
int sstv_detect_wav(const char *path, sstv_info_t *info);

/* Current image decoder.  Detection is automatic; at this stage only
 * Robot 36 image decoding is implemented.  rgb must hold at least
 * SSTV_MAX_W*SSTV_MAX_H*3 bytes.  Returns decoded rows, or <0.
 */
int sstv_decode_wav(const char *path, uint8_t *rgb, sstv_info_t *info);

/* Decode an already-captured 11025 Hz mono sample buffer. No VIS re-detection. */
int sstv_decode_samples(const float *samples, long count, sstv_mode_t mode, uint8_t *rgb, sstv_info_t *info);

/* Manual continuous RX decoder. The capture may start/end anywhere inside a
 * transmission and contains no VIS header. The normal Auto decoder path is
 * intentionally left untouched; this wrapper only supplies a synthetic
 * pre-roll so the existing robust per-mode decoders can lock to the first
 * usable sync found in the arbitrary capture. */
int sstv_decode_cont_samples(const float *samples, long count, sstv_mode_t mode, uint8_t *rgb, sstv_info_t *info);

/* ---- Progressive display (sstv/preview.c) ----
 * FM front end of the decoders: frequency (Hz) of every sample of x[0..n).
 * Returns a malloc'd array (free() it) or NULL.  Keep ~100 samples of
 * margin on each side of a row when applied to a slice. */
float *sstv_fm_demod(const float *x, long n);

/* Width in pixels of a row of mode (160 for Robot 8, else 320; 0 unknown). */
int sstv_row_width(sstv_mode_t mode);

/* Seconds from the end of the VIS to the anchor sync of picture row 0 (the
 * line sync for Robot/Martin, the B/R sync for Scottie); <0 if unknown. */
double sstv_row0_anchor_after_vis(sstv_mode_t mode);

/* Draw one row of mode into rgb_row (sstv_row_width() * 3 bytes) with the
 * decoders' channel layout.  freq[0..n): sstv_fm_demod() output; base: index
 * in freq of the row's anchor sync; scale: measured / nominal line period.
 * Robot 36 is drawn in grey (its chroma alternates between rows).  Returns 0,
 * or <0 if the row is not all inside freq[0..n) or the mode is unknown. */
int sstv_render_row(sstv_mode_t mode, const float *freq, long n, long base, double scale, uint8_t *rgb_row);

#endif
