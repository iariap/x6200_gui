# Host-side build image for the UI harness: the real GUI sources are compiled
# for the PC with stock LVGL and rendered headlessly to PNG.
FROM x6200-buildroot

RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
        build-essential cmake git pkg-config \
        libpng-dev libfftw3-dev libsndfile1-dev libglib2.0-dev libnm-dev \
        libsqlite3-dev sqlite3 \
        autoconf automake libtool \
    && rm -rf /var/lib/apt/lists/*
