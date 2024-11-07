#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <functional>
#include <queue>

#ifndef DISPATCHER_STACK_SIZE
#define DISPATCHER_STACK_SIZE                               (4096u)
#endif

#ifndef DISPATCHER_TASK_PRIORITY
#define DISPATCHER_TASK_PRIORITY                            (1u)
#endif

#ifndef DISPATCHER_TASK_RUNNING_TIMEOUT_MICRO
#define DISPATCHER_TASK_RUNNING_TIMEOUT_MICRO               (100)
#endif

class Dispatcher {
    using PrivateDispatchFn = std::function<void()>;

    static bool initialized;
    static uint64_t begin_processing_micros;
    static int processed_tasks;
    static TaskHandle_t task_handle;

    static portMUX_TYPE spinlock;
    static std::queue<PrivateDispatchFn> dispatched;

public:
    using DispatchFn = PrivateDispatchFn;

    Dispatcher() = delete;

    static bool begin();
    static bool dispatch(DispatchFn fn);

private:
    static bool notify_from_isr();
    static bool notify();

    static void dispatcher_task(void *);

    static bool process_pending_tasks();
    static bool has_pending_task();
    static void delay_if_too_long();
};

#endif