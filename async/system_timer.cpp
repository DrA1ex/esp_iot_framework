#ifdef ARDUINO_ARCH_ESP32

#include <esp_task_wdt.h>

#include "system_timer.h"
#include "promise.h"

bool SystemTimer::initialized = false;
uint64_t SystemTimer::begin_processing_micros = 0;
int SystemTimer::processed_tasks = 0;

SystemTimer::PriorityQueue SystemTimer::timers {};
portMUX_TYPE SystemTimer::spinlock = portMUX_INITIALIZER_UNLOCKED;

Future<void> SystemTimer::delay(unsigned long timeout_ms) {
    auto promise = Promise<void>::create();
    auto callback = [=] {
        promise->set_success();
    };

    if (!set_timeout(timeout_ms, std::move(callback))) {
        promise->set_error();
    }

    return Future {promise};
}

bool SystemTimer::set_timeout(unsigned long timeout_ms, CallbackType callback) {
    if (callback == nullptr) return false;

    portENTER_CRITICAL(&spinlock);

    if (!initialized) {
        if (!start_task()) {
            portEXIT_CRITICAL(&spinlock);
            return false;
        }

        initialized = true;
    }

    timers.push({.timeout_at = millis64() + timeout_ms, .callback = std::move(callback)});
    portEXIT_CRITICAL(&spinlock);

    VERBOSE(D_PRINTF("SystemTimer: Add new task. Total: %i\r\n", timers.size()));
    return true;
}

bool SystemTimer::start_task() {
    auto ret = xTaskCreatePinnedToCore(timer_task, "TimerCbTask",
        SYSTEM_TIMER_STACK_SIZE, nullptr, SYSTEM_TIMER_TASK_PRIORITY, nullptr, xPortGetCoreID());

    if (ret != pdPASS) {
        D_PRINTF("SystemTimer: Failed to start task: %x\r\n", pdPASS);
        return false;
    }

    return true;
}

[[noreturn]] void SystemTimer::timer_task(void *) {
    while (true) {
        begin_processing_micros = esp_timer_get_time();
        processed_tasks = 0;

        bool has_pending;
        do {
            has_pending = process_pending_tasks();
            if (has_pending) delay_if_too_long();
        } while (has_pending);

        if (processed_tasks > 0) VERBOSE(D_PRINT("SystemTimer: Waiting for new timer..."));
        delayMicroseconds(SYSTEM_TIMER_DELAY_INTERVAL_MICRO);
    }
}

bool SystemTimer::process_pending_tasks() {
    portENTER_CRITICAL(&spinlock);
    bool has_pending = has_pending_task();

    if (!has_pending) {
        portEXIT_CRITICAL(&spinlock);
        return false;
    }

    if (processed_tasks == 0) VERBOSE(D_PRINT("SystemTimer: Timers are ready. Processing..."));

    auto timer_task = std::move(const_cast<TimerTask &>(timers.top()));
    timers.pop();
    has_pending = has_pending_task();

    portEXIT_CRITICAL(&spinlock);

    VERBOSE(D_PRINTF("SystemTimer: Triggered at %llu (late for %llu ms). Left: %lu\r\n",
        timer_task.timeout_at, millis64() - timer_task.timeout_at, timers.size()));

    timer_task.callback();
    ++processed_tasks;

    return has_pending;
}

bool SystemTimer::has_pending_task() {
    return !timers.empty() && timers.top().timeout_at < millis64();
}

void SystemTimer::delay_if_too_long() {
    if (esp_timer_get_time() - begin_processing_micros > SYSTEM_TIMER_TASK_RUNNING_TIMEOUT_MICRO) {
        vTaskDelay(0);

        VERBOSE(D_PRINT("SystemTimer: Too long task execution. Wait before continue"));
        begin_processing_micros = esp_timer_get_time();
    } else {
        esp_task_wdt_reset();
    }
}


#endif
