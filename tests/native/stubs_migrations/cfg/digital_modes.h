/*
 * Minimal digital-modes surface for the native CB-bands migration test
 * (issue #146): src/params/migrations.c embeds CFG_DIG_TYPE_FT8 / CFG_DIG_TYPE_FT4
 * in the issue #51 migration's SQL. Only the two enum values are needed.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef enum {
    CFG_DIG_TYPE_FT8,
    CFG_DIG_TYPE_FT4,
    CFG_DIG_TYPE_JS8,
    CFG_DIG_TYPE_JS8_GHOSTNET,
    CFG_DIG_TYPE_WSPR,
} cfg_digital_type_t;
