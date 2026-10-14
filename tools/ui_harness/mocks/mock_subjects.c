/*
 *  Subject / observer implementation for the host UI harness.
 *
 *  The firmware keeps subjects in a C++ template (src/cfg/subjects.h +
 *  subjects.cpp). The GUI components only use the opaque pointer and the C
 *  API declared at the bottom of that header, so the harness provides a small
 *  C implementation with the same observable behaviour: setting a value
 *  notifies the observers, and the delayed ones are flushed by
 *  observer_delayed_notify_all() (the firmware's main loop does that once per
 *  tick; virtual_time_step() does it here).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>

#include "cfg/subjects.h"

struct Observer {
    Subject *subj;
    void (*fn)(Subject *, void *);
    void *user_data;
    int   delayed;
    int   pending;
    struct Observer *next;          /* the subject's own list */
    struct Observer *next_delayed;  /* the global delayed list */
};

struct Subject {
    enum data_type type;
    int32_t  ival;
    uint64_t uval;
    float    fval;
    struct Observer *observers;
};

/* Delayed notifications waiting for the next tick. */
static struct Observer *delayed_list;

static Subject *subject_new(enum data_type type)
{
    Subject *s = calloc(1, sizeof(*s));

    s->type = type;
    return s;
}

Subject *subject_create_int(int32_t val)
{
    Subject *s = subject_new(DTYPE_INT);

    s->ival = val;
    return s;
}

Subject *subject_create_uint64(uint64_t val)
{
    Subject *s = subject_new(DTYPE_UINT64);

    s->uval = val;
    return s;
}

Subject *subject_create_float(float val)
{
    Subject *s = subject_new(DTYPE_FLOAT);

    s->fval = val;
    return s;
}

int32_t subject_get_int(Subject *subj)
{
    return subj ? subj->ival : 0;
}

uint64_t subject_get_uint64(Subject *subj)
{
    return subj ? subj->uval : 0;
}

float subject_get_float(Subject *subj)
{
    return subj ? subj->fval : 0.0f;
}

enum data_type subject_get_dtype(Subject *subj)
{
    return subj ? subj->type : DTYPE_INVALID;
}

static Observer *observer_new(Subject *subj, void (*fn)(Subject *, void *),
                              void *user_data, int delayed)
{
    Observer *o = calloc(1, sizeof(*o));

    o->subj = subj;
    o->fn = fn;
    o->user_data = user_data;
    o->delayed = delayed;
    o->next = subj->observers;
    subj->observers = o;

    if (delayed) {
        o->next_delayed = delayed_list;
        delayed_list = o;
    }
    return o;
}

static Observer *subject_add_observer_impl(Subject *subj, void (*fn)(Subject *, void *),
                                           void *user_data, int delayed, int call_now)
{
    if (!subj || !fn) {
        return NULL;
    }

    Observer *o = observer_new(subj, fn, user_data, delayed);

    if (call_now) {
        fn(subj, user_data);
    }
    return o;
}

Observer *subject_add_observer(Subject *subj, void (*fn)(Subject *, void *), void *user_data)
{
    return subject_add_observer_impl(subj, fn, user_data, 0, 0);
}

Observer *subject_add_observer_and_call(Subject *subj, void (*fn)(Subject *, void *), void *user_data)
{
    return subject_add_observer_impl(subj, fn, user_data, 0, 1);
}

ObserverDelayed *subject_add_delayed_observer(Subject *subj, void (*fn)(Subject *, void *), void *user_data)
{
    return (ObserverDelayed *) subject_add_observer_impl(subj, fn, user_data, 1, 0);
}

ObserverDelayed *subject_add_delayed_observer_and_call(Subject *subj, void (*fn)(Subject *, void *), void *user_data)
{
    return (ObserverDelayed *) subject_add_observer_impl(subj, fn, user_data, 1, 1);
}

static void subject_notify(Subject *subj)
{
    for (Observer *o = subj->observers; o; o = o->next) {
        if (o->delayed) {
            o->pending = 1;
            continue;
        }
        o->fn(o->subj, o->user_data);
    }
}

void subject_set_int(Subject *subj, int32_t val)
{
    if (!subj || subj->ival == val) {
        return;
    }
    subj->ival = val;
    subject_notify(subj);
}

void subject_set_uint64(Subject *subj, uint64_t val)
{
    if (!subj || subj->uval == val) {
        return;
    }
    subj->uval = val;
    subject_notify(subj);
}

void subject_set_float(Subject *subj, float val)
{
    if (!subj || subj->fval == val) {
        return;
    }
    subj->fval = val;
    subject_notify(subj);
}

void observer_del(Observer *observer)
{
    /* The harness builds one screen per process and never removes an observer;
     * the firmware does, so keep the API: nothing to free here. */
    (void) observer;
}

void observer_delayed_del(ObserverDelayed *observer)
{
    (void) observer;
}

void observer_delayed_notify_all(void)
{
    for (Observer *o = delayed_list; o; o = o->next_delayed) {
        if (o->pending) {
            o->pending = 0;
            o->fn(o->subj, o->user_data);
        }
    }
}
