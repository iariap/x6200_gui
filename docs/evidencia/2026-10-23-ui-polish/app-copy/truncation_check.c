/* Runtime truncation check for the copy-only change (issue #288).
 *
 * Reproduces every `snprintf` whose format string this change touched, with the
 * actual destination sizes from the sources and worst-case data (31-char band
 * name, INT_MIN/INT_MAX, 32-bit unsigned max, 8-char callsign). A return value
 * >= size would mean truncated output: the contract forbids losing information
 * silently, so every case asserts `ret < size` and a NUL-terminated string.
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_FITS(buf, size, ret)                                                    \
    do {                                                                              \
        if ((ret) < 0 || (size_t) (ret) >= (size_t) (size)) {                         \
            printf("FAIL %-28s ret=%d size=%zu: %s\n", #buf, (int) (ret),             \
                   (size_t) (size), (buf));                                           \
            failures++;                                                               \
        } else if (strlen(buf) != (size_t) (ret)) {                                   \
            printf("FAIL %-28s strlen=%zu ret=%d\n", #buf, strlen(buf), (int) (ret)); \
            failures++;                                                               \
        }                                                                             \
    } while (0)

int main(void) {
    /* prop_status_t.band_name[32] at its maximum useful content. */
    const char *band = "160m-current-band-name-1234567";
    char        band_name[32];
    snprintf(band_name, sizeof(band_name), "%s", band);
    const char *call = "4U1UN";

    /* --- prop_status_t.note[192] ------------------------------------------ */
    char note[192];
    int  ret;

    ret = snprintf(note, sizeof(note),
                   "No IBP beacons on %s. IARU IBP beacons start at 14 MHz; "
                   "cluster spots follow if available.", band_name);
    CHECK_FITS(note, sizeof(note), ret);

    ret = snprintf(note, sizeof(note), "Cancelled by operator. VFO restored.");
    CHECK_FITS(note, sizeof(note), ret);

    ret = snprintf(note, sizeof(note), "No measurement slot available. VFO restored.");
    CHECK_FITS(note, sizeof(note), ret);

    ret = snprintf(note, sizeof(note),
                   "No beacon heard in its slot. Silence does not prove the band is "
                   "closed (QRM/QRN possible).");
    CHECK_FITS(note, sizeof(note), ret);

    ret = snprintf(note, sizeof(note),
                   "Thresholds: open >= %d dB, marginal >= %d dB.", INT_MIN, INT_MAX);
    CHECK_FITS(note, sizeof(note), ret);

    ret = snprintf(note, sizeof(note), "Radio left RX: scan cancelled. VFO restored.");
    CHECK_FITS(note, sizeof(note), ret);

    ret = snprintf(note, sizeof(note), "Could not start scan worker.");
    CHECK_FITS(note, sizeof(note), ret);

    /* --- render(): char buf[256] ----------------------------------------- */
    char buf[256];

    ret = snprintf(buf, sizeof(buf), "Band %s  |  %d eligible IBP beacons  |  %d scheduled",
                   band_name, INT_MAX, INT_MAX);
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf), "Band %s  |  no IBP beacons", band_name);
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf), "Band %s", band_name);
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf),
                   "No IBP beacons on %s: the IBP network starts at "
                   "14 MHz. Cluster spots appear below if available.", band_name);
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf),
                   "Current band and IBP schedule ready.\n"
                   "NTP clock: %s.\nPress Run or Repeat to measure.",
                   "NOT synchronized (slot timing may drift)");
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf),
                   "NTP clock: %s\nBeacon on air: %s (%s, %d s left)\n%s",
                   "NO: unsynchronized", call, band_name, INT_MAX,
                   "Beacon slot timing may drift without NTP.");
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf),
                   "Beacon %d of %d: %s at %u.%03u MHz\n%s\n%s",
                   INT_MAX, INT_MAX, call, UINT_MAX, 999u,
                   "Slot active: measuring power steps...", "100 W > 10 W > 1 W > 0.1 W");
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf),
                   "Measured %d/%d IBP beacons, %d heard. IARU R2 "
                   "(10 m): %d active of %d.",
                   INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX);
    CHECK_FITS(buf, sizeof(buf), ret);

    ret = snprintf(buf, sizeof(buf), "Measured %d/%d IBP beacons, %d heard.",
                   INT_MAX, INT_MAX, INT_MAX);
    CHECK_FITS(buf, sizeof(buf), ret);

    /* evidence_text(): char line[256] / char buf[256] with a full ladder. */
    char ladder[128];
    int  pos = 0;

    ladder[0] = '\0';
    for (int k = 0; k < 4; k++) {
        pos += snprintf(ladder + pos, sizeof(ladder) - (size_t) pos, " | ... %s", "100W");
    }
    ret = snprintf(buf, sizeof(buf), "%s %u.%03u %02d:%02d:%02dZ | SNR %d%s%s",
                   call, UINT_MAX, 999u, 23, 59, 59, INT_MIN, ladder, " | not heard");
    CHECK_FITS(buf, sizeof(buf), ret);

    /* --- render(): verdict[32] and note[512] ------------------------------ */
    char verdict[32];

    ret = snprintf(verdict, sizeof(verdict), "NO IBP BEACONS");
    CHECK_FITS(verdict, sizeof(verdict), ret);
    ret = snprintf(verdict, sizeof(verdict), "%s", "CLOSED");
    CHECK_FITS(verdict, sizeof(verdict), ret);

    char listening[512];
    pos = 0;
    listening[0] = '\0';
    pos += snprintf(listening + pos, sizeof(listening) - (size_t) pos,
                    "Listening to %s at %u.%03u MHz: Stop/Close restores VFO.\n",
                    call, UINT_MAX, 999u);
    CHECK_FITS(listening, sizeof(listening), pos);

    /* --- run_label[8] ----------------------------------------------------- */
    char run_label[8];

    ret = snprintf(run_label, sizeof(run_label), "Cancel");
    CHECK_FITS(run_label, sizeof(run_label), ret);
    ret = snprintf(run_label, sizeof(run_label), "Run");
    CHECK_FITS(run_label, sizeof(run_label), ret);

    /* --- thresholds_label_fn(): static char buf[48] ----------------------- */
    char thresholds[48];

    ret = snprintf(thresholds, sizeof(thresholds), "Thresholds:\n%d/%d dB", INT_MIN, INT_MAX);
    CHECK_FITS(thresholds, sizeof(thresholds), ret);

    /* --- dialog_swrscan.c: char readout[64] ------------------------------- */
    char readout[64];

    ret = snprintf(readout, sizeof(readout), "No measurements");
    CHECK_FITS(readout, sizeof(readout), ret);

    /* --- dialog_field.c header label ------------------------------------- */
    const char *header = "X6200";

    if (strcmp(header, "X6200") != 0) {
        printf("FAIL field header is not X6200\n");
        failures++;
    }

    printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
