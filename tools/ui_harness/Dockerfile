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
