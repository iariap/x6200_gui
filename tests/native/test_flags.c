/*
 * Unit tests: the country flags' colour conversion (flags.c).
 *
 * flags_data.h is generated (RGB888); LVGL draws RGB565. The conversion is the only
 * logic in that module, so it is tested on the host, together with the prefix table
 * the generator wrote.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

#include "flags.h"
#include "test_framework.h"

void test_flags(void) {
    /* The corners and a couple of mixed colours */
    CHECK(flags_rgb888_to_565(0x000000) == 0x0000);
    CHECK(flags_rgb888_to_565(0xFFFFFF) == 0xFFFF);
    CHECK(flags_rgb888_to_565(0xFF0000) == 0xF800);
    CHECK(flags_rgb888_to_565(0x00FF00) == 0x07E0);
    CHECK(flags_rgb888_to_565(0x0000FF) == 0x001F);

    /* The low bits are dropped, not rounded up */
    CHECK(flags_rgb888_to_565(0x080000) == 0x0800);
    CHECK(flags_rgb888_to_565(0x000400) == 0x0020);
    CHECK(flags_rgb888_to_565(0x000008) == 0x0001);

    /* A green like the ones in the flags */
    CHECK(flags_rgb888_to_565(0x009B3A) == 0x04C7);

    /* The table that comes with the generated header */
    CHECK(flags_count() > 100);

    int index = -1;

    CHECK(flags_index_of_prefix("LU", &index));
    CHECK(index >= 0 && index < flags_count());

    /* An unknown prefix is not invented */
    CHECK(!flags_index_of_prefix("QQ", &index));
    CHECK(!flags_index_of_prefix("", &index));
    CHECK(!flags_index_of_prefix(NULL, &index));

    /* The same prefix always gives the same flag */
    int again = -1;

    CHECK(flags_index_of_prefix("LU", &again));
    CHECK(again == index);
}
