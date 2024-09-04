#pragma once

#ifndef TIMER_GROW_AMOUNT
#define TIMER_GROW_AMOUNT                       (8u)
#endif

#include "functional"

typedef std::function<void(void *)> TimerFn;

struct TimerEntry {
    bool active = false;
    bool repeat = false;
    TimerFn callback = nullptr;
    void *parameter = nullptr;
    unsigned long interval = 0;
    unsigned long created_at = 0;
};

class Timer {
    TimerEntry *_entries = nullptr;
    unsigned long _count = 0;
    unsigned long _free_count = 0;

    void _grow();
    unsigned long _add(TimerFn callback, unsigned long interval, bool repeat, void *parameter = nullptr);
    void _clear(unsigned long timer_id);

public:
    ~Timer();

    void handle_timers();

    unsigned long add_timeout(TimerFn callback, unsigned long interval, void *parameter = nullptr);
    void clear_timeout(unsigned long timer_id);

    unsigned long add_interval(TimerFn callback, unsigned long interval, void *parameter = nullptr);
    void clear_interval(unsigned long timer_id);
};