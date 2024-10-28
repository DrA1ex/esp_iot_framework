#include "button.h"

#include <esp_sleep.h>

#include "../debug.h"

Button::Button(uint8_t pin, bool high_state, bool used_for_wakeup) : _pin(pin), _high_state(high_state),
                                                                     _used_for_wakeup(used_for_wakeup) {}

void Button::begin(uint8_t mode) {
    pinMode(_pin, mode);

#ifdef SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    if (_used_for_wakeup) {
        auto wakeup_status = esp_sleep_get_gpio_wakeup_status();
        bool waked_up = wakeup_status & (1 << _pin);
        if (!_read() && waked_up) {
            _last_impulse_time = millis();
            _click_count = 1;
        }
    }
#endif

    _last_interrupt_state = _read();
    attachInterruptArg(digitalPinToInterrupt(_pin), _handle_interrupt_change_static, this, CHANGE);

    D_PRINTF("Setup button interruption for pin %u\n", _pin);
}

void Button::end() {
    detachInterrupt(digitalPinToInterrupt(_pin));
    pinMode(_pin, INPUT);

    _click_count = 0;
    _hold = false;

    D_PRINTF("Detach button interruption for pin %u\n", _pin);
}

bool Button::_read() const {
    return digitalRead(_pin) ^ !_high_state;
}

void Button::_handle_interrupt_change_static(void *arg) {
    auto self = (Button *) arg;
    self->_handle_interrupt_change();
}

void Button::_handle_interrupt_change() {
    auto delta = millis() - _last_impulse_time;
    _last_impulse_time = millis();

    if (delta < BTN_SILENCE_INTERVAL) {
        VERBOSE(D_PRINT("Button: filtering noise"));
        return;
    }

    bool state = _read();
    if (state == _last_interrupt_state) {
        VERBOSE(D_PRINTF("Button interruption: Got same state (%i). Inverting\n", state));
        state = !state;
    }

    _last_interrupt_state = state;
    if (state) {
        _handle_rising_interrupt(delta);
    } else {
        _handle_falling_interrupt(delta);
    }
}


void Button::_handle_rising_interrupt(unsigned long delta) {
    VERBOSE(D_PRINT("Button Interception: RISING"));
    if ((_click_count || _hold) && delta > BTN_RESET_INTERVAL) {
        VERBOSE(D_PRINT("Button Interception: Start Over. Forget to call Button::handle() ?"));
        _hold = false;
        _click_count = 0;
    }
}

void Button::_handle_falling_interrupt(unsigned long delta) {
    VERBOSE(D_PRINT("Button Interception: FALLING"));
    if (!_hold) {
        VERBOSE(D_PRINT("Button Interception: Click"));
        _click_count++;
    }
}


void Button::handle() {
    unsigned long delta = millis() - _last_impulse_time;

    const bool state = _read();
    if (!_hold && state && delta >= BTN_HOLD_INTERVAL) {
        VERBOSE(D_PRINT("Button: Set Hold"));
        _hold = true;
        _click_count++;
    } else if (_hold && !state) {
        D_PRINT("Button: Button hold release");

        if (_hold_release_handler != nullptr) {
            _hold_release_handler(_click_count);
        }

        _hold = false;
        _click_count = 0;
        _last_interrupt_state = false;
    }

    if (_hold) {
        auto hold_call_delta = millis() - _last_button_hold_call_time;
        if (hold_call_delta >= BTN_HOLD_CALL_INTERVAL) {
            D_PRINTF("Button: Hold #%i\n", _click_count);

            _last_state.click_count = _click_count;
            _last_state.hold = true;
            _last_state.timestamp = millis();

            if (_hold_handler != nullptr) {
                _hold_handler(_click_count);
            }

            _last_button_hold_call_time = millis();
        }
    } else if (_click_count && delta > BTN_PRESS_WAIT_INTERVAL) {
        D_PRINTF("Button: Click count %i\n", _click_count);

        _last_state.click_count = _click_count;
        _last_state.hold = false;
        _last_state.timestamp = millis();

        if (_click_handler != nullptr) {
            _click_handler(_click_count);
        }

        _click_count = 0;
        _last_interrupt_state = false;
    }
}
