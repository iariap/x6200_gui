/* res-sampler.c — local capture helper for #198 (QEMU_USER /proc window sampler).
 *
 * Reads the /proc of the GUI process inside the private container and appends one
 * CSV row per second. It does NOT touch firmware, harness, tests or CI: it only
 * reads kernel counters that a shell loop could read too (kept in C so the
 * monotonic timestamps and the field parsing are one atomic sample).
 *
 * Build: gcc -O2 -static -o res-sampler res-sampler.c
 * Use (inside the container): res-sampler <label> <pid> <seconds> <interval_ms> <out_csv>
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static long long mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int slurp(const char *path, char *buf, size_t n) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t r = read(fd, buf, n - 1);
    close(fd);
    if (r < 0) return -1;
    buf[r] = 0;
    return (int)r;
}

static long long stat_field(const char *stat, int field) {
    /* field is 1-based as in proc(5); comm is field 2 and can contain spaces/parens.
     * Field 3 is the state letter, so the numeric scan starts at field 4. */
    const char *p = strrchr(stat, ')');
    if (!p) return -1;
    p++;
    while (*p == ' ') p++;
    while (*p && *p != ' ') p++;      /* skip field 3 (state) */
    int cur = 3;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        cur++;
        char *end;
        long long v = strtoll(p, &end, 10);
        if (cur == field) return v;
        if (end == p) break;          /* malformed field: never spin */
        p = end;
    }
    return -1;
}

static long long kv_kb(const char *text, const char *key) {
    const char *p = text;
    size_t kl = strlen(key);
    while (p && *p) {
        if (!strncmp(p, key, kl) && p[kl] == ':') {
            return strtoll(p + kl + 1, NULL, 10);
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    return -1;
}

static long long kv_ll(const char *text, const char *key) {
    const char *p = strstr(text, key);
    if (!p) return -1;
    p += strlen(key);
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    return strtoll(p, NULL, 10);
}

static int fd_count(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] != '.') n++;
    }
    closedir(d);
    return n;
}

static long long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

typedef struct { int tid; long long u, s; } thr_t;
static thr_t prev[256];
static int nprev;

/* Per-thread CPU deltas since the previous sample. The radio thread is the one
 * that spins when /dev/ttyS2 has no BASE answering; separating it from the GUI
 * main thread is what keeps the CPU figure interpretable in QEMU_USER. */
static void scan_threads(int pid, long long *main_u, long long *main_s,
                         long long *other_u, long long *other_s,
                         long long *top_tid, long long *top_ticks, int *nthr) {
    char dir[128];
    snprintf(dir, sizeof dir, "/proc/%d/task", pid);
    DIR *d = opendir(dir);
    *main_u = *main_s = *other_u = *other_s = 0;
    *top_tid = -1; *top_ticks = 0; *nthr = 0;
    if (!d) return;
    thr_t cur[256];
    int ncur = 0;
    struct dirent *e;
    while ((e = readdir(d)) && ncur < 256) {
        if (e->d_name[0] == '.') continue;
        int tid = atoi(e->d_name);
        if (tid <= 0) continue;
        char p[160], st[4096];
        snprintf(p, sizeof p, "/proc/%d/task/%d/stat", pid, tid);
        if (slurp(p, st, sizeof st) < 0) continue;
        long long u = stat_field(st, 14), sv = stat_field(st, 15);
        if (u < 0 || sv < 0) continue;
        cur[ncur].tid = tid; cur[ncur].u = u; cur[ncur].s = sv;
        ncur++;
        long long du = 0, ds = 0;
        for (int i = 0; i < nprev; i++) {
            if (prev[i].tid == tid) { du = u - prev[i].u; ds = sv - prev[i].s; break; }
        }
        if (du < 0) du = 0;
        if (ds < 0) ds = 0;
        if (tid == pid) { *main_u += du; *main_s += ds; }
        else { *other_u += du; *other_s += ds; }
        if (du + ds > *top_ticks) { *top_ticks = du + ds; *top_tid = tid; }
    }
    closedir(d);
    memcpy(prev, cur, sizeof(thr_t) * (size_t)ncur);
    nprev = ncur;
    *nthr = ncur;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s label pid seconds interval_ms out_csv\n", argv[0]);
        return 2;
    }
    const char *label = argv[1];
    int pid = atoi(argv[2]);
    int seconds = atoi(argv[3]);
    int interval_ms = atoi(argv[4]);
    const char *out = argv[5];
    long clk = sysconf(_SC_CLK_TCK);

    char path[128];
    snprintf(path, sizeof path, "/proc/%d/stat", pid);

    FILE *f = fopen(out, "w");
    if (!f) { perror(out); return 1; }
    fprintf(f, "label,ts_mono_ms,pid_ns,pid_host,utime_ticks,stime_ticks,clk_tck,threads,"
               "rss_kb,pss_kb,vmhwm_kb,fds,vol_ctxt,nonvol_ctxt,rchar,wchar,syscr,syscw,"
               "read_bytes,write_bytes,log_bytes,gui_log_bytes,"
               "main_utime_ticks,main_stime_ticks,other_utime_ticks,other_stime_ticks,"
               "top_tid,top_ticks,tasks\n");
    fflush(f);

    char stat[4096], status[8192], io[4096], smaps[16384];
    long long start = mono_ms();
    int samples = 0;
    while (1) {
        long long now = mono_ms();
        if (now - start > (long long)seconds * 1000LL) break;
        stat[0] = status[0] = io[0] = smaps[0] = 0;
        if (slurp(path, stat, sizeof stat) < 0) {
            fprintf(stderr, "cannot read %s: %s\n", path, strerror(errno));
            break;
        }
        snprintf(path, sizeof path, "/proc/%d/status", pid);
        slurp(path, status, sizeof status);
        snprintf(path, sizeof path, "/proc/%d/io", pid);
        slurp(path, io, sizeof io);
        snprintf(path, sizeof path, "/proc/%d/smaps_rollup", pid);
        slurp(path, smaps, sizeof smaps);
        snprintf(path, sizeof path, "/proc/%d/fd", pid);
        int fds = fd_count(path);

        long long utime = stat_field(stat, 14), stime = stat_field(stat, 15);
        long long threads = stat_field(stat, 20);
        long long rss_pages = stat_field(stat, 24);
        long rss_kb = rss_pages > 0 ? rss_pages * (sysconf(_SC_PAGESIZE) / 1024) : -1;
        long long pss = kv_kb(smaps, "Pss");
        long long vmhwm = kv_kb(status, "VmHWM");
        long long main_u, main_s, other_u, other_s, top_tid = -1, top_ticks = 0;
        int tasks = 0;
        scan_threads(pid, &main_u, &main_s, &other_u, &other_s, &top_tid, &top_ticks, &tasks);
        long long vol = kv_ll(status, "voluntary_ctxt_switches");
        long long nonvol = kv_ll(status, "nonvoluntary_ctxt_switches");
        long long ns_pid = -1;
        {
            const char *p = strstr(status, "NSpid:");
            if (p) {
                const char *q = p + 6;
                while (*q == ' ' || *q == '\t') q++;
                ns_pid = strtoll(q, NULL, 10);
            }
        }
        fprintf(f, "\"%s\",%lld,%lld,%d,%lld,%lld,%ld,%lld,%ld,%lld,%lld,%d,%lld,%lld,"
                   "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%d\n",
                label, now, ns_pid, pid, utime, stime, clk, threads, rss_kb, pss, vmhwm, fds,
                vol, nonvol, kv_ll(io, "rchar"), kv_ll(io, "wchar"), kv_ll(io, "syscr"),
                kv_ll(io, "syscw"), kv_ll(io, "read_bytes"), kv_ll(io, "write_bytes"),
                file_size("/t/out-banco.txt"), file_size("/t/tmp/x6200_log.txt"),
                main_u, main_s, other_u, other_s, top_tid, top_ticks, tasks);
        fflush(f);
        samples++;
        snprintf(path, sizeof path, "/proc/%d/stat", pid);
        struct timespec ts = {.tv_sec = interval_ms / 1000,
                              .tv_nsec = (long)(interval_ms % 1000) * 1000000L};
        nanosleep(&ts, NULL);
    }
    fclose(f);
    fprintf(stderr, "res-sampler: %d samples for %s\n", samples, label);
    return 0;
}
