/*
 * x6200-remote: manda una linea JSON al socket de control de la GUI (/tmp/x6200_remote.sock) y
 * muestra lo que conteste. Sirve para manejar la radio sin teclas: abrir una app, pedir el estado,
 * inyectar un spot, etc. (mismo protocolo que la pagina /remote del webserver).
 *
 *   x6200-remote '{"cmd":"app","text":"Settings"}'
 *
 * Se compila para la radio con el toolchain de buildroot:
 *   tools/remote/compila.sh
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCK "/tmp/x6200_remote.sock"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s '<json>'\n", argv[0]);
        return 2;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un dir = {0};
    dir.sun_family = AF_UNIX;
    snprintf(dir.sun_path, sizeof(dir.sun_path), "%s", SOCK);

    if (connect(fd, (struct sockaddr *) &dir, sizeof(dir)) < 0) {
        perror("connect (¿esta la GUI andando?)");
        return 1;
    }

    struct timeval to = {.tv_sec = 2, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));

    if (write(fd, argv[1], strlen(argv[1])) <= 0 || write(fd, "\n", 1) <= 0) {
        perror("write");
        close(fd);
        return 1;
    }

    char    buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    if (n > 0) {
        buf[n] = 0;
        fputs(buf, stdout);
    }

    close(fd);
    return 0;
}
