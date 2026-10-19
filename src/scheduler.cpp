/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#include "scheduler.h"

#include <queue>
#include <mutex>
#include <thread>

extern "C" {
    #include "lvgl/lvgl.h"
}

#define QUEUE_MAX_SIZE  64

struct item_t {
    scheduler_fn_t fn;
    void    *arg;
};

static std::queue<item_t> queue;
static std::mutex m_mutex;

/* Hilo de GUI (issue #131): lo fija scheduler_thread_init() desde main(). Antes
 * de esa llamada se responde true para no diferir nada de la inicializacion. */
static std::thread::id gui_tid;
static bool            gui_tid_set = false;

void scheduler_thread_init(void) {
    gui_tid     = std::this_thread::get_id();
    gui_tid_set = true;
}

bool scheduler_in_main_thread(void) {
    return !gui_tid_set || std::this_thread::get_id() == gui_tid;
}

bool scheduler_put(scheduler_fn_t fn, void * arg, size_t arg_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (queue.size() > QUEUE_MAX_SIZE){
        LV_LOG_ERROR("Scheduler queue overflow");
        return false;
    }
    void *arg_copy = nullptr;
    if (arg_size) {
        arg_copy = malloc(arg_size);
        if (arg_copy == nullptr) {
            LV_LOG_ERROR("Scheduler argument copy failed");
            return false;
        }
        memcpy(arg_copy, arg, arg_size);
    }
    item_t item = {fn, arg_copy};
    queue.push(item);
    return true;
}

bool scheduler_put_noargs(scheduler_fn_t fn) {
    return scheduler_put(fn, NULL, 0);
}

void scheduler_work() {
    item_t item;
    while (!queue.empty()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            item = queue.front();
            queue.pop();
        }
        item.fn(item.arg);
        if (item.arg) {
            free(item.arg);
        }
    }
}
