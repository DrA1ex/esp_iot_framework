#pragma once

#include <cstdint>
#include <new>

#include <lib/debug.h>

#ifdef CONFIG_CXX_EXCEPTIONS
#include <stdexcept>
#endif

#ifndef VECTOR_MIN_CAPACITY_GROWTH
#define VECTOR_MIN_CAPACITY_GROWTH      (8u)
#endif

#ifndef VECTOR_MAX_CAPACITY_GROWTH
#define VECTOR_MAX_CAPACITY_GROWTH      (64u)
#endif


template<typename T>
class Vector {
    uint32_t _capacity = 0;
    uint32_t _size = 0;
    uint8_t *_data = nullptr; // Use uint8_t* to avoid call constructor when allocating memory

public:
    T *data() { return (T *) _data; }
    const T *data() const { return (T *) _data; }

    [[nodiscard]] uint32_t capacity() const { return _capacity; }
    [[nodiscard]] uint32_t size() const { return _size; }

    Vector() = default;
    Vector(std::initializer_list<T> init);
    explicit Vector(uint32_t size, T value = {});

    ~Vector();

    bool reserve(uint32_t capacity);
    void resize(uint32_t size, T value = {});

    void clear();

    template<typename... Args> T &emplace(Args... args);
    void push(const T &value);
    void push(T &&value);
    void pop() noexcept;

    T *at(uint32_t index) noexcept;
    T &operator[](uint32_t index);
    T &operator[](uint32_t index) const;

    T *begin() noexcept { return (T *) _data; }
    T *end() noexcept { return (T *) _data + _size; }

    const T *begin() const noexcept { return (T *) _data; }
    const T *end() const noexcept { return (T *) _data + _size; }

private:
    void _grow_if_needed();
};


template<typename T>
Vector<T>::Vector(uint32_t size, T value) {
    resize(size, value);
}

template<typename T>
Vector<T>::Vector(std::initializer_list<T> init): Vector(init.size()) {
    uint32_t index = 0;
    for (auto &&item: init) {
        (*this)[index++] = std::move(item);
    }
}

template<typename T>
Vector<T>::~Vector() {
    if (_size == 0) return;

    if constexpr (!std::is_trivially_constructible_v<T>) {
        for (uint32_t i = 0; i < _size; i++) {
            ((T *) _data)[i].~T();
        }
    }

    delete[] _data;

    _size = 0;
    _capacity = 0;
    _data = nullptr;
}


template<typename T>
bool Vector<T>::reserve(uint32_t capacity) {
    if (capacity <= _capacity) return false;

    VERBOSE(D_PRINTF("Vector::reserve(): Reallocate from %i to %i\r\n", _capacity, capacity));

    uint8_t *old_data = _data;
    _capacity = capacity;
    _data = new uint8_t[sizeof(T) * capacity];

    for (int i = 0; i < _size; i++) {
        T &old_item = ((T *) old_data)[i];
        T *p_new_item = (T *) _data + i;

        if constexpr (std::is_move_constructible_v<T>) {
            new(p_new_item)T(std::move(old_item));
        } else if constexpr (std::is_copy_constructible_v<T>) {
            new(p_new_item)T(old_item);
        } else {
            D_PRINT("Vector::reserve(): T is not move or copy constructible");

            delete []_data;
            _data = old_data;

#ifdef CONFIG_CXX_EXCEPTIONS
            throw std::runtime_error("Vector::reserve(): T is not move or copy constructible");
#else
            abort();
#endif
        }

        old_item.~T();
    }

    delete[] old_data;
    return true;
}

template<typename T>
void Vector<T>::resize(uint32_t size, T value) {
    if (size == _size) return;

    reserve(size);
    while (size < _size) pop();
    while (size > _size) push(value);
}

template<typename T>
void Vector<T>::clear() {
    while (_size > 0) pop();
}

template<typename T> template<typename... Args>
T &Vector<T>::emplace(Args... args) {
    _grow_if_needed();

    VERBOSE(D_PRINTF("Vector::emplace() at %u\r\n", _size));

    auto memory = (T *) _data + _size++;
    new(memory) T(std::forward<Args>(args)...);

    return *memory;
}

template<typename T>
void Vector<T>::push(const T &value) {
    _grow_if_needed();

    VERBOSE(D_PRINTF("Vector::push(T&) at %u\r\n", _size));

    auto memory = (T *) _data + _size++;
    new(memory) T(value);
}

template<typename T>
void Vector<T>::push(T &&value) {
    _grow_if_needed();

    VERBOSE(D_PRINTF("Vector::push(T&&) at %u\r\n", _size));

    auto memory = (T *) _data + _size++;
    new(memory) T(std::move(value));
}

template<typename T>
void Vector<T>::pop() noexcept {
    if (_size == 0) return;

    --_size;
    if constexpr (!std::is_trivially_destructible_v<T>) {
        ((T *) _data)[_size].~T();
    }
}

template<typename T>
T *Vector<T>::at(uint32_t index) noexcept {
    if (index >= _size) return nullptr;
    return (T *) _data + index;
}

template<typename T>
T &Vector<T>::operator[](uint32_t index) {
    if (index >= _size) {
        D_PRINTF("Vector::operator[%u]: Out of range\r\n", index);
#ifdef CONFIG_CXX_EXCEPTIONS
        throw std::out_of_range("Vector::operator[]");
#else
        abort();
#endif
    }

    return ((T *) _data)[index];
}

template<typename T> T &Vector<T>::operator[](uint32_t index) const {
    return (*const_cast<Vector *>(this))[index];
}

template<typename T>
void Vector<T>::_grow_if_needed() {
    if (_size == _capacity) {
        auto growth = std::max<uint32_t>(VECTOR_MIN_CAPACITY_GROWTH,
            std::min<uint32_t>(VECTOR_MAX_CAPACITY_GROWTH, _capacity));

        auto new_size = _capacity + growth;
        reserve(new_size);
    }
}
