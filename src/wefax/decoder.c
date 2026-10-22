/*
 * DWD WeFax decoder
 *
 * APT start/stop detection and phasing alignment from 1KO125's x6100_gui
 * 1.0.2 (Hany El Imam)
 *
 * Fixed format:
 *
 *   Sample rate : 11025 Hz
 *   Center      : 1500 Hz
 *   Shift       : 850 Hz
 *   IOC         : 576
 *   LPM         : 120
 *   Width       : 1809 pixels
 */

#include "decoder.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>


#define WEFAX_PI       3.14159265358979323846f
#define WEFAX_LPF_HZ   600.0f

/*
 * Complex baseband FIR low-pass.
 *
 * 63 taps, Hamming-windowed sinc, generated once when the
 * decoder is created.  Cutoff remains 600 Hz: this experiment
 * changes only the filter shape, not the nominal bandwidth.
 */
#define WEFAX_FIR_TAPS  63


/*
 * Tone histogram used only for diagnostics.
 *
 * We are interested in the useful WEFAX audio
 * region. 500..2500 Hz gives plenty of margin
 * around the nominal 1075..1925 Hz range.
 *
 * One bin = 1 Hz.
 */
#define TONE_HIST_MIN_HZ     500
#define TONE_HIST_MAX_HZ    2500
#define TONE_HIST_SIZE       \
    (TONE_HIST_MAX_HZ - TONE_HIST_MIN_HZ + 1)


/*
 * DWD IOC 576 APT START detector.
 */
#define WEFAX_APT_START_HZ          300.0f
#define WEFAX_APT_START_TOLERANCE     30.0f
#define WEFAX_APT_WINDOW_SAMPLES    (WEFAX_SAMPLE_RATE / 2)
#define WEFAX_APT_WHITE_THRESHOLD   215
#define WEFAX_APT_BLACK_THRESHOLD    40
#define WEFAX_APT_START_WINDOWS       2
#define WEFAX_APT_STOP_HZ           450.0f
#define WEFAX_APT_STOP_TOLERANCE      30.0f // 6.0f
#define WEFAX_APT_STOP_WINDOWS        1

/*
 * APT STOP is measured with thresholds derived from the window itself
 * (5th/95th percentile of the demodulated gray level), so that a receiver
 * mistuned by more than ~110 Hz does not hide the 450 Hz tone.  The fixed
 * 215/40 thresholds are kept for START.  A window whose gray range is
 * narrower than this is not a black/white tone and counts as 0 Hz.
 */
#define WEFAX_APT_STOP_MIN_RANGE     150

/*
 * Second, independent STOP criterion: narrow-band energy at 450 Hz.
 *
 * Counting black->white transitions collapses as soon as noise makes the
 * demodulated square wave miss a few thresholds, while the 450 Hz spectral
 * line of the gray signal survives far lower SNR (simulation: still 100%
 * of the windows at -2 dB where the transition count is at 0% below ~3 dB).
 * The measure is the fraction of the window's gray-level power found in the
 * three 2 Hz bins around 450 Hz (2.0002 Hz at 11025 Hz).  Measured on
 * real saved faxes: image rows never exceed ~0.017, rows carrying the stop tone are 0.5..0.6 (0.10 or
 * more even when the tone fills only part of the row).
 */
#define WEFAX_APT_STOP_LINE_MIN      0.10f



/* Confirm a provisional START only after a sustained black band. */
/*
 * A phasing row is black except for its ~5% white pulse, so at most ~95%
 * of the pixels can be dark.  Measured on real faxes (3 saved images) the
 * old 50/90% test passed only 20 of 39 phasing rows on the worst one, the
 * 80/85% test passes 34 of 39, and not a single one of ~3900 image rows
 * nor of the no-signal noise rows.
 */
#define WEFAX_CONFIRM_BLACK_THRESHOLD  80
#define WEFAX_CONFIRM_BLACK_PERCENT    85

/*
 * Narrow-band START criterion (300 Hz), independent of the gray swing.
 * Real image rows reach at most ~0.03; the tone gives 0.3 and more.
 */
#define WEFAX_APT_START_LINE_MIN     0.12f
#define WEFAX_CONFIRM_BLACK_ROWS        4
/*
 * START state machine, identical for the first fax and for a new fax that
 * starts while another one is being decoded:
 *
 *   SEARCH      two consecutive START windows -> WAIT_BLACK
 *   WAIT_BLACK  further START windows only mean "still inside the same
 *               START" (they refresh the timeout, they reset nothing);
 *               4 black rows -> CONFIRMED;
 *               no START window for this long and no confirmation
 *               -> back to SEARCH.
 */
/*
 * Counted from the LAST START window: the phasing band follows the tone
 * immediately.  Real data: black confirmation came 4.3 s after the tone
 * on the recorded WAV, at most 6.5 s on the saved faxes.  Longer values
 * keep a spurious 300 Hz match alive into an unrelated black band.
 */
#define WEFAX_APT_START_TIMEOUT_SAMPLES (WEFAX_SAMPLE_RATE * 10)

/*
 * Grace period after a verified STOP.  The GUI saves the fax and re-arms
 * AUTO while the 450 Hz STOP tone is still on the air (it is usually
 * detected in its first second).  A degraded STOP tone can be read by the
 * transition counter as 300 Hz, and the 10 s of black that follow would
 * then confirm a ghost fax.  START detection is therefore paused for this
 * long after a reset that follows a STOP.  The next real START cannot
 * arrive earlier: residual STOP tone + 10 s black come first.
 */
#define WEFAX_APT_STOP_HOLDOFF_SAMPLES (WEFAX_SAMPLE_RATE * 8)

/*
 * Automatic horizontal alignment from the phasing band (AUTO only).
 *
 * After FAX CONFIRMED the phasing lines are still on the air: black with a
 * short white pulse at the line start.  Groups of 4 consecutive black rows
 * are averaged column by column and the white pulse is located
 * (circularly, it may wrap around x=0).  When enough groups agree, the
 * line generator phase is shifted once so that the line starts right after
 * the pulse: the RIGHT edge of the white pulse falls on the right border
 * of the image (white bar at the end of every phasing row).  This is the
 * correct framing for GYA Northwood; DWD charts are narrower than the
 * line, so they are complete either way.  If the measurement is not
 * convincing nothing is changed.
 *
 * Calibrated on 3 saved faxes and 2 recordings (2 stations):
 *   pulse width 84..90 px, left-edge scatter between groups <= 7 px,
 *   pulse contrast 180..220 on phasing, <= 53 on noise, <= 83 on image
 *   rows, ~3 on a black band without pulse.  The phasing band can be
 *   interrupted by fading (rows mean 57..129); the image starts with
 *   bright rows (mean >= 141), which is what ends the measurement.
 */
#define WEFAX_PHASE_GROUP_ROWS        4
#define WEFAX_PHASE_SEARCH_PX        64
#define WEFAX_PHASE_MIN_CONTRAST    100
#define WEFAX_PHASE_MIN_WIDTH        30
#define WEFAX_PHASE_MAX_WIDTH       130
#define WEFAX_PHASE_AGREE_PX          8
#define WEFAX_PHASE_NEEDED            5
#define WEFAX_PHASE_LAST              8
#define WEFAX_PHASE_MAX_ROWS         40
#define WEFAX_PHASE_END_BRIGHT_ROWS   3
#define WEFAX_PHASE_BRIGHT_MEAN     135


struct wefax_decoder {

    wefax_mode_t mode;

    uint64_t sample_count;
    uint64_t block_count;


    /*
     * Complex mixer.
     */
    float oscillator_phase;


    /*
     * Complex baseband FIR low-pass.
     */
    float fir_coeff[WEFAX_FIR_TAPS];
    float fir_i[WEFAX_FIR_TAPS];
    float fir_q[WEFAX_FIR_TAPS];

    unsigned int fir_pos;


    /*
     * Previous complex sample for
     * phase discriminator.
     */
    float previous_i;
    float previous_q;

    bool previous_valid;


    /*
     * Grayscale diagnostics.
     */
    uint8_t gray_min;
    uint8_t gray_max;

    double gray_sum;
    uint64_t gray_count;


    /*
     * Tone-frequency diagnostics.
     */
    double tone_sum;
    uint64_t tone_count;

    unsigned int tone_print_samples;

    uint32_t tone_histogram[TONE_HIST_SIZE];


    /*
     * WEFAX line generator.
     */
    double line_position;

    unsigned int current_pixel;

    double pixel_sum;
    unsigned int pixel_samples;

    uint8_t row[WEFAX_WIDTH];

    uint32_t row_count;


    /*
     * Completed-row callback.
     */
    /* AUTO / APT START detector. */
    bool apt_is_white;
    bool apt_level_valid;
    unsigned int apt_window_samples;
    unsigned int apt_transitions;
    unsigned int apt_start_windows;
    unsigned int apt_stop_windows;
    float apt_frequency;
    float apt_stop_frequency;
    float apt_stop_line;
    float apt_start_line;
    uint8_t apt_buf[WEFAX_APT_WINDOW_SAMPLES];
    bool apt_start_detected;
    uint32_t apt_start_generation;
    bool apt_fax_confirmed;
    unsigned int apt_black_rows;
    bool apt_stop_detected;

    /*
     * Confirmed-fax restart detector.  A fresh APT START seen while a fax
     * is already confirmed is only a candidate.  It must be followed by
     * the same four 90%-black half-second bands used by the normal START
     * confirmation before the current fax is discarded.
     */
    bool apt_restart_candidate;
    uint32_t apt_restart_age;
    uint32_t apt_start_age;
    uint32_t apt_start_holdoff;

    /* phasing alignment */
    bool phase_active;
    bool phase_skip_row;
    uint8_t phase_ring[WEFAX_PHASE_GROUP_ROWS][WEFAX_WIDTH];
    unsigned int phase_ring_pos;
    unsigned int phase_black_run;
    unsigned int phase_bright_run;
    unsigned int phase_rows;
    unsigned int phase_groups;
    int phase_left[WEFAX_PHASE_LAST];
    int phase_width[WEFAX_PHASE_LAST];
    unsigned int phase_count;
    unsigned int apt_restart_start_windows;
    unsigned int apt_restart_black_rows;


    wefax_row_callback_t row_callback;
    void *row_callback_data;
};


/*
 * Return a percentile from the current
 * one-second tone histogram.
 */
static float tone_percentile(
    const wefax_decoder_t *decoder,
    float percentile)
{
    uint64_t total = 0;


    for (
        unsigned int i = 0;
        i < TONE_HIST_SIZE;
        i++
    ) {

        total += decoder->tone_histogram[i];
    }


    if (total == 0) {
        return 0.0f;
    }


    uint64_t target =
        (uint64_t)(
            percentile *
            (double)(total - 1)
        );


    uint64_t accumulated = 0;


    for (
        unsigned int i = 0;
        i < TONE_HIST_SIZE;
        i++
    ) {

        accumulated +=
            decoder->tone_histogram[i];


        if (accumulated > target) {

            return
                (float)(
                    TONE_HIST_MIN_HZ + i
                );
        }
    }


    return (float)TONE_HIST_MAX_HZ;
}


/*
 * Clear only the one-second tone
 * diagnostic window.
 */
static void reset_tone_window(
    wefax_decoder_t *decoder)
{
    decoder->tone_sum = 0.0;
    decoder->tone_count = 0;

    decoder->tone_print_samples = 0;


    memset(
        decoder->tone_histogram,
        0,
        sizeof(decoder->tone_histogram)
    );
}


/*
 * Build a unity-DC-gain low-pass FIR using a windowed sinc.
 */
static void init_fir(
    wefax_decoder_t *decoder)
{
    const int middle =
        (WEFAX_FIR_TAPS - 1) / 2;

    const float fc =
        WEFAX_LPF_HZ /
        (float)WEFAX_SAMPLE_RATE;

    float sum = 0.0f;


    for (
        int n = 0;
        n < WEFAX_FIR_TAPS;
        n++
    ) {

        const int k =
            n - middle;

        float sinc;


        if (k == 0) {

            sinc =
                2.0f * fc;
        }
        else {

            sinc =
                sinf(
                    2.0f *
                    WEFAX_PI *
                    fc *
                    (float)k
                ) /
                (
                    WEFAX_PI *
                    (float)k
                );
        }


        const float window =
            0.54f -
            0.46f *
            cosf(
                2.0f *
                WEFAX_PI *
                (float)n /
                (float)(
                    WEFAX_FIR_TAPS - 1
                )
            );


        decoder->fir_coeff[n] =
            sinc * window;

        sum +=
            decoder->fir_coeff[n];
    }


    /*
     * Normalize DC gain to exactly 1.
     */
    if (sum != 0.0f) {

        for (
            int n = 0;
            n < WEFAX_FIR_TAPS;
            n++
        ) {

            decoder->fir_coeff[n] /=
                sum;
        }
    }
}


static void reset_dsp(
    wefax_decoder_t *decoder)
{
    decoder->oscillator_phase = 0.0f;

    memset(
        decoder->fir_i,
        0,
        sizeof(decoder->fir_i)
    );

    memset(
        decoder->fir_q,
        0,
        sizeof(decoder->fir_q)
    );

    decoder->fir_pos = 0;

    decoder->previous_i = 0.0f;
    decoder->previous_q = 0.0f;

    decoder->previous_valid = false;


    decoder->gray_min = 255;
    decoder->gray_max = 0;

    decoder->gray_sum = 0.0;
    decoder->gray_count = 0;


    reset_tone_window(decoder);

    decoder->apt_is_white = false;
    decoder->apt_level_valid = false;
    decoder->apt_window_samples = 0;
    decoder->apt_transitions = 0;
    decoder->apt_start_windows = 0;
    decoder->apt_stop_windows = 0;
    decoder->apt_frequency = 0.0f;
    decoder->apt_stop_frequency = 0.0f;
    decoder->apt_stop_line = 0.0f;
    decoder->apt_start_line = 0.0f;
    decoder->apt_start_detected = false;
    decoder->apt_start_generation = 0;
    decoder->apt_fax_confirmed = false;
    decoder->apt_black_rows = 0;
    decoder->apt_stop_detected = false;
    decoder->apt_restart_candidate = false;
    decoder->apt_restart_age = 0;
    decoder->apt_start_age = 0;
    decoder->apt_start_holdoff = 0;
    decoder->phase_active = false;
    decoder->phase_skip_row = false;
    decoder->phase_ring_pos = 0;
    decoder->phase_black_run = 0;
    decoder->phase_bright_run = 0;
    decoder->phase_rows = 0;
    decoder->phase_groups = 0;
    decoder->phase_count = 0;
    decoder->apt_restart_start_windows = 0;
    decoder->apt_restart_black_rows = 0;


    decoder->line_position = 0.0;

    decoder->current_pixel = 0;

    decoder->pixel_sum = 0.0;
    decoder->pixel_samples = 0;


    memset(
        decoder->row,
        0,
        sizeof(decoder->row)
    );


    decoder->row_count = 0;
}


wefax_decoder_t *wefax_decoder_create(void)
{
    wefax_decoder_t *decoder =
        calloc(
            1,
            sizeof(wefax_decoder_t)
        );


    if (!decoder) {
        return NULL;
    }


    decoder->mode = WEFAX_MODE_IDLE;


    init_fir(decoder);


    reset_dsp(decoder);


    return decoder;
}


void wefax_decoder_destroy(
    wefax_decoder_t *decoder)
{
    if (!decoder) {
        return;
    }


    free(decoder);
}


void wefax_decoder_reset(
    wefax_decoder_t *decoder)
{
    if (!decoder) {
        return;
    }


    /*
     * A reset that follows a detected STOP in AUTO (the GUI re-arm after
     * saving the fax) starts with the STOP grace period.  Any other reset
     * (AUTO/CONT/CLEAR/Save pressed by the user) does not.
     */
    const bool after_stop =
        decoder->mode == WEFAX_MODE_AUTO &&
        decoder->apt_stop_detected;

    decoder->sample_count = 0;
    decoder->block_count = 0;

    reset_dsp(decoder);

    if (after_stop) {
        decoder->apt_start_holdoff = WEFAX_APT_STOP_HOLDOFF_SAMPLES;

        printf(
            "WEFAX AUTO: START detection paused for %u s after STOP\n",
            (unsigned int)(WEFAX_APT_STOP_HOLDOFF_SAMPLES / WEFAX_SAMPLE_RATE)
        );
    }
}


void wefax_decoder_set_mode(
    wefax_decoder_t *decoder,
    wefax_mode_t mode)
{
    if (!decoder) {
        return;
    }


    decoder->mode = mode;


    wefax_decoder_reset(decoder);
}


wefax_mode_t wefax_decoder_get_mode(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return WEFAX_MODE_IDLE;
    }


    return decoder->mode;
}


void wefax_decoder_set_row_callback(
    wefax_decoder_t *decoder,
    wefax_row_callback_t callback,
    void *user_data)
{
    if (!decoder) {
        return;
    }


    decoder->row_callback = callback;
    decoder->row_callback_data = user_data;
}


/*
 * Measure the black<->white toggle rate of one APT window with thresholds
 * adapted to the window itself.  Returns 0 if the gray range is too small.
 */
static float apt_measure_adaptive(
    const uint8_t *buf,
    unsigned int n)
{
    unsigned int hist[256] = { 0 };

    for (unsigned int i = 0; i < n; i++) {
        hist[buf[i]]++;
    }

    const unsigned int t05 = (n * 5U + 99U) / 100U;
    const unsigned int t95 = (n * 95U + 99U) / 100U;
    unsigned int acc = 0;
    int p05 = -1;
    int p95 = 255;

    for (int v = 0; v < 256; v++) {
        acc += hist[v];
        if (p05 < 0 && acc >= t05) {
            p05 = v;
        }
        if (acc >= t95) {
            p95 = v;
            break;
        }
    }

    const int range = p95 - p05;

    if (p05 < 0 || range < WEFAX_APT_STOP_MIN_RANGE) {
        return 0.0f;
    }

    const float mid = 0.5f * (float)(p05 + p95);
    const float hi = mid + 0.25f * (float)range;
    const float lo = mid - 0.25f * (float)range;

    bool high = (float)buf[0] > mid;
    unsigned int transitions = 0;

    for (unsigned int i = 0; i < n; i++) {
        const float v = (float)buf[i];

        if (v > hi && !high) {
            high = true;
            transitions++;
        }
        else if (v < lo && high) {
            high = false;
        }
    }

    return (float)WEFAX_SAMPLE_RATE *
           (float)transitions / (float)n;
}


/*
 * Fraction (0..1) of the window's gray-level power located in the three
 * DFT bins nearest to `hz` (bin width = sample_rate / n: 11025 / 5512,
 * 2.0002 Hz, so the bins are 448..452 Hz and 298..302 Hz).
 */
static float apt_line_fraction(
    const uint8_t *buf,
    unsigned int n,
    float hz)
{
    double mean = 0.0;

    for (unsigned int i = 0; i < n; i++) {
        mean += buf[i];
    }
    mean /= (double)n;

    double energy = 0.0;

    for (unsigned int i = 0; i < n; i++) {
        const double v = (double)buf[i] - mean;
        energy += v * v;
    }

    if (energy < 1.0) {
        return 0.0f;
    }

    const double bin_hz = (double)WEFAX_SAMPLE_RATE / (double)n;
    const int center = (int)(hz / bin_hz + 0.5);
    double line = 0.0;

    for (int k = center - 1; k <= center + 1; k++) {
        const double w = 2.0 * 3.14159265358979323846 *
                         (double)k / (double)n;
        const double coeff = 2.0 * cos(w);
        double s1 = 0.0;
        double s2 = 0.0;

        for (unsigned int i = 0; i < n; i++) {
            const double s0 = ((double)buf[i] - mean) + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        line += s1 * s1 + s2 * s2 - coeff * s1 * s2;
    }

    /* one-sided spectrum: total power = n * energy / 2 */
    return (float)(2.0 * line / ((double)n * energy));
}


/* ------------------------------------------------------------------ */
/* Phasing alignment                                                    */
/* ------------------------------------------------------------------ */

static void phase_begin(
    wefax_decoder_t *decoder)
{
    decoder->phase_active = true;
    decoder->phase_skip_row = false;
    decoder->phase_ring_pos = 0;
    decoder->phase_black_run = 0;
    decoder->phase_bright_run = 0;
    decoder->phase_rows = 0;
    decoder->phase_groups = 0;
    decoder->phase_count = 0;
}


/* signed circular distance a - b in (-W/2, W/2] */
static int phase_circ_diff(
    int a,
    int b)
{
    int d = (a - b) % WEFAX_WIDTH;

    if (d > WEFAX_WIDTH / 2) {
        d -= WEFAX_WIDTH;
    }
    else if (d <= -WEFAX_WIDTH / 2) {
        d += WEFAX_WIDTH;
    }

    return d;
}


/*
 * Locate the white pulse in the column-wise mean of the last group of
 * rows.  Returns true and the pulse left edge / width if the pulse is
 * clear enough.
 */
static bool phase_estimate(
    const wefax_decoder_t *decoder,
    int *left,
    int *width,
    int *contrast_out)
{
    static float mean[WEFAX_WIDTH];
    float total = 0.0f;

    for (unsigned int x = 0; x < WEFAX_WIDTH; x++) {
        unsigned int sum = 0;

        for (unsigned int r = 0; r < WEFAX_PHASE_GROUP_ROWS; r++) {
            sum += decoder->phase_ring[r][x];
        }

        mean[x] = (float)sum / (float)WEFAX_PHASE_GROUP_ROWS;
        total += mean[x];
    }

    /* circular sliding window */
    float win = 0.0f;

    for (unsigned int x = 0; x < WEFAX_PHASE_SEARCH_PX; x++) {
        win += mean[x];
    }

    float best = win;
    unsigned int best_start = 0;

    for (unsigned int start = 1; start < WEFAX_WIDTH; start++) {
        win += mean[(start + WEFAX_PHASE_SEARCH_PX - 1) % WEFAX_WIDTH] -
               mean[start - 1];

        if (win > best) {
            best = win;
            best_start = start;
        }
    }

    const float inside = best / (float)WEFAX_PHASE_SEARCH_PX;
    const float rest = (total - best) /
                       (float)(WEFAX_WIDTH - WEFAX_PHASE_SEARCH_PX);
    const float contrast = inside - rest;

    *contrast_out = (int)(contrast + 0.5f);

    if (contrast < (float)WEFAX_PHASE_MIN_CONTRAST) {
        return false;
    }

    /* pulse edges at half level around the window centre */
    const float half = rest + contrast / 2.0f;
    const int centre = (int)((best_start + WEFAX_PHASE_SEARCH_PX / 2) %
                             WEFAX_WIDTH);

    if (mean[centre] <= half) {
        return false;
    }

    /*
     * Edges are searched from OUTSIDE the pulse inwards (first 3
     * consecutive pixels above half level), so noise dips inside the
     * pulse do not cut it short.
     */
    int l = -150;
    bool found_l = false;

    for (; l <= 0; l++) {
        unsigned int above = 0;

        for (int j = 0; j < 3; j++) {
            if (mean[(centre + l + j + 2 * WEFAX_WIDTH) % WEFAX_WIDTH] > half) {
                above++;
            }
        }

        if (above == 3) {
            found_l = true;
            break;
        }
    }

    int r = 150;
    bool found_r = false;

    for (; r >= 0; r--) {
        unsigned int above = 0;

        for (int j = 0; j < 3; j++) {
            if (mean[(centre + r - j + 2 * WEFAX_WIDTH) % WEFAX_WIDTH] > half) {
                above++;
            }
        }

        if (above == 3) {
            found_r = true;
            break;
        }
    }

    if (!found_l || !found_r) {
        return false;
    }

    *width = r - l + 1;
    *left = (centre + l + 2 * WEFAX_WIDTH) % WEFAX_WIDTH;

    return *width >= WEFAX_PHASE_MIN_WIDTH &&
           *width <= WEFAX_PHASE_MAX_WIDTH;
}


/*
 * Feed one finished row (AUTO, fax confirmed, alignment still pending).
 * Returns the x (current line grid) where the new line must start, i.e.
 * the first pixel after the pulse, or -1
 * while undecided / when giving up.
 */
static int phase_process_row(
    wefax_decoder_t *decoder,
    const uint8_t *row)
{
    if (decoder->phase_skip_row) {
        decoder->phase_skip_row = false;
        return -1;
    }

    decoder->phase_rows++;

    unsigned int black = 0;

    for (unsigned int x = 0; x < WEFAX_WIDTH; x++) {
        if (row[x] < WEFAX_CONFIRM_BLACK_THRESHOLD) {
            black++;
        }
    }

    const bool is_black =
        black * 100U >= WEFAX_WIDTH * WEFAX_CONFIRM_BLACK_PERCENT;

    unsigned int row_sum = 0;

    for (unsigned int x = 0; x < WEFAX_WIDTH; x++) {
        row_sum += row[x];
    }

    if (row_sum >= (unsigned int)WEFAX_PHASE_BRIGHT_MEAN * WEFAX_WIDTH) {
        decoder->phase_bright_run++;
    }
    else {
        decoder->phase_bright_run = 0;
    }

    if (is_black) {
        memcpy(
            decoder->phase_ring[decoder->phase_ring_pos],
            row,
            WEFAX_WIDTH
        );
        decoder->phase_ring_pos =
            (decoder->phase_ring_pos + 1) % WEFAX_PHASE_GROUP_ROWS;
        decoder->phase_black_run++;
    }
    else {
        decoder->phase_black_run = 0;
    }

    if (is_black &&
        decoder->phase_black_run >= WEFAX_PHASE_GROUP_ROWS) {
        int left = 0;
        int width = 0;
        int contrast = 0;
        const bool ok = phase_estimate(decoder, &left, &width, &contrast);

        decoder->phase_groups++;

        if (ok) {
            const unsigned int slot =
                decoder->phase_count % WEFAX_PHASE_LAST;

            decoder->phase_left[slot] = left;
            decoder->phase_width[slot] = width;
            decoder->phase_count++;

            const unsigned int n =
                decoder->phase_count < WEFAX_PHASE_LAST ?
                decoder->phase_count : WEFAX_PHASE_LAST;

            /* the estimate with most neighbours within the tolerance */
            unsigned int best_votes = 0;
            int ref = 0;

            for (unsigned int i = 0; i < n; i++) {
                unsigned int votes = 0;

                for (unsigned int j = 0; j < n; j++) {
                    const int d = phase_circ_diff(
                        decoder->phase_left[j],
                        decoder->phase_left[i]
                    );

                    if (d >= -WEFAX_PHASE_AGREE_PX &&
                        d <= WEFAX_PHASE_AGREE_PX) {
                        votes++;
                    }
                }

                if (votes > best_votes) {
                    best_votes = votes;
                    ref = decoder->phase_left[i];
                }
            }

            if (best_votes >= WEFAX_PHASE_NEEDED) {
                /* median of the agreeing estimates, relative to ref */
                int d[WEFAX_PHASE_LAST];
                int w[WEFAX_PHASE_LAST];
                unsigned int m = 0;

                for (unsigned int j = 0; j < n; j++) {
                    const int dd = phase_circ_diff(
                        decoder->phase_left[j],
                        ref
                    );

                    if (dd >= -WEFAX_PHASE_AGREE_PX &&
                        dd <= WEFAX_PHASE_AGREE_PX) {
                        d[m] = dd;
                        w[m] = decoder->phase_width[j];
                        m++;
                    }
                }

                for (unsigned int i = 1; i < m; i++) {
                    for (unsigned int j = i; j > 0 && d[j - 1] > d[j]; j--) {
                        const int t = d[j]; d[j] = d[j - 1]; d[j - 1] = t;
                    }
                    for (unsigned int j = i; j > 0 && w[j - 1] > w[j]; j--) {
                        const int t = w[j]; w[j] = w[j - 1]; w[j - 1] = t;
                    }
                }

                const int pulse_left =
                    (ref + d[m / 2] + WEFAX_WIDTH) % WEFAX_WIDTH;

                /* first pixel after the pulse: becomes the new x=0 */
                const int pulse_end =
                    (pulse_left + w[m / 2]) % WEFAX_WIDTH;

                decoder->phase_active = false;

                printf(
                    "WEFAX AUTO: PHASING aligned - pulse at x=%d..%d "
                    "(width %d px, %u/%u groups agree), "
                    "white bar moved to the right border\n",
                    pulse_left,
                    (pulse_end + WEFAX_WIDTH - 1) % WEFAX_WIDTH,
                    w[m / 2],
                    best_votes,
                    n
                );

                return pulse_end;
            }
        }
    }

    /* the image has started: never shift the line inside the image */
    const bool band_over =
        decoder->phase_bright_run >= WEFAX_PHASE_END_BRIGHT_ROWS;

    if (band_over || decoder->phase_rows >= WEFAX_PHASE_MAX_ROWS) {
        decoder->phase_active = false;

        printf(
            "WEFAX AUTO: PHASING not reliable (%u good groups) - "
            "alignment unchanged\n",
            decoder->phase_count
        );
    }

    return -1;
}


void wefax_decoder_process(
    wefax_decoder_t *decoder,
    const float *samples,
    unsigned int count)
{
    if (
        !decoder ||
        !samples ||
        count == 0
    ) {

        return;
    }


    if (decoder->mode == WEFAX_MODE_IDLE) {
        return;
    }


    const float phase_step =
        2.0f *
        WEFAX_PI *
        (float)WEFAX_CENTER_HZ /
        (float)WEFAX_SAMPLE_RATE;


    const float deviation_ratio =
        (
            (float)WEFAX_SAMPLE_RATE /
            (float)WEFAX_SHIFT_HZ
        ) /
        (2.0f * WEFAX_PI);


    const double line_step =
        (double)WEFAX_LPM /
        (
            60.0 *
            (double)WEFAX_SAMPLE_RATE
        );


    for (
        unsigned int n = 0;
        n < count;
        n++
    ) {

        const float sample =
            samples[n];


        /*
         * Mix center frequency to complex DC.
         */
        const float c =
            cosf(
                decoder->oscillator_phase
            );

        const float s =
            sinf(
                decoder->oscillator_phase
            );


        const float i =
            sample * c;

        const float q =
            -sample * s;


        decoder->oscillator_phase +=
            phase_step;


        if (
            decoder->oscillator_phase >=
            2.0f * WEFAX_PI
        ) {

            decoder->oscillator_phase -=
                2.0f * WEFAX_PI;
        }


        /*
         * Complex FIR low-pass.
         *
         * Store newest I/Q pair in the circular buffer, then
         * convolve newest -> oldest with the symmetric FIR.
         */
        decoder->fir_i[
            decoder->fir_pos
        ] = i;

        decoder->fir_q[
            decoder->fir_pos
        ] = q;


        float filtered_i = 0.0f;
        float filtered_q = 0.0f;

        unsigned int fir_index =
            decoder->fir_pos;


        for (
            unsigned int tap = 0;
            tap < WEFAX_FIR_TAPS;
            tap++
        ) {

            const float coefficient =
                decoder->fir_coeff[tap];


            filtered_i +=
                coefficient *
                decoder->fir_i[fir_index];

            filtered_q +=
                coefficient *
                decoder->fir_q[fir_index];


            if (fir_index == 0) {

                fir_index =
                    WEFAX_FIR_TAPS - 1;
            }
            else {

                fir_index--;
            }
        }


        decoder->fir_pos++;

        if (
            decoder->fir_pos >=
            WEFAX_FIR_TAPS
        ) {

            decoder->fir_pos = 0;
        }


        if (decoder->previous_valid) {

            /*
             * arg(conj(previous) * current)
             */
            const float real =
                decoder->previous_i *
                filtered_i +
                decoder->previous_q *
                filtered_q;


            const float imag =
                decoder->previous_i *
                filtered_q -
                decoder->previous_q *
                filtered_i;


            const float phase_difference =
                atan2f(
                    imag,
                    real
                );


            /*
             * ------------------------------------------------
             * Actual tone frequency
             * ------------------------------------------------
             */
            const float baseband_hz =
                phase_difference *
                (float)WEFAX_SAMPLE_RATE /
                (2.0f * WEFAX_PI);


            const float tone_hz =
                (float)WEFAX_CENTER_HZ -
                baseband_hz;


            /*
             * ------------------------------------------------
             * Tone diagnostic
             * ------------------------------------------------
             *
             * Store only values inside a sensible
             * WEFAX audio range. This prevents large
             * discriminator glitches from destroying
             * the percentile measurement.
             */
            if (
                tone_hz >=
                    (float)TONE_HIST_MIN_HZ &&
                tone_hz <=
                    (float)TONE_HIST_MAX_HZ
            ) {

                unsigned int bin =
                    (unsigned int)(
                        tone_hz -
                        (float)TONE_HIST_MIN_HZ +
                        0.5f
                    );


                if (bin >= TONE_HIST_SIZE) {
                    bin = TONE_HIST_SIZE - 1;
                }


                decoder->tone_histogram[bin]++;


                decoder->tone_sum +=
                    tone_hz;

                decoder->tone_count++;
            }


            decoder->tone_print_samples++;


            /*
             * Print approximately once per second.
             */
            if (
                decoder->tone_print_samples >=
                WEFAX_SAMPLE_RATE
            ) {

                if (
                    decoder->tone_count > 0
                ) {

                    const double average_tone =
                        decoder->tone_sum /
                        (double)
                            decoder->tone_count;


                    const float p01 =
                        tone_percentile(
                            decoder,
                            0.01f
                        );

                    const float p05 =
                        tone_percentile(
                            decoder,
                            0.05f
                        );

                    const float p50 =
                        tone_percentile(
                            decoder,
                            0.50f
                        );

                    const float p95 =
                        tone_percentile(
                            decoder,
                            0.95f
                        );

                    const float p99 =
                        tone_percentile(
                            decoder,
                            0.99f
                        );


                    /*
                    printf(
                        "WEFAX: tone avg=%.1f "
                        "P01=%.0f P05=%.0f "
                        "P50=%.0f P95=%.0f "
                        "P99=%.0f Hz\n",

                        average_tone,
                        p01,
                        p05,
                        p50,
                        p95,
                        p99
                    );
                    */
                }


                reset_tone_window(
                    decoder
                );
            }


            /*
             * ------------------------------------------------
             * WEFAX grayscale demodulation
             * ------------------------------------------------
             *
             * Polarity corrected for DWD:
             * low tone -> white,
             * high tone -> black.
             */
            float gray_f =
                255.0f *
                (
                    0.5f +
                    deviation_ratio *
                    phase_difference
                );


            if (gray_f < 0.0f) {
                gray_f = 0.0f;
            }


            if (gray_f > 255.0f) {
                gray_f = 255.0f;
            }


            const uint8_t gray =
                (uint8_t)(
                    gray_f + 0.5f
                );


            /*
             * Grayscale diagnostics.
             */
            if (
                gray <
                decoder->gray_min
            ) {

                decoder->gray_min =
                    gray;
            }


            if (
                gray >
                decoder->gray_max
            ) {

                decoder->gray_max =
                    gray;
            }


            decoder->gray_sum +=
                gray;

            decoder->gray_count++;


            /*
             * AUTO: DWD APT detector.
             *
             * Before START we look for two consecutive 0.5 s windows
             * around 300 Hz.  After START has been acquired, the same
             * hysteretic transition counter looks for two consecutive
             * windows around 450 Hz to identify APT STOP.
             *
             * Thresholds and counting follow the fldigi WEFAX approach:
             * white > 215, black < 40, count black->white transitions.
             */
            /* STOP grace period */
            if (
                decoder->mode == WEFAX_MODE_AUTO &&
                decoder->apt_start_holdoff > 0
            ) {
                decoder->apt_start_holdoff--;

                if (decoder->apt_start_holdoff == 0) {
                    printf(
                        "WEFAX AUTO: START detection resumed\n"
                    );
                }
            }

            /* WAIT_BLACK timeout, first fax */
            if (
                decoder->mode == WEFAX_MODE_AUTO &&
                decoder->apt_start_detected &&
                !decoder->apt_fax_confirmed
            ) {
                if (++decoder->apt_start_age >=
                    WEFAX_APT_START_TIMEOUT_SAMPLES) {
                    decoder->apt_start_detected = false;
                    decoder->apt_start_windows = 0;
                    decoder->apt_black_rows = 0;
                    decoder->apt_start_age = 0;

                    printf(
                        "WEFAX AUTO: provisional START expired - "
                        "searching again\n"
                    );
                }
            }

            /* WAIT_BLACK timeout, new fax during a confirmed one */
            if (
                decoder->mode == WEFAX_MODE_AUTO &&
                decoder->apt_fax_confirmed &&
                decoder->apt_restart_candidate
            ) {
                if (++decoder->apt_restart_age >=
                    WEFAX_APT_START_TIMEOUT_SAMPLES) {
                    decoder->apt_restart_candidate = false;
                    decoder->apt_restart_black_rows = 0;
                    decoder->apt_restart_start_windows = 0;
                    decoder->apt_restart_age = 0;

                    printf(
                        "WEFAX AUTO: new START candidate expired\n"
                    );
                }
            }

            if (
                decoder->mode == WEFAX_MODE_AUTO &&
                !decoder->apt_stop_detected
            ) {
                bool transition = false;

                if (!decoder->apt_level_valid) {
                    if (gray > WEFAX_APT_WHITE_THRESHOLD) {
                        decoder->apt_is_white = true;
                        decoder->apt_level_valid = true;
                    }
                    else if (gray < WEFAX_APT_BLACK_THRESHOLD) {
                        decoder->apt_is_white = false;
                        decoder->apt_level_valid = true;
                    }
                }
                else if (
                    !decoder->apt_is_white &&
                    gray > WEFAX_APT_WHITE_THRESHOLD
                ) {
                    decoder->apt_is_white = true;
                    transition = true;
                }
                else if (
                    decoder->apt_is_white &&
                    gray < WEFAX_APT_BLACK_THRESHOLD
                ) {
                    decoder->apt_is_white = false;
                }

                if (transition) {
                    decoder->apt_transitions++;
                }

                decoder->apt_buf[decoder->apt_window_samples] = gray;
                decoder->apt_window_samples++;

                if (
                    decoder->apt_window_samples >=
                    WEFAX_APT_WINDOW_SAMPLES
                ) {
                    decoder->apt_frequency =
                        (float)WEFAX_SAMPLE_RATE *
                        (float)decoder->apt_transitions /
                        (float)decoder->apt_window_samples;

                    decoder->apt_stop_frequency =
                        apt_measure_adaptive(
                            decoder->apt_buf,
                            decoder->apt_window_samples
                        );

                    decoder->apt_stop_line =
                        decoder->apt_fax_confirmed ?
                        apt_line_fraction(
                            decoder->apt_buf,
                            decoder->apt_window_samples,
                            WEFAX_APT_STOP_HZ
                        ) : 0.0f;

                    decoder->apt_start_line =
                        apt_line_fraction(
                            decoder->apt_buf,
                            decoder->apt_window_samples,
                            WEFAX_APT_START_HZ
                        );

                    const bool start_window =
                        decoder->apt_start_holdoff == 0 &&
                        (fabsf(
                            decoder->apt_frequency -
                            WEFAX_APT_START_HZ
                        ) <= WEFAX_APT_START_TOLERANCE ||
                        decoder->apt_start_line >=
                            WEFAX_APT_START_LINE_MIN);

                    /*
                    printf(
                        "WEFAX AUTO: APT %.1f Hz, transitions=%u\n",
                        decoder->apt_frequency,
                        decoder->apt_transitions
                    );
                    */

                    if (
                        !decoder->apt_fax_confirmed &&
                        decoder->apt_start_detected
                    ) {
                        /*
                         * WAIT_BLACK: a START window here is the same START
                         * we already acquired (possibly degraded).  Keep it
                         * alive, reset nothing.
                         */
                        if (start_window) {
                            decoder->apt_start_age = 0;
                        }
                    }
                    else if (!decoder->apt_fax_confirmed) {
                        /* SEARCH */
                        if (start_window) {
                            decoder->apt_start_windows++;

                            /*
                            printf(
                                "WEFAX AUTO: APT START candidate "
                                "%.1f Hz (%u/%u)\n",
                                decoder->apt_frequency,
                                decoder->apt_start_windows,
                                WEFAX_APT_START_WINDOWS
                            );
                            */
                        }
                        else {
                            decoder->apt_start_windows = 0;
                        }

                        if (
                            decoder->apt_start_windows >=
                            WEFAX_APT_START_WINDOWS
                        ) {
                            decoder->apt_start_detected = true;
                            decoder->apt_start_generation++;
                            decoder->apt_start_windows = 0;
                            decoder->apt_start_age = 0;
                            decoder->apt_stop_windows = 0;
                            decoder->apt_black_rows = 0;

                            /*
                             * Start (or restart) provisional image
                             * acquisition exactly at this START.
                             */
                            decoder->line_position = 0.0;
                            decoder->current_pixel = 0;
                            decoder->pixel_sum = 0.0;
                            decoder->pixel_samples = 0;
                            decoder->row_count = 0;

                            memset(
                                decoder->row,
                                0,
                                sizeof(decoder->row)
                            );

                            printf(
                                "WEFAX AUTO: APT START DETECTED - "
                                "provisional fax (%.1f Hz, line=%.3f)\n",
                                decoder->apt_frequency,
                                decoder->apt_start_line
                            );
                        }
                    }
                    else {
                        /*
                         * A confirmed fax keeps looking for STOP as before.
                         * After a short grace period it also watches for a
                         * completely new START.  A 300 Hz match alone never
                         * disturbs the current image: it merely opens a
                         * restart candidate.  The candidate is accepted only
                         * after the same four 90%-black half-second bands
                         * required by the normal initial START.
                         */
                        if (
                            fabsf(
                                decoder->apt_stop_frequency -
                                WEFAX_APT_STOP_HZ
                            ) <= WEFAX_APT_STOP_TOLERANCE ||
                            decoder->apt_stop_line >=
                                WEFAX_APT_STOP_LINE_MIN
                        ) {
                            decoder->apt_stop_windows++;
                        }
                        else {
                            decoder->apt_stop_windows = 0;
                        }

                        if (
                            decoder->apt_stop_windows >=
                            WEFAX_APT_STOP_WINDOWS
                        ) {
                            decoder->apt_stop_detected = true;

                            printf(
                                "WEFAX AUTO: APT STOP DETECTED "
                                "(%.1f Hz, line=%.3f)\n",
                                decoder->apt_stop_frequency,
                                decoder->apt_stop_line
                            );
                        }

                        if (!decoder->apt_stop_detected) {
                            /*
                             * Use the same START detector while a fax is
                             * already confirmed.  The 300 Hz sequence only
                             * opens a restart candidate; it never disturbs
                             * the current fax.  Black confirmation is done
                             * later, on completed WEFAX rows, exactly like
                             * the initial START confirmation.
                             */
                            if (decoder->apt_restart_candidate) {
                                /* WAIT_BLACK: same START, keep it alive */
                                if (start_window) {
                                    decoder->apt_restart_age = 0;
                                }
                            }
                            else if (start_window) {
                                decoder->apt_restart_start_windows++;
                            }
                            else {
                                decoder->apt_restart_start_windows = 0;
                            }

                            if (
                                !decoder->apt_restart_candidate &&
                                decoder->apt_restart_start_windows >=
                                WEFAX_APT_START_WINDOWS
                            ) {
                                decoder->apt_restart_candidate = true;
                                decoder->apt_restart_age = 0;
                                decoder->apt_restart_black_rows = 0;
                                decoder->apt_restart_start_windows = 0;

                                printf(
                                    "WEFAX AUTO: NEW APT START DETECTED - "
                                    "waiting for normal black-row confirmation "
                                    "(%.1f Hz)\n",
                                    decoder->apt_frequency
                                );
                            }
                        }
                    }

                    decoder->apt_window_samples = 0;
                    decoder->apt_transitions = 0;
                }
            }


            /*
             * ------------------------------------------------
             * WEFAX line generation
             * ------------------------------------------------
             *
             * CONT starts immediately.
             * AUTO must remain completely idle visually until APT START
             * has actually been detected.  The demodulator above keeps
             * running so the START detector can do its job.
             */
            if (
                decoder->mode == WEFAX_MODE_AUTO &&
                !decoder->apt_start_detected
            ) {
                /*
                 * The discriminator compares every sample with the
                 * previous one: the reference MUST advance even while
                 * no line is generated.  Skipping this update (as the
                 * bare `continue` did) froze previous_i/q at the first
                 * sample, so before START the gray signal measured the
                 * phase against a fixed phasor instead of the tone
                 * frequency, and the START detector worked on garbage.
                 */
                decoder->previous_i = filtered_i;
                decoder->previous_q = filtered_q;
                decoder->previous_valid = true;
                continue;
            }

            decoder->line_position +=
                line_step;


            bool line_finished =
                false;


            unsigned int pixel =
                (unsigned int)(
                    decoder->line_position *
                    (double)WEFAX_WIDTH
                );


            if (
                pixel >=
                WEFAX_WIDTH
            ) {

                pixel =
                    WEFAX_WIDTH - 1;

                line_finished =
                    true;
            }


            if (
                pixel ==
                decoder->current_pixel
            ) {

                decoder->pixel_sum +=
                    gray;

                decoder->pixel_samples++;
            }
            else {

                if (
                    decoder->pixel_samples > 0 &&
                    decoder->current_pixel <
                        WEFAX_WIDTH
                ) {

                    decoder->row[
                        decoder->current_pixel
                    ] =
                        (uint8_t)(
                            decoder->pixel_sum /
                            decoder->pixel_samples +
                            0.5
                        );
                }


                while (
                    decoder->current_pixel + 1 <
                        pixel &&
                    decoder->current_pixel + 1 <
                        WEFAX_WIDTH
                ) {

                    decoder->current_pixel++;


                    decoder->row[
                        decoder->current_pixel
                    ] = gray;
                }


                decoder->current_pixel =
                    pixel;

                decoder->pixel_sum =
                    gray;

                decoder->pixel_samples =
                    1;
            }


            if (line_finished) {

                if (
                    decoder->pixel_samples > 0
                ) {

                    decoder->row[
                        decoder->current_pixel
                    ] =
                        (uint8_t)(
                            decoder->pixel_sum /
                            decoder->pixel_samples +
                            0.5
                        );
                }


                /*
                 * AUTO provisional START confirmation.
                 *
                 * The DWD fax START sequence is followed by a long black
                 * band.  A completed row is considered black when at least
                 * 90% of its pixels are below gray level 50.  Four such
                 * consecutive rows (2 seconds at 120 LPM) confirm that the
                 * START was real.  Until then the START detector remains
                 * armed and another START can restart acquisition.
                 */
                if (
                    decoder->mode == WEFAX_MODE_AUTO &&
                    decoder->apt_start_detected &&
                    !decoder->apt_fax_confirmed
                ) {
                    unsigned int black_pixels = 0;

                    for (
                        unsigned int x = 0;
                        x < WEFAX_WIDTH;
                        x++
                    ) {
                        if (
                            decoder->row[x] <
                            WEFAX_CONFIRM_BLACK_THRESHOLD
                        ) {
                            black_pixels++;
                        }
                    }

                    if (
                        black_pixels * 100U >=
                        WEFAX_WIDTH * WEFAX_CONFIRM_BLACK_PERCENT
                    ) {
                        decoder->apt_black_rows++;

                        /*
                        printf(
                            "WEFAX AUTO: BLACK %u/%u\n",
                            decoder->apt_black_rows,
                            WEFAX_CONFIRM_BLACK_ROWS
                        );
                        */

                        if (
                            decoder->apt_black_rows >=
                            WEFAX_CONFIRM_BLACK_ROWS
                        ) {
                            decoder->apt_fax_confirmed = true;
                            decoder->apt_start_windows = 0;
                            decoder->apt_stop_windows = 0;
                            decoder->apt_restart_candidate = false;
                            decoder->apt_restart_black_rows = 0;

                            printf(
                                "WEFAX AUTO: FAX CONFIRMED\n"
                            );

                            phase_begin(decoder);
                        }
                    }
                    else {
                        decoder->apt_black_rows = 0;
                    }
                }

                /*
                 * AUTO confirmed-fax restart confirmation.
                 *
                 * This deliberately uses the exact same completed-row
                 * black criterion as the initial START above.  Until four
                 * consecutive black rows are seen, the current fax remains
                 * untouched.  A non-black row only resets the consecutive
                 * black-row count; it does NOT discard the START candidate.
                 */
                if (
                    decoder->mode == WEFAX_MODE_AUTO &&
                    decoder->apt_fax_confirmed &&
                    decoder->apt_restart_candidate
                ) {
                    unsigned int black_pixels = 0;

                    for (
                        unsigned int x = 0;
                        x < WEFAX_WIDTH;
                        x++
                    ) {
                        if (
                            decoder->row[x] <
                            WEFAX_CONFIRM_BLACK_THRESHOLD
                        ) {
                            black_pixels++;
                        }
                    }

                    if (
                        black_pixels * 100U >=
                        WEFAX_WIDTH * WEFAX_CONFIRM_BLACK_PERCENT
                    ) {
                        decoder->apt_restart_black_rows++;

                        if (
                            decoder->apt_restart_black_rows >=
                            WEFAX_CONFIRM_BLACK_ROWS
                        ) {
                            /* 1.0, not 0.0: the common end-of-row code
                             * below subtracts 1.0, and a negative
                             * line_position is converted to unsigned (UB). */
                            decoder->line_position = 1.0;
                            decoder->current_pixel = 0;
                            decoder->pixel_sum = 0.0;
                            decoder->pixel_samples = 0;
                            decoder->row_count = 0;
                            memset(
                                decoder->row,
                                0,
                                sizeof(decoder->row)
                            );

                            decoder->apt_start_generation++;
                            decoder->apt_restart_start_windows = 0;
                            decoder->apt_stop_windows = 0;
                            decoder->apt_restart_candidate = false;
                            decoder->apt_restart_black_rows = 0;

                            printf(
                                "WEFAX AUTO: NEW APT START CONFIRMED - "
                                "discarding current fax and restarting\n"
                            );

                            /* this row was blanked by the restart */
                            phase_begin(decoder);
                            decoder->phase_skip_row = true;
                        }
                    }
                    else {
                        decoder->apt_restart_black_rows = 0;
                    }
                }

                int phase_shift = -1;

                if (
                    decoder->mode == WEFAX_MODE_AUTO &&
                    decoder->apt_fax_confirmed &&
                    decoder->phase_active
                ) {
                    phase_shift = phase_process_row(
                        decoder,
                        decoder->row
                    );
                }

                if (
                    decoder->row_callback
                ) {

                    decoder->row_callback(
                        decoder->row,
                        WEFAX_WIDTH,
                        decoder->row_count,
                        decoder->row_callback_data
                    );
                }


                decoder->row_count++;


                /*
                 * Keep the fractional part.
                 */
                decoder->line_position -=
                    1.0;


                decoder->current_pixel =
                    (unsigned int)(
                        decoder->line_position *
                        (double)WEFAX_WIDTH
                    );


                if (
                    decoder->current_pixel >=
                    WEFAX_WIDTH
                ) {

                    decoder->current_pixel =
                        0;
                }


                decoder->pixel_sum =
                    0.0;

                decoder->pixel_samples =
                    0;


                memset(
                    decoder->row,
                    0,
                    sizeof(decoder->row)
                );

                /*
                 * Phasing alignment: the first pixel after the pulse is at
                 * x=phase_shift in the current line grid.  Start the next
                 * row at x = WIDTH - phase_shift: that (phasing) row ends
                 * exactly after the pulse, and every following row starts
                 * right after it, with the white bar on the right border.
                 */
                if (
                    phase_shift > 1 &&
                    phase_shift < (int)WEFAX_WIDTH - 1
                ) {
                    decoder->line_position +=
                        (double)(WEFAX_WIDTH - phase_shift) /
                        (double)WEFAX_WIDTH;

                    if (decoder->line_position >= 1.0) {
                        decoder->line_position -= 1.0;
                    }

                    decoder->current_pixel =
                        (unsigned int)(
                            decoder->line_position *
                            (double)WEFAX_WIDTH
                        );

                    if (decoder->current_pixel >= WEFAX_WIDTH) {
                        decoder->current_pixel = 0;
                    }
                }
            }
        }


        decoder->previous_i =
            filtered_i;

        decoder->previous_q =
            filtered_q;

        decoder->previous_valid =
            true;
    }


    decoder->sample_count +=
        count;

    decoder->block_count++;
}


uint64_t wefax_decoder_sample_count(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return 0;
    }


    return decoder->sample_count;
}


uint64_t wefax_decoder_block_count(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return 0;
    }


    return decoder->block_count;
}


uint32_t wefax_decoder_row_count(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return 0;
    }


    return decoder->row_count;
}


uint8_t wefax_decoder_gray_min(
    const wefax_decoder_t *decoder)
{
    if (
        !decoder ||
        decoder->gray_count == 0
    ) {

        return 0;
    }


    return decoder->gray_min;
}


uint8_t wefax_decoder_gray_max(
    const wefax_decoder_t *decoder)
{
    if (
        !decoder ||
        decoder->gray_count == 0
    ) {

        return 0;
    }


    return decoder->gray_max;
}


float wefax_decoder_gray_average(
    const wefax_decoder_t *decoder)
{
    if (
        !decoder ||
        decoder->gray_count == 0
    ) {

        return 0.0f;
    }


    return
        (float)(
            decoder->gray_sum /
            (double)
                decoder->gray_count
        );
}


bool wefax_decoder_auto_stop_detected(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_stop_detected : false;
}


uint32_t wefax_decoder_auto_start_generation(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_start_generation : 0;
}


bool wefax_decoder_auto_fax_confirmed(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_fax_confirmed : false;
}
