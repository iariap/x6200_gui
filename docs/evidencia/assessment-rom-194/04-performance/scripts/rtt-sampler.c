/* rtt-sampler.c — local capture helper for #198: observable `get` round-trip time.
 *
 * One connection per sample (the control server answers one line per request).
 * The timer starts immediately before send() and stops after the complete line
 * has been received, so it is the socket round trip a client observes, not key
 * latency and not the radio's CAT latency. Runs inside the private container
 * next to the GUI (QEMU_USER); it only speaks the documented control protocol.
 *
 * Build: gcc -O2 -static -o rtt-sampler rtt-sampler.c
 * Use:  rtt-sampler <socket> <samples|seconds> <interval_ms> <out_csv> <mode>
 *       mode "n" = number of samples, mode "s" = run for that many seconds
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static long long mono_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s socket samples-or-seconds interval_ms out_csv n|s\n", argv[0]);
        return 2;
    }
    const char *sock = argv[1];
    long limit = atol(argv[2]);
    int interval_ms = atoi(argv[3]);
    const char *out = argv[4];
    char mode = argv[5][0];
    FILE *f = fopen(out, "w");
    if (!f) { perror(out); return 1; }
    fprintf(f, "seq,ts_mono_ms,rtt_us,bytes,ok,response\n");

    const char *req = "{\"cmd\":\"get\"}\n";
    long long start = mono_us();
    long seq = 0;
    while (1) {
        if (mode == 'n' && seq >= limit) break;
        if (mode == 's' && (mono_us() - start) > (long long)limit * 1000000LL) break;

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        long long t0 = mono_us();
        int ok = 0;
        long n = -1;
        char buf[8192];
        buf[0] = 0;
        if (fd >= 0) {
            struct sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            snprintf(addr.sun_path, sizeof addr.sun_path, "%s", sock);
            struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
            if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0) {
                if (write(fd, req, strlen(req)) == (ssize_t)strlen(req)) {
                    ssize_t r;
                    size_t used = 0;
                    while (used < sizeof buf - 1 &&
                           (r = read(fd, buf + used, sizeof buf - 1 - used)) > 0) {
                        used += r;
                        if (memchr(buf, '\n', used)) break;
                    }
                    if (used > 0) {
                        buf[used] = 0;
                        n = (long)used;
                        ok = 1;
                    }
                }
            }
            close(fd);
        }
        long long rtt = mono_us() - t0;
        /* First field of the state JSON: keep a compact, comma-free excerpt. */
        char excerpt[161];
        size_t j = 0;
        for (size_t i = 0; buf[i] && j < sizeof excerpt - 1 && i < 300; i++) {
            unsigned char c = (unsigned char)buf[i];
            excerpt[j++] = (c == ',' || c == '"' || c == '\n' || c == '\r') ? ' ' : (char)c;
        }
        excerpt[j] = 0;
        fprintf(f, "%ld,%lld,%lld,%ld,%d,\"%s\"\n", seq, t0 / 1000, rtt, n, ok, excerpt);
        fflush(f);
        seq++;
        struct timespec ts = {.tv_sec = interval_ms / 1000,
                              .tv_nsec = (long)(interval_ms % 1000) * 1000000L};
        nanosleep(&ts, NULL);
    }
    fclose(f);
    fprintf(stderr, "rtt-sampler: %ld samples\n", seq);
    return 0;
}
