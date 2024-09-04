#pragma once

#include <cstdint>
#include <type_traits>

#include "../debug.h"

template<typename T, size_t Size, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
class CircularBuffer {
    T _buffer[Size] = {};

    size_t _used = 0;
    size_t _next_index = 0;

public:
    [[nodiscard]] inline T *buffer() { return _buffer; }
    [[nodiscard]] inline size_t size() const { return Size; }

    [[nodiscard]] inline bool can_acquire() const { return _used < Size; }
    [[nodiscard]] inline bool can_pop() const { return _used != 0; }

    T *acquire() {
        if (!can_acquire()) return nullptr;

        VERBOSE(D_PRINTF("Acquire value at %u; Used: %u / %u\n", _next_index, _used + 1, Size));

        auto value = &_buffer[_next_index];
        _used++;
        if (++_next_index >= Size) _next_index = 0;

        return value;
    }

    T *pop() {
        if (!can_pop()) return nullptr;

        const auto index = (Size + _next_index - _used) % Size;
        _used--;

        VERBOSE(D_PRINTF("Pop value at %u; Used: %u / %u\n", index, _used, Size));

        return &_buffer[index];
    }
};