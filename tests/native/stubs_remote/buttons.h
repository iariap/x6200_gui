/* Stub for src/buttons.h (native remote_server test): the page groups and buttons_press. */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct buttons_page_t buttons_page_t;
typedef buttons_page_t *buttons_group_t[];

extern buttons_group_t buttons_group_gen;
extern buttons_group_t buttons_group_app;
extern buttons_group_t buttons_group_key;
extern buttons_group_t buttons_group_dfn;
extern buttons_group_t buttons_group_dfl;

void buttons_press(uint8_t n, bool hold);
void buttons_load_page_group(buttons_group_t group);
