#include "button.h"

#include "../debug.h"

Button::Button(uint8_t pin) : _pin(pin) {}

void Button::begin() {
    pinMode(_pin, INPUT);

    attachInterruptArg(digitalPinToInterrupt(_pin), _handle_interrupt, this, CHANGE);

    D_PRINTF("Setup button interruption for pin %u\n", _pin);
}

void Button::_handle_interrupt(void *arg) {
    auto self = (Button *) arg;
    self->_handle_interrupt();
}

void Button::_handle_interrupt() {
    auto silence_time = millis() - _last_impulse_time;
    _last_impulse_time = millis();

    if (silence_time < BTN_SILENCE_INTERVAL) return;

    if (digitalRead(_pin)) {
        _handle_rising_interrupt();
    } else {
        _handle_falling_interrupt();
    }
}


void Button::_handle_rising_interrupt() {
    const auto delta = millis() - _last_impulse_time;
    if (delta > BTN_RESET_INTERVAL) {
        VERBOSE(D_PRINT("Button Interception: Start Over"));
        _click_count = 0;
    }
}


void Button::_handle_falling_interrupt() {
    if (!_hold) {
        VERBOSE(D_PRINT("Button Interception: Click"));
        _click_count++;
    }
}


void Button::handle() {
    unsigned long delta = millis() - _last_impulse_time;

    const bool state = digitalRead(_pin);
    if (!_hold && state && delta >= BTN_HOLD_INTERVAL) {
        VERBOSE(D_PRINT("Button: Set Hold"));
        _hold = true;
        _click_count++;
    } else if (_click_count && !_hold && delta >= BTN_RESET_INTERVAL) {
        VERBOSE(D_PRINT("Button: Reset"));
        _click_count = 0;
    } else if (_hold && !state) {
        D_PRINT("Button: Button hold release");

        if (_hold_release_handler != nullptr) {
            _hold_release_handler(_click_count);
        }

        _hold = false;
        _click_count = 0;
    }

    if (_hold) {
        D_PRINTF("Button: Hold #%i\n", _click_count);

        if (_hold_handler != nullptr) {
            _hold_handler(_click_count);
        }
    } else if (_click_count && delta > BTN_PRESS_WAIT_INTERVAL) {
        D_PRINTF("Button: Click count %i\n", _click_count);

        if (_click_handler != nullptr) {
            _click_handler(_click_count);
        }

        _click_count = 0;
    }
}
