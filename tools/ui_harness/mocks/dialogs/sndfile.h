/*
 *  Minimal stand-in for libsndfile's <sndfile.h>, used ONLY by the
 *  `ui_harness_dialogs` target (CMakeLists.txt, -Imocks/dialogs).
 *
 *  The host build image (`x6200-uiharness`, the CI stage of Dockerfile) does not
 *  ship libsndfile, and the voice-message window (src/dialog_msg_voice.c) pulls
 *  <sndfile.h> for its WAV record/play backend. Rendering the window never
 *  touches that backend (it runs on the Rec/Play buttons), so the target links
 *  the no-op implementation below (mocks/dialogs/sndfile_stub.c) and declares
 *  the WAV/audio side SYNTHETIC in the coverage table.
 *
 *  Only the surface src/dialog_msg_voice.c uses is declared, with the real
 *  library's signatures (sf_count_t is int64_t, SFM_READ/SFM_WRITE and the
 *  WAV/PCM_16 format bits keep libsndfile's values). It is a mock of an absent
 *  audio library, not a reimplementation: it can neither read nor write a file.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdint.h>

typedef int64_t sf_count_t;

typedef struct SNDFILE_tag SNDFILE;

/* The opaque handle, defined here so the no-op stub can hold one statically.
 * The real library keeps its own definition private; nothing of this struct is
 * read by the dialog. */
struct SNDFILE_tag {
    int unused;
};

typedef struct {
    sf_count_t frames;
    int        samplerate;
    int        channels;
    int        format;
    int        sections;
    int        seekable;
} SF_INFO;

#define SFM_READ  (0x10)
#define SFM_WRITE (0x20)

#define SF_FORMAT_WAV    (0x010000)
#define SF_FORMAT_PCM_16 (0x0002)

SNDFILE    *sf_open(const char *path, int mode, SF_INFO *sfinfo);
int         sf_close(SNDFILE *sndfile);
sf_count_t  sf_read_short(SNDFILE *sndfile, short *ptr, sf_count_t items);
sf_count_t  sf_write_short(SNDFILE *sndfile, const short *ptr, sf_count_t items);
const char *sf_strerror(SNDFILE *sndfile);
