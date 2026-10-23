/*
 * Minimal cfg/cfg.h for the WiFi idle-pump native stage (issue #218, F-06).
 * src/wifi.cpp includes it but never reads the cfg struct, so the stage needs the header
 * to exist and nothing else. El header real arrastra sqlite3/liquid/params y no aporta
 * nada a esta medicion.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
