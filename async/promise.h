#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>

#include <memory>
#include <vector>

#include "dispatcher.h"
#include "future.h"
#include "../debug.h"

class FutureBase;
class PromiseBase;
template<typename T> class Future;
template<typename T> class Promise;


class PromiseBase {
    volatile bool _finished = false;
    volatile bool _success = false;

    std::vector<FutureFinishedCb> _on_finished_callbacks;

#ifdef DEBUG
    int _initial_core_id = xPortGetCoreID();
#endif

public:
    PromiseBase(const PromiseBase &) = delete;
    PromiseBase &operator=(PromiseBase const &) = delete;
    virtual ~PromiseBase() = default;

    [[nodiscard]] virtual bool has_result() const { return false; }

    [[nodiscard]] bool finished() const { return _finished; }
    [[nodiscard]] bool success() const { return _success; }

    [[nodiscard]] bool wait(unsigned long timeout = 0, unsigned long delay_interval = 1) const;
    void on_finished(FutureFinishedCb callback);

    static Future<void> all(const std::vector<Future<void>> &collection);
    static Future<void> any(const std::vector<Future<void>> &collection);

    template<typename T>
    static Future<T> sequential(
        Future<T> first,
        std::function<bool(const Future<T> &prev)> has_next_fn,
        std::function<Future<T>(Future<T> prev)> fn);

protected:
    portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

    PromiseBase() = default;
    PromiseBase(PromiseBase &&) = default;

    void set_success();
    void set_error();

private:
    void _on_promise_finished();

    template<class T>
    static void _sequential_step(
        std::shared_ptr<Promise<T>> result_promise, Future<T> first,
        std::function<bool(const Future<T> &prev)> has_next_fn,
        std::function<Future<T>(Future<T> prev)> fn);
};

template<typename T>
class Promise final : public PromiseBase {
    T _result {};

public:
    Promise() = default;
    Promise(Promise &&) = default;

    [[nodiscard]] bool has_result() const override { return true; }

    [[nodiscard]] T result() const;

    void set_success(T value);

    using PromiseBase::set_error;

    static std::shared_ptr<Promise> create() { return std::make_shared<Promise>(); }
};

template<>
class Promise<void> final : public PromiseBase {
public:
    Promise() = default;
    Promise(Promise &&) = default;

    using PromiseBase::set_success;
    using PromiseBase::set_error;

    static std::shared_ptr<Promise> create() { return std::make_shared<Promise>(); }
};

template<typename T>
T Promise<T>::result() const {
    if (finished() && success()) return _result;

    D_PRINT("Trying to get value of unfinished or unsuccessful promise");
    Serial.flush();

    abort();
}

template<typename T>
void Promise<T>::set_success(T value) {
    portENTER_CRITICAL(&spinlock);

    bool already_finished = finished();
    if (!already_finished) {
        _result = std::move(value);
    }

    portEXIT_CRITICAL(&spinlock);

    if (!already_finished) {
        PromiseBase::set_success();
    }
}

template<typename T>
Future<T> PromiseBase::sequential(
    Future<T> first, std::function<bool(const Future<T> &prev)> has_next_fn,
    std::function<Future<T>(Future<T> prev)> fn) {

    auto result_promise = Promise<T>::create();

    VERBOSE(D_PRINTF("Promise::sequential(): Start sequence (%p)\r\n", result_promise.get()));
    _sequential_step<T>(result_promise, std::move(first), std::move(has_next_fn), std::move(fn));

    return result_promise;
}

template<typename T>
void PromiseBase::_sequential_step(
    std::shared_ptr<Promise<T>> result_promise, Future<T> first,
    std::function<bool(const Future<T> &prev)> has_next_fn,
    std::function<Future<T>(Future<T> prev)> fn) {

    first.on_finished([
            result_promise, prev=std::move(first),
            has_next_fn=std::move(has_next_fn), fn = std::move(fn)
        ](bool success) {
            VERBOSE(D_PRINTF("Promise::sequential(): Sequence (%p) step promise resolved\r\n", result_promise.get()));

            if (has_next_fn(prev)) {
                auto next = fn(prev);
                VERBOSE(D_PRINTF("Promise::sequential(): Sequence (%p) next step\r\n", result_promise.get()));

                _sequential_step(result_promise, std::move(next), std::move(has_next_fn), std::move(fn));
            } else {
                VERBOSE(D_PRINTF("Promise::sequential(): Finished sequence (%p) with result: %s\r\n",
                    result_promise.get(), prev.success() ? "success" : "failed"));

                if (success) {
                    if constexpr (std::is_void_v<T>) result_promise->set_success();
                    else result_promise->set_success(prev.result());
                } else {
                    result_promise->set_error();
                }
            }
        });
}


#endif
