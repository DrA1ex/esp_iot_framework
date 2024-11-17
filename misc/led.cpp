#include "led.h"

#include "../debug.h"

Led::Led(uint8_t pin) : _pin(pin) {}

void Led::begin() {
    pinMode(_pin, OUTPUT);

    _initialized = true;
}

void Led::_refresh_led(bool active) {
    if (!_initialized) return;

    if (_brightness < _max_brightness) {
        analogWrite(_pin, active ? _brightness : 0);
    } else {
        digitalWrite(_pin, active ? HIGH : LOW);
    }
}

void Led::flash(unsigned long duration) {
    if (!_initialized || (_active && _blink_count == 0)) return;

    _active = true;
    _start_time = millis();

    _flash_duration = duration;
    _blink_count = 0;
    _blink_count_left = 0;
    _continuously = false;

    _refresh_led(true);

    if (duration > 0) {
        VERBOSE(D_PRINTF("Led: setup flash mode for %i\n", duration));
    } else {
        VERBOSE(D_PRINT("Led: setup flash mode, endless"));
    }
}

void Led::blink(uint8_t count, bool continuously) {
    if (!_initialized) return;

    if (_active && count == 0) {
        turn_off();
    } else if (_active && _blink_count > 0) {
        _continuously = continuously;
        _blink_count = count;
        _blink_count_left = min(_blink_count_left, _blink_count);

        VERBOSE(D_PRINTF("Led: reconfigure blink mode, count: %i, %s\n", count, continuously ? "continuously" : "once"));
    } else {
        _active = true;
        _start_time = millis();

        _flash_duration = 0;
        _blink_count = count;
        _blink_count_left = count;
        _continuously = continuously;

        _refresh_led(true);

        VERBOSE(D_PRINTF("Led: setup blink mode, count: %i, %s\n", count, continuously ? "continuously" : "once"));
    }
}

void Led::turn_off() {
    if (!_initialized || !_active) return;

    _active = false;
    _refresh_led(false);

    VERBOSE(D_PRINT("Led: Turn off"));
}

void Led::tick() {
    if (!_initialized || !_active) return;

    auto time = millis();
    auto delta = time - _start_time;

    if (_blink_count_left > 0) {    // Blink Mode
        if (delta < _blink_active_duration) {
            _refresh_led(true);
        } else {
            _refresh_led(false);

            if (delta - _blink_active_duration > _blink_wait_duration) {
                _start_time = time;
                if (--_blink_count_left == 0 && !_continuously) turn_off();
            }
        }
    } else if (_blink_count > 0 && _continuously) {     // Blink Mode Cool down
        if (delta > _blink_repeat_interval) {
            _start_time = time;
            _blink_count_left = _blink_count;
            _refresh_led(true);
        }
    } else {    // Flash Mode
        if (_flash_duration > 0 && delta < _flash_duration) {
            turn_off();
        }
    }
}
