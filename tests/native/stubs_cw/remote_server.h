/*
 * Stub header for the native CW decoder test (see tests/native/test_cw_decoder.c
 * and the cw_src stage in tests/native/run_native_tests.sh).
 *
 * The real remote_server.h pulls in cJSON and the C++ bridge; the decoder only
 * needs the remote_text() prototype for the no-sink fallback.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>

void remote_text(const char *type, const char *s, size_t n);
