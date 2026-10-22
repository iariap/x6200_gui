/*
 * Unit tests: country lookup from cty.dat (dxcc.c).
 *
 * The image ships cty.dat and nothing read it; the FT8 app uses it now to name the
 * country of a station. The test runs against **that same file** (the runner passes it
 * in CTY_PATH), because the parser normalises some names on the way in ("United
 * States" comes out as "USA") and a synthetic file would only test the fixture.
 *
 * dxcc_lookup parses the file once per process, so this is the only case here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dxcc.h"
#include "test_framework.h"

void test_dxcc(void) {
    const char *path = getenv("CTY_PATH");

    CHECK(path != NULL && path[0]);

    if (!path || !path[0]) {
        return;
    }

    CHECK(access(path, R_OK) == 0);

    const char *name = NULL;
    const char *prefix = NULL;

    /* The countries the image's cty.dat gives for these, as they come out of the
     * parser (it shortens a few: United States -> USA, Fed. Rep. of Germany ->
     * Germany) */
    CHECK(dxcc_lookup("LU1WXY", &name, &prefix));
    CHECK(name && strcmp(name, "Argentina") == 0);
    CHECK(prefix && strcmp(prefix, "LU") == 0);

    CHECK(dxcc_lookup("K1ABC", &name, &prefix));
    CHECK(name && strcmp(name, "USA") == 0);

    CHECK(dxcc_lookup("JA1ABC", &name, &prefix));
    CHECK(name && strcmp(name, "Japan") == 0);

    CHECK(dxcc_lookup("DL1ABC", &name, &prefix));
    CHECK(name && strcmp(name, "Germany") == 0);

    CHECK(dxcc_lookup("EA1ABC", &name, &prefix));
    CHECK(name && strcmp(name, "Spain") == 0);

    /* A prefix nobody has (ZZ, for instance, is Brazil) */
    CHECK(!dxcc_lookup("QQ9QQQ", &name, &prefix));

    /* Nothing to look up */
    CHECK(!dxcc_lookup("", &name, &prefix));
}
