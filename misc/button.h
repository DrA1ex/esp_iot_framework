#pragma once

#include <Arduino.h>

#ifndef BTN_SILENCE_INTERVAL
#define BTN_SILENCE_INTERVAL                    (40u)
#endif

#ifndef BTN_HOLD_INTERVAL
#define BTN_HOLD_INTERVAL                       (500u)
#endif

#ifndef BTN_PRESS_WAIT_INTERVAL
#define BTN_PRESS_WAIT_INTERVAL                 (500u)
#endif

#ifndef BTN_RESET_INTERVAL
#define BTN_RESET_INTERVAL                      (1000u)
#endif


typedef std::function<void(uint8_t count)> ButtonOnClickFn;
typedef ButtonOnClickFn ButtonOnHoldFn;

class Button {
    volatile bool _hold = false;
    volatile int _click_count = 0;

    volatile unsigned long _last_impulse_time = 0;

    ButtonOnClickFn _click_handler = nullptr;
    ButtonOnHoldFn _hold_handler = nullptr;
    ButtonOnHoldFn _hold_release_handler = nullptr;

    uint8_t _pin;

public:
    explicit Button(uint8_t pin);

    void begin();
    void handle();

    inline void set_on_click(const ButtonOnClickFn &fn) { _click_handler = fn; }
    inline void set_on_hold(const ButtonOnHoldFn &fn) { _hold_handler = fn; }
    inline void set_on_hold_release(const ButtonOnHoldFn &fn) { _hold_release_handler = fn; }

private:
    IRAM_ATTR static void _handle_interrupt(void *arg);
    IRAM_ATTR void _handle_interrupt();

    IRAM_ATTR void _handle_rising_interrupt();
    IRAM_ATTR void _handle_falling_interrupt();
};
