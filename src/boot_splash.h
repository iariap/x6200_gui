/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Cesion del framebuffer del arranque neo-retro a la GUI (issue #192).
 *
 *  El script /etc/init.d/S01bootanim arranca x6200_bootanim, que dibuja en
 *  /dev/fb0 durante todo el arranque. Este modulo es el unico lugar de la GUI
 *  que le pide que suelte la pantalla: hay UN solo escritor visible del
 *  framebuffer por fase, nunca dos.
 *
 *  Protocolo (contrato, seccion 3): SIGUSR1 al PID del animador, 100 ms de
 *  espera en sondeos de 10 ms; si sigue vivo, SIGTERM al MISMO PID re-verificado
 *  y 20 ms mas; como ultimo recurso SIGKILL, solo si el PID y su identidad
 *  vuelven a coincidir. La identidad se verifica con /proc/<pid>/exe: un PID
 *  reciclado por otro proceso jamas recibe una senal.
 *
 *  Este archivo NO depende de LVGL (a proposito): asi el banco de host lo
 *  compila y lo prueba con procesos reales (tests/bootanim/handoff_test.c).
 */
#ifndef X6200_BOOT_SPLASH_H
#define X6200_BOOT_SPLASH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Presupuesto de espera del contrato, en milisegundos. */
#define BOOT_SPLASH_POLL_MS      10u
#define BOOT_SPLASH_USR1_WAIT_MS 100u
#define BOOT_SPLASH_TERM_WAIT_MS 20u
#define BOOT_SPLASH_KILL_WAIT_MS 20u

/* Detiene el animador temprano y confirma que solto el framebuffer.
 *   true  = no hay animador o ya termino (se puede pintar la GUI sin duplicar
 *           escritores);
 *   false = no se pudo confirmar (kill fallo): la GUI sigue igual, pero queda
 *           registrado en stderr.
 * No hace nada si no hay PIDFILE, si el PID es invalido/corrupto/ajeno o si el
 * proceso ya no existe. Nunca bloquea mas de USR1_WAIT + TERM_WAIT + KILL_WAIT. */
bool boot_splash_stop(void);

/* Seam del banco de pruebas: cambia el PIDFILE y el ejecutable del animador
 * (con NULL en cualquiera de los dos, se deja el valor del contrato). En la ROM
 * se usan siempre los defaults: /var/run/x6200_bootanim.pid y
 * /usr/sbin/x6200_bootanim. */
void boot_splash_set_paths(const char *pidfile, const char *exe);

#ifdef __cplusplus
}
#endif

#endif /* X6200_BOOT_SPLASH_H */
