#pragma once

#include <functional>
#include <unordered_map>
#include <utility>

#include "../base/parameter.h"

typedef std::function<void(void *sender, const AbstractParameter *p)> ParameterChangedCallback;


class NotificationBus {
    std::vector<ParameterChangedCallback> _subscriptions;

    NotificationBus() = default;

public:
    static inline NotificationBus &get() {
        static NotificationBus bus;
        return bus;
    }

    void subscribe(ParameterChangedCallback callback) {
        _subscriptions.push_back(std::move(callback));
    }

    void notify_parameter_changed(void *sender, const AbstractParameter *parameter) {
        for (auto &cb: _subscriptions) {
            cb(sender, parameter);
        }
    }
};
