#pragma once

#include <Arduino.h>

typedef std::function<void(uint8_t count)> ButtonOnClickFn;
typedef ButtonOnClickFn ButtonOnHoldFn;

struct ButtonState {
    bool hold = false;
    uint8_t click_count = 0;

    unsigned long timestamp = 0;
};

class Button {
    unsigned long _silence_interval = 40u;
    unsigned long _hold_interval = 500u;
    unsigned long _hold_call_interval = 500u;
    unsigned long _press_wait_interval = 500u;
    unsigned long _reset_interval = 1000u;

    bool _hold_repeat = true;

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

    bool _hold_called = false;
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

    inline void set_hold_repeat(bool value) {_hold_repeat = value;}
    [[nodiscard]] inline bool hold_repeat() const { return _hold_repeat; }

    inline void set_silence_interval(unsigned long interval) { _silence_interval = interval; }
    [[nodiscard]] inline unsigned long silence_interval() const { return _silence_interval; }

    inline void set_hold_interval(unsigned long interval) { _hold_interval = interval; }
    [[nodiscard]] inline unsigned long hold_interval() const { return _hold_interval; }

    inline void set_hold_call_interval(unsigned long interval) { _hold_call_interval = interval; }
    [[nodiscard]] inline unsigned long hold_call_interval() const { return _hold_call_interval; }

    inline void set_press_wait_interval(unsigned long interval) { _press_wait_interval = interval; }
    [[nodiscard]] inline unsigned long press_wait_interval() const { return _press_wait_interval; }

    inline void set_reset_interval(unsigned long interval) { _reset_interval = interval; }
    [[nodiscard]] inline unsigned long geset_interval() const { return _reset_interval; }


private:
    [[nodiscard]] bool _read() const;

    IRAM_ATTR static void _handle_interrupt_change_static(void *arg);

    IRAM_ATTR void _handle_interrupt_change();

    IRAM_ATTR void _handle_rising_interrupt(unsigned long delta);
    IRAM_ATTR void _handle_falling_interrupt(unsigned long delta);
};
