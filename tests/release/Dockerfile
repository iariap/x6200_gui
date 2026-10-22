# Private, disposable lab for the update path of the X6200 ROM
# (issue iariap/x6200_gui#215, epic #211).
#
# It is an x86_64 userland with qemu-user-static: the target root filesystem
# (ARM, with the image's own BusyBox and bash) is run inside a chroot with those
# real ARM binaries, which is the "HOST + QEMU_USER/chroot privado con binario ARM
# real y rootfs/BusyBox objetivo" the epic asks for. It is NOT the ROM bench: it
# never touches the radio, the owner's SD card, the shared containers or the
# shared volumes (banco-apps, banco-audio, x6200-build-output).
#
#   docker build -t h215-x6200-update-lab tests/release
#
# tests/release/test_safe_update.sh builds it and runs it for you.
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        bash coreutils util-linux qemu-user-static gzip tar python3 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

CMD ["/bin/bash"]
