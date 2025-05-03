#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <functional>
#include <queue>

#ifndef SYSTEM_TIMER_STACK_SIZE
#define SYSTEM_TIMER_STACK_SIZE                             (4096u)
#endif

#ifndef SYSTEM_TIMER_TASK_PRIORITY
#define SYSTEM_TIMER_TASK_PRIORITY                          (1u)
#endif

#ifndef SYSTEM_TIMER_DELAY_INTERVAL_MICRO
#define SYSTEM_TIMER_DELAY_INTERVAL_MICRO                   (1000u)
#endif

#ifndef SYSTEM_TIMER_TASK_RUNNING_TIMEOUT_MICRO
#define SYSTEM_TIMER_TASK_RUNNING_TIMEOUT_MICRO             (100)
#endif

template<typename T> class Future;

class SystemTimer {
    struct TimerTask;

    static bool initialized;
    static uint64_t begin_processing_micros;
    static int processed_tasks;

    typedef std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<>> PriorityQueue;
    static PriorityQueue timers;
    static portMUX_TYPE spinlock;

public:
    typedef std::function<void()> CallbackType;
    SystemTimer() = delete;

    static Future<void> delay(unsigned long timeout_ms);
    static bool set_timeout(unsigned long timeout_ms, CallbackType callback);

private:
    struct TimerTask {
        uint64_t timeout_at;
        CallbackType callback;

        bool operator>(const TimerTask &other) const { return timeout_at > other.timeout_at; }
    };

    static bool start_task();
    [[noreturn]] static void timer_task(void *arg);

    static bool process_pending_tasks();
    static bool has_pending_task();
    static void delay_if_too_long();

    // Actually it's ~ 53 bits, but it doesn't really matter...
    static uint64_t millis64() { return esp_timer_get_time() / 1000; }
};

#endif
