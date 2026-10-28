# Entorno aislado del banco de SpeexDSP (issue #345, tarea B).
#
# Base pinneada por digest y libspeexdsp pinneada por version: el banco no
# depende de lo que tenga el host (aca no hay libspeexdsp: `pkg-config --exists
# speexdsp` falla). El resto del toolchain se registra en commands.log en cada
# corrida (no se pinnea para que la imagen siga construyendo ante un point
# release de Debian).
FROM debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        gcc \
        libc6-dev \
        libspeexdsp-dev=1.2.1-1 \
        pkg-config \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /build
