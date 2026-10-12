#pragma once

#include "buttons.h"
#include "controls_ids.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Encoder-control ids of the X6100 apps. El registro está portado en controls_encoder.c
 * (ver docs/NEW_FEATURES.md §38): la app toma el control con mfk_set_ctrl() y los giros del
 * MFK van a controls_encoder_update(), no al parámetro del MFK de la pantalla principal. */


/* Aplica `diff` al control que tomó la app con mfk_set_ctrl; devuelve el valor nuevo. */
int32_t controls_encoder_update(int ctrl, int32_t diff);

/* Topes y pasos, sin dependencias: lo cubren los tests nativos (controls_step.c). */
int32_t controls_step(int ctrl, int32_t actual, int32_t diff);

void controls_toggle_agc_hang(button_item_t *btn);
void controls_toggle_key_train(button_item_t *btn);
void controls_toggle_key_iambic_mode(button_item_t *btn);
void controls_toggle_cw_decoder(button_item_t *btn);
void controls_toggle_cw_tuner(button_item_t *btn);

void controls_loop_dnf(button_item_t *btn);
void controls_toggle_nb(button_item_t *btn);
void controls_toggle_nr(button_item_t *btn);

void controls_toggle_att(button_item_t *btn);
void controls_toggle_pre(button_item_t *btn);
void controls_toggle_comp(button_item_t *btn);

#ifdef __cplusplus
}
#endif
