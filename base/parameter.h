#pragma once

#include <Arduino.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>

class AbstractParameter {
public:
    virtual ~AbstractParameter() = default;

    virtual bool set_value(const void *data, size_t size) = 0;
    [[nodiscard]] virtual const void *get_value() const = 0;
    [[nodiscard]] virtual size_t size() const = 0;

    virtual bool parse(const String &data) = 0;
    [[nodiscard]] virtual String to_string() const = 0;
};

template<typename T, typename = std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>>>
class Parameter : public AbstractParameter {
    T *_value;

public:
    using Type = T;

    Parameter(T *value) : _value(value) {} // NOLINT(*-explicit-constructor)

    bool set_value(const void *data, size_t size) override {
        if (size != sizeof(T)) return false;

        memcpy(_value, data, sizeof(T));
        return true;
    }

    [[nodiscard]] const void *get_value() const override { return _value; }

    [[nodiscard]] size_t size() const override { return sizeof(T); }

    bool parse(const String &data) override {
        if (data.length() == 0) return false;

        T value;
        if constexpr (std::is_same_v<T, float>) {
            value = data.toFloat();
        } else if constexpr (std::is_same_v<T, double>) {
            value = data.toDouble();
        } else {
            value = data.toInt();
        }

        set_value(&value, sizeof(T));

        return true;
    }

    [[nodiscard]] String to_string() const override {
        T value;
        memcpy(&value, _value, sizeof(T)); //To avoid unaligned memory access

        return String(value);
    }
};

template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
class ComplexParameter : public AbstractParameter {
    T *_value;

public:
    using Type = T;

    ComplexParameter(T *value) : _value(value) {} // NOLINT(*-explicit-constructor)

    bool set_value(const void *data, size_t size) override {
        if (size != sizeof(T)) return false;

        memcpy(_value, data, sizeof(T));
        return true;
    }

    [[nodiscard]] const void *get_value() const override { return _value; }

    [[nodiscard]] size_t size() const override { return sizeof(T); }

    bool parse(const String &data) override { return false; }

    [[nodiscard]] String to_string() const override { return "*Not supported*"; }
};

class FixedString : public AbstractParameter {
    char *_ptr;
    const size_t _size;

public:
    using Type = char *;

    FixedString(char *ptr, size_t size) : _ptr(ptr), _size(size) {}

    bool set_value(const void *data, size_t size) override {
        if (size > _size) return false;

        memcpy(_ptr, data, _size);
        if (size < _size) memset(_ptr + size, 0, _size - size);
        return true;
    }

    [[nodiscard]] const void *get_value() const override { return _ptr; }

    [[nodiscard]] size_t size() const override { return _size; }

    bool parse(const String &data) override {
        if (data.length() > _size) return false;

        return set_value(data.c_str(), data.length());
    }

    [[nodiscard]] String to_string() const override {
#if ARDUINO_ARCH_ESP32
        return {_ptr, strnlen(_ptr, _size)};
#else
        String str;
        str.concat(_ptr, strnlen(_ptr, _size));
        return str;
#endif
    }
};

template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
class GeneratedParameter : public AbstractParameter {
public:
    using Type = T;
    using GeneratorFn = std::function<T()>;

    GeneratedParameter(GeneratorFn generator) : _generator(std::move(generator)) {} // NOLINT(*-explicit-constructor)

    bool set_value(const void *data, size_t size) override { return false; }

    [[nodiscard]] const void *get_value() const override {
        _value = _generator();
        return &_value;
    }

    [[nodiscard]] size_t size() const override { return sizeof(T); }

    bool parse(const String &data) override { return false; }

    [[nodiscard]] String to_string() const override {
        if constexpr (std::is_constructible_v<T, String>) {
            return String(*(T *) get_value());
        }

        if constexpr (std::is_constructible_v<int, T>) {
            return String((int) *(T *) get_value());
        }

        return "*Not supported*";
    }

private:
    mutable T _value;
    GeneratorFn _generator;
};

typedef std::function<void()> Command;
