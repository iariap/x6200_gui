/*
 * NAVTEX RX check on the host (ported from the X6100 firmware by IU1WXY,
 * tools/navtex_loopback.cpp): a SITOR-B FEC transmission made here (CCIR 476 codes from the
 * decoder's own table, every character twice 5 characters apart, alpha / rep phasing before and
 * after, 100 baud FSK +-85 Hz around 1000 Hz, noise added), decoded by src/navtex through the C API the
 * NavTex app uses (navtex_decoder_create(0), 11025 Hz). Checks the message comes back.
 *
 *   tests/native/run_navtex_tests.sh
 *
 * Exit status 0 when it passes at every SNR tried.
 */

#include "decoder.h"
#include "navtex_rx.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static const int    RATE    = 11025;
static const double BAUD    = 100.0;
static const double CENTER  = 1000.0, SHIFT = 85.0;
static const int    ALPHA   = 0x0f, REP = 0x66; /* phasing codes (navtex_rx.cpp code_alpha, code_rep) */
static const char  *MESSAGE = "ZCZC FA01\r\nTEST MESSAGE FROM X6200 ROM 123 456\r\nNNNN\r\n";

static std::string got;

static void on_char(int c, void *) { got.push_back((char)c); }

static double gauss() {
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = (rand() + 1.0) / (RAND_MAX + 2.0);
    return sqrt(-2 * log(u)) * cos(2 * M_PI * v);
}

/* The FEC slots: char k in the rep phase (even slot 2k) and again in the alpha phase 5 slots later */
static std::vector<int> fec_slots(const std::vector<int> &codes, int phasing) {
    int              n = (int)codes.size(), total = 2 * (n + 2 * phasing) + 5;
    std::vector<int> slots(total);
    for (int s = 0; s < total; s++) {
        int k = s % 2 == 0 ? s / 2 - phasing : (s - 5) / 2 - phasing;
        bool in = s % 2 == 0 ? true : s >= 5;
        slots[s] = in && k >= 0 && k < n ? codes[k] : s % 2 == 0 ? REP : ALPHA;
    }
    return slots;
}

static bool run(double snr_db, bool one_is_mark) {
    CCIR476          ccir;
    std::string      enc;
    bool             figs = false;
    for (const char *p = MESSAGE; *p; p++) ccir.char_to_code(enc, *p, figs);
    std::vector<int> codes(enc.begin(), enc.end());
    std::vector<int> slots = fec_slots(codes, 20);

    /* continuous-phase FSK, LSB of each code first (CCIR476::bytes_to_code) */
    std::vector<float> audio;
    double             phase = 0, t_bit = 0, spb = RATE / BAUD;
    audio.insert(audio.end(), RATE, 0.0f);
    for (int code : slots)
        for (int b = 0; b < 7; b++) {
            bool   one = (code >> b) & 1;
            double f   = CENTER + ((one == one_is_mark) ? SHIFT : -SHIFT);
            for (t_bit += spb; t_bit >= 1; t_bit -= 1) {
                audio.push_back((float)(0.5 * sin(phase)));
                phase += 2 * M_PI * f / RATE;
            }
        }
    audio.insert(audio.end(), RATE, 0.0f);
    /* noise: SNR in the 500 Hz NAVTEX channel; signal power 0.125 */
    double nz = sqrt(0.125 / pow(10, snr_db / 10) * (RATE / 2.0) / 500.0);
    for (float &v : audio) v += (float)(nz * gauss());

    got.clear();
    navtex_decoder_t *d = navtex_decoder_create(0);
    navtex_decoder_set_char_callback(d, on_char, nullptr);
    for (size_t i = 0; i < audio.size(); i += 512)
        navtex_decoder_process(d, &audio[i], (unsigned)std::min<size_t>(512, audio.size() - i));
    navtex_decoder_destroy(d);

    bool ok = got.find("TEST MESSAGE FROM X6200 ROM 123 456") != std::string::npos;
    std::string shown = got;
    for (char &c : shown)
        if (c == '\r' || c == '\n') c = '|';
    printf("SNR %+5.1f dB, 1 = %s: %s  \"%s\"\n", snr_db, one_is_mark ? "mark" : "space", ok ? "OK  " : "FAIL",
           shown.c_str());
    return ok;
}

int main() {
    srand(5);
    /* which polarity the decoder takes as normal (reverse off) */
    bool mark = run(20, true), space = run(20, false);
    if (mark == space) {
        printf("polarity: %s\n", mark ? "both decode?" : "neither decodes");
        return 1;
    }
    printf("polarity: a 1 bit is the %s (%s frequency)\n", mark ? "mark" : "space", mark ? "higher" : "lower");
    int fails = 0;
    for (double snr : {10.0, 5.0}) fails += !run(snr, mark);
    run(0.0, mark); /* SITOR-B's edge: a few wrong characters, shown only */
    printf(fails ? "%d FAILED\n" : "all OK\n", fails);
    return fails != 0;
}
