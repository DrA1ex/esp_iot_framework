#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <functional>
#include <memory>

#include "system_timer.h"
#include "../debug.h"

class SystemTimer;

class FutureBase;
class PromiseBase;
template<typename T> class Future;
template<typename T> class Promise;

typedef std::function<void(bool success)> FutureFinishedCb;

class FutureBase {
protected:
    std::shared_ptr<const PromiseBase> promise;

public:
    explicit FutureBase(const std::shared_ptr<PromiseBase> &promise) : promise(promise) {}
    virtual ~FutureBase() = default;

    [[nodiscard]] virtual bool has_result() const;

    [[nodiscard]] bool finished() const;
    [[nodiscard]] bool success() const;

    [[nodiscard]] bool wait(unsigned long timeout = 0, unsigned long delay_interval = 1) const;
    void on_finished(FutureFinishedCb callback) const;

protected:
    template<typename T, typename R> static Future<R> then(const Future<T> &future, std::function<Future<R>(const Future<T> &)> fn);
    template<typename T, typename R> static Future<R> then(const Future<T> &future, std::function<R(const Future<T> &)> fn);

    template<typename T> static Future<T> on_error(const Future<T> &future, std::function<Future<T>(const Future<T> &)> fn);
    template<typename T> static Future<T> finally(const Future<T> &future, std::function<void(const Future<T> &)> fn);

    template<typename T> static Future<T> with_timeout(const Future<T> &future, unsigned long timeout);
};

template<typename T>
class Future final : public FutureBase {
public:
    Future(const std::shared_ptr<Promise<T>> &promise); // NOLINT(*-explicit-constructor)

    [[nodiscard]] T result() const;

    static Future successful(T value);
    static Future errored();

    template<typename R> Future<R> then(std::function<Future<R>(const Future &)> fn);
    template<typename R> Future<R> then(std::function<R(const Future &)> fn);

    Future on_error(std::function<Future(const Future &)> fn) const;
    Future on_error(std::function<Future()> fn) const;
    Future on_error(std::function<void()> fn) const;

    Future finally(std::function<void(const Future &)> fn) const;
    Future finally(std::function<void()> fn) const;

    Future with_timeout(unsigned long timeout) const;
};

template<>
class Future<void> final : public FutureBase {
public:
    Future(const std::shared_ptr<Promise<void>> &promise); // NOLINT(*-explicit-constructor)
    Future(const std::shared_ptr<PromiseBase> &promise); // NOLINT(*-explicit-constructor)
    Future(const FutureBase &future); // NOLINT(*-explicit-constructor)

    static Future successful();
    static Future errored();

    template<typename R> Future<R> then(std::function<Future<R>(const Future &)> fn);
    template<typename R> Future<R> then(std::function<R(const Future &)> fn);

    Future on_error(std::function<Future(const Future &)> fn) const;
    Future on_error(std::function<Future()> fn) const;
    Future on_error(std::function<void()> fn) const;

    Future finally(std::function<void()> fn) const;
    Future finally(std::function<void(const Future &)> fn) const;

    Future with_timeout(unsigned long timeout) const;
};

template<typename T, typename R> Future<R> FutureBase::then(const Future<T> &future, std::function<Future<R>(const Future<T> &)> fn) {
    VERBOSE(D_PRINTF("Promise (%p): Set continuation (promise)\n", future.promise.get()));

    auto chained_promise = Promise<R>::create();
    future.on_finished([self = future, fn = std::move(fn), chained_promise](bool success) {
        if (success) {
            auto ret_future = fn(self);
            ret_future.on_finished([ret_future, chained_promise](bool inner_success) {
                if (inner_success) {
                    if constexpr (std::is_void_v<R>) chained_promise->set_success();
                    else chained_promise->set_success(ret_future.result());
                } else {
                    chained_promise->set_error();
                }
            });
        } else {
            chained_promise->set_error();
        }
    });

    return chained_promise;
}

template<typename T, typename R> Future<R> FutureBase::then(const Future<T> &future, std::function<R(const Future<T> &)> fn) {
    VERBOSE(D_PRINTF("Promise (%p): Set continuation (non-promise)\n", future.promise.get()));

    auto chained_promise = Promise<R>::create();
    future.on_finished([self = future, fn = std::move(fn), chained_promise](bool success) {
        if (success) {
            if constexpr (std::is_void_v<R>) {
                fn(self);
                chained_promise->set_success();
            } else {
                chained_promise->set_success(fn(self));
            }
        } else {
            chained_promise->set_error();
        }
    });

    return chained_promise;
}

template<typename T> Future<T> FutureBase::on_error(const Future<T> &future, std::function<Future<T>(const Future<T> &)> fn) {
    VERBOSE(D_PRINTF("Promise (%p): Set error handler\n", future.promise.get()));

    auto chained_promise = Promise<T>::create();
    future.on_finished([self = future, fn = std::move(fn), chained_promise](bool success) {
        if (success) {
            if constexpr (std::is_void_v<T>) chained_promise->set_success();
            else chained_promise->set_success(self.result());
            return;
        }

        auto ret_future = fn(self);
        ret_future.on_finished([outer_future = self, ret_future, chained_promise](bool inner_success) {
            if (inner_success) {
                if constexpr (std::is_void_v<T>) chained_promise->set_success();
                else chained_promise->set_success(ret_future.result());
            } else {
                chained_promise->set_error();
            }
        });
    });

    return chained_promise;
}

template<typename T>
Future<T> FutureBase::finally(const Future<T> &future, std::function<void(const Future<T> &)> fn) {
    VERBOSE(D_PRINTF("Promise (%p): Set finally handler\n", future.promise.get()));

    auto chained_promise = Promise<T>::create();
    future.on_finished([self = future,fn = std::move(fn)](auto) {
        fn(self);
    });

    return future;
}

template<typename T>
Future<T> FutureBase::with_timeout(const Future<T> &future, unsigned long timeout) {
    if (timeout == 0) return future;

    auto result = Promise<T>::create();

    SystemTimer::set_timeout(timeout, [=] { if (!result->finished()) result->set_error(); });

    future.on_finished([=](auto success) {
        if (result->finished()) return;

        if (success) {
            if constexpr (std::is_void_v<T>) result->set_success();
            else result->set_success(future.result());
        } else {
            result->set_error();
        }
    });

    return result;
}

template<typename T>
Future<T>::Future(const std::shared_ptr<Promise<T>> &promise) : FutureBase(promise) {}

template<typename T>
T Future<T>::result() const { return ((const std::shared_ptr<Promise<T>> &) promise)->result(); }

template<typename T>
Future<T> Future<T>::errored() {
    auto promise = Promise<T>::create();
    promise->set_error();
    return promise;
}

template<typename T>
template<typename R>
Future<R> Future<T>::then(std::function<Future<R>(const Future &)> fn) { return FutureBase::then<T, R>(*this, std::move(fn)); }

template<typename T>
template<typename R>
Future<R> Future<T>::then(std::function<R(const Future &)> fn) { return FutureBase::then<T, R>(*this, std::move(fn)); }

template<typename T>
Future<T> Future<T>::on_error(
    std::function<Future(const Future &)> fn)
const {
    return FutureBase::on_error(*this, std::move(fn));
}

template<typename T>
Future<T> Future<T>::on_error(std::function<Future()> fn) const {
    return FutureBase::on_error(*this, [fn=std::move(fn)](auto) {
        return fn();
    });
}

template<typename T>
Future<T> Future<T>::on_error(std::function<void()> fn) const {
    return FutureBase::on_error(*this, [fn=std::move(fn)](auto f) {
        fn();
        return f;
    });
}

template<typename T>
Future<T> Future<T>::finally(std::function<void()> fn) const {
    return FutureBase::finally<T>(*this, [fn=std::move(fn)](auto) {
        fn();
    });
}

template<typename T>
Future<T> Future<T>::with_timeout(unsigned long timeout) const {
    return FutureBase::with_timeout<T>(*this, timeout);
}

template<typename T>
Future<T> Future<T>::finally(
    std::function<void(const Future &)> fn)
const {
    return FutureBase::finally(*this, std::move(fn));
}

template<typename T>
Future<T> Future<T>::successful(T value) {
    auto promise = Promise<T>::create();
    promise->set_success(value);
    return promise;
}

template<typename R>
Future<R> Future<void>::then(std::function<Future<R>(const Future &)> fn) { return FutureBase::then<void, R>(*this, std::move(fn)); }

template<typename R>
Future<R> Future<void>::then(std::function<R(const Future &)> fn) { return FutureBase::then<void, R>(*this, std::move(fn)); }


#endif
