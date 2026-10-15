/*
 * Stub header for the native CW decoder test (see tests/native/test_cw_decoder.c
 * and the cw_src stage in tests/native/run_native_tests.sh).
 *
 * cw_decoder.c falls back to the panel when no sink is set; the test always sets
 * a sink, so this only has to satisfy the compiler and the linker.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void pannel_add_text(const char *text);
