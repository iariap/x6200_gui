# Reproducible host build environment for the UI harness (and the ROM's host
# tests). It is buildable from scratch on any machine or CI: a plain Debian base
# plus the host build dependencies. It does NOT depend on the local Buildroot
# image (which is 6.7 GB and cannot be rebuilt in CI).
#
#   docker build -t x6200-uiharness tools/ui_harness                # full (default)
#   docker build -t x6200-uiharness-ci --target ci tools/ui_harness # minimal (CI)
#
# The harness itself needs only libpng and libnm (through pkg-config); the
# signal generator (tools/ui_harness/src/signal_gen.c) is plain C and does NOT
# link FFTW. The `full` stage adds what the ROM's native test suite and the JS8
# suite use (fftw/sndfile/glib/sqlite3, liquid-dsp, Catch2, Boost) so the same
# image can run tests/run_all_tests.sh without the Buildroot image.

# --- ci: minimal image the harness and the Python framework need ------------
# The harness links only libpng and libnm, but the firmware headers it compiles
# against pull in <sqlite3.h> (src/cfg/cfg.h), <liquid/liquid.h>
# (src/dsp.h, src/util.h) and <speex/speex_preprocess.h>
# (src/audio_enhancement.h, which src/dialog_settings.c includes), so
# libsqlite3-dev, libliquid-dev and libspeexdsp-dev are required to build it.
# The signal generator is plain C and does NOT use FFTW, and the harness never
# links speex: only the headers are needed so the real sources compile.
FROM ubuntu:24.04 AS ci
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
    && apt-get install -y -qq --no-install-recommends \
        build-essential cmake git pkg-config ca-certificates \
        libpng-dev libnm-dev libsqlite3-dev libliquid-dev libspeexdsp-dev \
    && rm -rf /var/lib/apt/lists/*

# --- full: adds the extra host dependencies of the ROM's test suites --------
FROM ci AS full
RUN apt-get update -qq \
    && apt-get install -y -qq --no-install-recommends \
        libfftw3-dev libsndfile1-dev libglib2.0-dev sqlite3 \
        autoconf automake libtool libboost-dev \
    && rm -rf /var/lib/apt/lists/*

# liquid-dsp 1.4.0: the WSPR baseband recorder (NCO + resampler) and the
# waterfall use it, so the ROM host tests need the real library, not a stub.
# Parallelism is capped instead of `nproc`: on a many-core host the concurrent
# compiler peaks are what drove the build (and the host) into OOM. Override with
# `--build-arg JOBS=<n>`.
ARG JOBS=8
RUN git clone -q --depth 1 --branch v1.4.0 https://github.com/jgaeddert/liquid-dsp /tmp/liquid-dsp \
    && cd /tmp/liquid-dsp && ./bootstrap.sh >/dev/null && ./configure --prefix=/usr/local >/dev/null \
    && make -j"${JOBS}" >/dev/null && make install >/dev/null && ldconfig \
    && rm -rf /tmp/liquid-dsp

# Catch2 and Boost headers: the X6100's JS8 test suite (tests/test_js8.cpp) is
# built against them, and js8core itself needs Boost.
RUN git clone -q --depth 1 --branch v3.5.2 https://github.com/catchorg/Catch2 /tmp/Catch2 \
    && cmake -S /tmp/Catch2 -B /tmp/Catch2/build -DCMAKE_BUILD_TYPE=Release \
         -DCATCH_BUILD_TESTING=OFF -DCATCH_INSTALL_DOCS=OFF >/dev/null \
    && cmake --build /tmp/Catch2/build --target install -j"${JOBS}" >/dev/null \
    && rm -rf /tmp/Catch2
