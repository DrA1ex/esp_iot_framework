#pragma once

#include <Arduino.h>

#ifndef BTN_SILENCE_INTERVAL
#define BTN_SILENCE_INTERVAL                    (40u)
#endif

#ifndef BTN_HOLD_INTERVAL
#define BTN_HOLD_INTERVAL                       (500u)
#endif

#ifndef BTN_HOLD_CALL_INTERVAL
#define BTN_HOLD_CALL_INTERVAL                  (500u)
#endif

#ifndef BTN_PRESS_WAIT_INTERVAL
#define BTN_PRESS_WAIT_INTERVAL                 (500u)
#endif

#ifndef BTN_RESET_INTERVAL
#define BTN_RESET_INTERVAL                      (1000u)
#endif


typedef std::function<void(uint8_t count)> ButtonOnClickFn;
typedef ButtonOnClickFn ButtonOnHoldFn;

struct ButtonState {
    bool hold = false;
    uint8_t click_count = 0;

    unsigned long timestamp = 0;
};

class Button {
    volatile bool _hold = false;
    volatile int _click_count = 0;

    volatile unsigned long _last_impulse_time = 0;
    unsigned long _last_button_hold_call_time = 0;

    ButtonOnClickFn _click_handler = nullptr;
    ButtonOnHoldFn _hold_handler = nullptr;
    ButtonOnHoldFn _hold_release_handler = nullptr;

    uint8_t _pin;
    bool _high_state;
    bool _used_for_wakeup;

    bool _last_interrupt_state = false;
    ButtonState _last_state;

public:
    explicit Button(uint8_t pin, bool high_state = true, bool used_for_wakeup = false);

    void begin(uint8_t mode = INPUT);
    void handle();

    void end();

    [[nodiscard]] inline bool idle() const { return !_hold && _click_count == 0; }
    [[nodiscard]] inline const ButtonState &last_state() const { return _last_state; }

    inline void set_on_click(const ButtonOnClickFn &fn) { _click_handler = fn; }
    inline void set_on_hold(const ButtonOnHoldFn &fn) { _hold_handler = fn; }
    inline void set_on_hold_release(const ButtonOnHoldFn &fn) { _hold_release_handler = fn; }

private:
    [[nodiscard]] bool _read() const;

    IRAM_ATTR static void _handle_interrupt_change_static(void *arg);

    IRAM_ATTR void _handle_interrupt_change();

    IRAM_ATTR void _handle_rising_interrupt(unsigned long delta);
    IRAM_ATTR void _handle_falling_interrupt(unsigned long delta);
};
