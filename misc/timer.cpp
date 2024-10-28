#include "Arduino.h"

#include "./timer.h"
#include "../debug.h"

Timer::~Timer() {
    if (_entries == nullptr) return;

    delete[] _entries;
    _entries = nullptr;

    _count = 0;
    _free_count = 0;
}

unsigned long Timer::add_timeout(const TimerFn &callback, unsigned long interval, void *parameter) {
    return _add(callback, interval, false, parameter);
}

void Timer::clear_timeout(unsigned long timer_id) {
    _clear(timer_id);
}

unsigned long Timer::add_interval(const TimerFn &callback, unsigned long interval, void *parameter) {
    return _add(callback, interval, true, parameter);
}

void Timer::clear_interval(unsigned long timer_id) {
    _clear(timer_id);
}

void Timer::handle_timers() {
    if (_entries == nullptr || _count == _free_count) return;

    for (unsigned long i = 0; i < _count; ++i) {
        auto &entry = _entries[i];

        const auto now = millis();
        if (!entry.active || (now - entry.created_at) < entry.interval) continue;

        if (entry.repeat) {
            entry.created_at = now;
        } else {
            // To avoid repeating run if handle_timers() was called from callback
            entry.interval = ~0ul;
        }

        VERBOSE(D_PRINTF("Call timer: %lu\r\n", i));
        entry.callback(entry.parameter);

        if (!entry.repeat) {
            _clear(i);
        }
    }
}

unsigned long Timer::_add(const TimerFn &callback, unsigned long interval, bool repeat, void *parameter) {
    if (_free_count == 0) _grow();

    for (unsigned long i = 0; i < _count; ++i) {
        auto &entry = _entries[i];
        if (entry.active) continue;

        entry.active = true;
        entry.created_at = millis();
        entry.interval = interval;
        entry.repeat = repeat;
        entry.callback = callback;
        entry.parameter = parameter;

        _free_count--;

        VERBOSE(D_PRINTF("Add %s: %lu. Used: %lu / %lu\r\n", repeat ? "interval" : "timeout", i, _count - _free_count, _count));

        return i;
    }

    D_PRINT("Timer: Failed to add timer. No free slots!");
    return -1ul;
}

void Timer::_clear(unsigned long timer_id) {
    if (_entries == nullptr) return;

    auto &entry = _entries[timer_id];
    if (!entry.active) return;

    entry = TimerEntry();
    _free_count++;

    VERBOSE(D_PRINTF("Remove timer: %lu. Used: %lu / %lu\r\n", timer_id, _count - _free_count, _count));
}

void Timer::_grow() {
    const unsigned long new_count = _count + TIMER_GROW_AMOUNT;
    TimerEntry *new_data = nullptr;

    new_data = new(std::nothrow) TimerEntry[new_count];
    if (!new_data) {
        D_PRINT("Timer: Unable to allocate memory");
        return;
    }

    if (_entries != nullptr) {
        for (unsigned long i = 0; i < _count; ++i) {
            new_data[i] = std::move(_entries[i]);
        }

        delete[] _entries;
    }

    D_PRINTF("Grow timer memory from %lu to %lu\r\n", _count, new_count);

    _entries = new_data;
    _count = new_count;
    _free_count += TIMER_GROW_AMOUNT;
}
