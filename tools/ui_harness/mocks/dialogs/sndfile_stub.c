/*
 *  No-op implementation of the libsndfile surface the voice-message window uses
 *  (see mocks/dialogs/sndfile.h). Compiled ONLY into `ui_harness_dialogs`.
 *
 *  Every call is a documented no-op: no file is opened, read or written and no
 *  sample is processed. `sf_open` returns a valid opaque handle so the dialog's
 *  error path (which only runs on the Rec/Play buttons, never while rendering) is
 *  not taken and the window builds deterministically.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>

#include "sndfile.h"

static SNDFILE sndfile_dummy;

SNDFILE *sf_open(const char *path, int mode, SF_INFO *sfinfo)
{
    (void) path;
    (void) mode;
    (void) sfinfo;

    return &sndfile_dummy;
}

int sf_close(SNDFILE *sndfile)
{
    (void) sndfile;

    return 0;
}

sf_count_t sf_read_short(SNDFILE *sndfile, short *ptr, sf_count_t items)
{
    (void) sndfile;
    (void) ptr;
    (void) items;

    return 0;
}

sf_count_t sf_write_short(SNDFILE *sndfile, const short *ptr, sf_count_t items)
{
    (void) sndfile;
    (void) ptr;

    return items;
}

const char *sf_strerror(SNDFILE *sndfile)
{
    (void) sndfile;

    return "libsndfile is not linked in the host dialogs harness";
}
