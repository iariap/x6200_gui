# Host-side build image for the UI harness: the real GUI sources are compiled
# for the PC with stock LVGL and rendered headlessly to PNG.
FROM x6200-buildroot

RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
        build-essential cmake git pkg-config \
        libpng-dev libfftw3-dev libsndfile1-dev libglib2.0-dev libnm-dev \
        libsqlite3-dev sqlite3 \
        autoconf automake libtool \
    && rm -rf /var/lib/apt/lists/*

# liquid-dsp 1.4.0: the WSPR baseband recorder (NCO + resampler) and the
# waterfall use it, so the host tests need the real library, not a stub.
RUN git clone -q --depth 1 --branch v1.4.0 https://github.com/jgaeddert/liquid-dsp /tmp/liquid-dsp \
    && cd /tmp/liquid-dsp && ./bootstrap.sh >/dev/null && ./configure --prefix=/usr/local >/dev/null \
    && make -j"$(nproc)" >/dev/null && make install >/dev/null && ldconfig \
    && rm -rf /tmp/liquid-dsp

# Catch2 and Boost headers: the X6100's JS8 test suite (tests/test_js8.cpp) is
# built against them, and js8core itself needs Boost.
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq libboost-dev \
    && rm -rf /var/lib/apt/lists/* \
    && git clone -q --depth 1 --branch v3.5.2 https://github.com/catchorg/Catch2 /tmp/Catch2 \
    && cmake -S /tmp/Catch2 -B /tmp/Catch2/build -DCMAKE_BUILD_TYPE=Release \
         -DCATCH_BUILD_TESTING=OFF -DCATCH_INSTALL_DOCS=OFF >/dev/null \
    && cmake --build /tmp/Catch2/build --target install -j"$(nproc)" >/dev/null \
    && rm -rf /tmp/Catch2
