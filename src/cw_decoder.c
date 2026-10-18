/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

/* Based on idea Michael A. Maynard, a.k.a. "K4ICY" */

#include <math.h>
#include <stdint.h>
#include "lvgl/lvgl.h"
#include "cw_decoder.h"
#include "remote_server.h"
#include "pannel.h"

#define HIST_SIZE       10

static void (*sink_text)(const char *);
static void (*sink_wpm)(int);

static uint32_t debounce_factor = 15;
static uint32_t thr_mean = 139;

static uint32_t time_track = 0;

static int32_t  key_line_event_prev = 0;
static int32_t  key_line_event_new = 0;
static uint32_t key_line_ref = 0;

static uint32_t event_hist_index = 0;
static uint32_t short_event_hist[HIST_SIZE];
static uint32_t long_event_hist[HIST_SIZE];
static uint32_t space_event_hist[HIST_SIZE];

static uint64_t long_event_avr = 0;
static uint64_t short_event_avr = 0;
static uint64_t space_event_avr = 0;

static uint16_t wpm_old;
static uint32_t wpm;

static uint32_t space_duration = 0;
static uint32_t space_duration_prev = 0;
static uint32_t space_duration_ref;

static uint32_t word_space_duration = 0;
static uint32_t word_space_duration_ref = 0;
static float    word_space_timing = 3.0f;

static bool     key_line = false;

static float    compare_factor = 2.0;

static bool     character_step = false;
static bool     word_step = false;
static char     elements[128];

/* The longest Morse in cw_characters is 9 elements ("...---...", <SOS>), so a run
 * anywhere near this bound is not a character: it is a stuck signal or corrupted
 * audio. The append is sheared at the buffer bound (decode what came in and start
 * over) so `elements` cannot overrun. Without this, a run of key events with no
 * character boundary grows `elements` past 128 and glibc fortify aborts the
 * process inside strcat (issue #51; the CW Trainer keeps the decoder fed). */
_Static_assert(sizeof(elements) >= 16, "elements must hold the longest Morse plus the NUL");

cw_characters_t cw_characters[] = {
    { .morse = ".-",        .character = "A" },
    { .morse = "-...",      .character = "B" },
    { .morse = "-.-.",      .character = "C" },
    { .morse = "-..",       .character = "D" },
    { .morse = ".",         .character = "E" },
    { .morse = "..-.",      .character = "F" },
    { .morse = "--.",       .character = "G" },
    { .morse = "....",      .character = "H" },
    { .morse = "..",        .character = "I" },
    { .morse = ".---",      .character = "J" },
    { .morse = "-.-",       .character = "K" },
    { .morse = ".-..",      .character = "L" },
    { .morse = "--",        .character = "M" },
    { .morse = "-.",        .character = "N" },
    { .morse = "---",       .character = "O" },
    { .morse = ".--.",      .character = "P" },
    { .morse = "--.-",      .character = "Q" },
    { .morse = ".-.",       .character = "R" },
    { .morse = "...",       .character = "S" },
    { .morse = "-",         .character = "T" },
    { .morse = "..-",       .character = "U" },
    { .morse = "...-",      .character = "V" },
    { .morse = ".--",       .character = "W" },
    { .morse = "-..-",      .character = "X" },
    { .morse = "-.--",      .character = "Y" },
    { .morse = "--..",      .character = "Z" },

    { .morse = ".----",     .character = "1" },
    { .morse = "..---",     .character = "2" },
    { .morse = "...--",     .character = "3" },
    { .morse = "....-",     .character = "4" },
    { .morse = ".....",     .character = "5" },
    { .morse = "-....",     .character = "6" },
    { .morse = "--...",     .character = "7" },
    { .morse = "---..",     .character = "8" },
    { .morse = "----.",     .character = "9" },
    { .morse = "-----",     .character = "0" },

    { .morse = "-.-.--",    .character = "!" },
    { .morse = "..--.",     .character = "!" },
    { .morse = ".-..-.",    .character = "\"" },
    { .morse = "...-..-",   .character = "$" },
    { .morse = ".----.",    .character = "'" },
    { .morse = "--..--",    .character = "," },
    { .morse = "-....-",    .character = "-" },
    { .morse = ".-.-.-",    .character = "." },
    { .morse = "-..-.",     .character = "/" },
    { .morse = "---...",    .character = ":" },
    { .morse = "-.-.-.",    .character = ";" },
    { .morse = "-...-",     .character = "=" },
    { .morse = "..--..",    .character = "?" },
    { .morse = ".--.-.",    .character = "@" },
    { .morse = "..--.-",    .character = "_" },

    { .morse = ".-.-.",     .character = "<AR>" },
    { .morse = ".-...",     .character = "<AS>" },
    { .morse = "-...-.-",   .character = "<BK>" },
    { .morse = "-.-..-..",  .character = "<CL>" },
    { .morse = "-.-.-",     .character = "<CT>" },
    { .morse = "-.--.",     .character = "<KN>" },
    { .morse = "...-.-",    .character = "<SK>" },
    { .morse = "...-.",     .character = "<SN>" },
    { .morse = "...---...", .character = "<SOS>" },
    { .morse = "-.-.--.-",  .character = "<CQ>" },

    { .morse = "......",    .character = "<ERR>" },
    { .morse = ".......",   .character = "<ERR>" },
    { .morse = "........",  .character = "<ERR>" },
    { .morse = NULL }
};

void cw_decoder_init() {
}

static void cw_decoder_ans(const char *ans) {
    if (sink_text) {
        sink_text(ans);
    } else {
        pannel_add_text(ans);
        remote_text("cw_text", ans, strlen(ans));
    }
}

static void cw_decoder_wpm(uint16_t wpm) {
}

static void cw_decoder_dict() {
    cw_characters_t *character = &cw_characters[0];

    while (character->morse) {
        if (strcmp(elements, character->morse) == 0) {
            cw_decoder_ans(character->character);
            return;
        }

        character++;
    }

    cw_decoder_ans("<?>");
}

/* Append one element (dot/dash) to the character being collected, bounded to the
 * buffer: a full buffer is treated as a character boundary (see the static
 * assert on `elements`). */
static void cw_decoder_element(char e) {
    size_t n = strlen(elements);

    if (n + 1 >= sizeof(elements)) {
        cw_decoder_dict();
        n = 0;
    }

    elements[n]     = e;
    elements[n + 1] = 0;
}

static void cw_decoder_calc_wpm() {
    wpm_old = wpm;
    wpm = (6000 * 1.06) / (long_event_avr + short_event_avr + space_event_avr);

    if (wpm != wpm_old) {
        cw_decoder_wpm(wpm);
    }
}

static void cw_decoder_dot_dash(uint16_t short_event, uint16_t long_event) {
    /* Find out which one is the Dot and which is the Dash and roll them into a moving average of each */

    long_event_hist[event_hist_index] = long_event;
    short_event_hist[event_hist_index] = short_event;

    /* Keep a moving average of the intra-element space duration */

    space_event_hist[event_hist_index] = space_duration_prev;

    /* Keep a moving averages */

    long_event_avr = 0;
    short_event_avr = 0;
    space_event_avr = 0;

    for (uint8_t i = 0; i < HIST_SIZE; i++) {
        long_event_avr += long_event_hist[i];
        short_event_avr += short_event_hist[i];
        space_event_avr += space_event_hist[i];
    }

    long_event_avr /= HIST_SIZE;
    short_event_avr /= HIST_SIZE;
    space_event_avr /= HIST_SIZE;

    /* Find threshold mean */

    thr_mean = sqrt(short_event_avr * long_event_avr);

    /* Bootstrap threshold values - - - If any are below or above known Dot/Dash pair ranges then move them instantly */

    if (thr_mean < short_event_hist[event_hist_index] || thr_mean > long_event_hist[event_hist_index]) {
        thr_mean = sqrt(short_event_hist[event_hist_index] * long_event_hist[event_hist_index]);

        long_event_avr = long_event_hist[event_hist_index];
        short_event_avr = short_event_hist[event_hist_index];

        for (uint8_t i = 0; i < HIST_SIZE; i++) {
            long_event_hist[i] = long_event_avr;
            short_event_hist[i] = short_event_avr;
        }
    }

    event_hist_index++;

    if (event_hist_index > HIST_SIZE - 1)
        event_hist_index = 0;

    cw_decoder_calc_wpm();
}


static void cw_decoder_inner_space() {
    space_duration_prev = space_duration;
    space_duration = time_track - space_duration_ref;

    /* DECODE collected string of elements */

    /* check to see if inter-element space duration threshold has been exceeded - then decode   */
    /* it is assumed that the intra-space is longer than a Dot but shorter than a Dash          */

    if (space_duration >= thr_mean) {
        space_duration_ref = time_track;

        if (character_step) {
            cw_decoder_dict();
            elements[0] = 0;

            character_step = false;
        }
    }
}

static void cw_decoder_word_space() {
    word_space_duration = time_track - word_space_duration_ref;

    if (word_space_duration >= thr_mean * word_space_timing) {
        word_space_duration_ref = time_track;

        if (word_step) {
            cw_decoder_ans(" ");
            word_step = false;
        }
    }
}

void cw_decoder_signal(bool on, float ms) {
    time_track += (ms + 0.5f);

    /* Key down */

    if (on) {
        if (!key_line) {
            key_line_ref = time_track;
            word_space_duration_ref = time_track;

            key_line = true;
        }
    }

    /* Key up */

    if (!on) {
        if (time_track - key_line_ref < debounce_factor) {
            key_line = false;
            return;
        }

        if (key_line) {
            key_line = false;
            key_line_event_prev = key_line_event_new;
            key_line_event_new = time_track - key_line_ref;

            /* If the Current Duration Event Compared to the Previous Event appears to be a Dot / Dash pair [ roughly (>2):1 ] */

            if (key_line_event_new >= key_line_event_prev * compare_factor && space_duration_prev <= key_line_event_prev * compare_factor) {
                cw_decoder_dot_dash(key_line_event_new, key_line_event_prev);
            } else if (key_line_event_prev >= key_line_event_new * compare_factor && space_duration_prev <= key_line_event_new * compare_factor) {
                cw_decoder_dot_dash(key_line_event_prev, key_line_event_new);
            }

            /* Reset space durations */

            space_duration_ref = time_track;
            word_space_duration_ref = time_track;

            /* Classify and add most likely Dots or Dashes to a string for eventual character decoding */

            cw_decoder_element((uint32_t) key_line_event_new <= thr_mean ? '.' : '-');

            character_step = true;
            word_step = true;
        }

        cw_decoder_inner_space();
        cw_decoder_word_space();
    }
}

int cw_decoder_get_wpm(void) {
    /* `wpm` es uint32_t y sale de 6360/suma: sin un par punto/raya todavia vale
     * 0. Se satura a int para que el llamador no vea un negativo por cast. */
    if (wpm > INT32_MAX) {
        return INT32_MAX;
    }
    return (int) wpm;
}

void cw_decoder_set_sink(void (*text)(const char *), void (*wpm)(int)) {
    sink_text = text;
    sink_wpm  = wpm;
}
