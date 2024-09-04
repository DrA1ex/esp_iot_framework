#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

template<typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
class Subscription {
public:
    typedef std::function<void(void *sender, T type, void *arg)> SubscriptionCallback;

    Subscription(void *subscriber, SubscriptionCallback callback) : _subscriber(subscriber), _callback(callback) {}

    inline void call(void *sender, T type, void *arg) const { _callback(sender, type, arg); }

    bool operator==(const Subscription &other) const {
        return _subscriber == other._subscriber;
    }

    struct HashFunction {
        size_t operator()(const Subscription &sub) const {
            return std::hash<void *>()(sub._subscriber);
        }
    };

private:
    void *_subscriber;
    SubscriptionCallback _callback;
};

template<typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
class EventTopic {
    typedef std::unordered_set<Subscription<T>, typename Subscription<T>::HashFunction> SubscriptionsSetT;

    std::unordered_map<T, SubscriptionsSetT> _subscribers{};
    SubscriptionsSetT _broadcast_subscribers{};

public:
    typedef typename Subscription<T>::SubscriptionCallback SubscriptionCallback;

    void subscribe(void *target, T type, SubscriptionCallback callback);
    void subscribe(void *target, SubscriptionCallback callback);

    void publish(void *sender, T type, void *arg = nullptr);
};

template<typename T, typename S1>
void EventTopic<T, S1>::subscribe(void *target, EventTopic::SubscriptionCallback callback) {
    _broadcast_subscribers.emplace(target, callback);
}

template<typename T, typename S1>
void EventTopic<T, S1>::subscribe(void *target, T type, EventTopic::SubscriptionCallback callback) {
    auto &type_set = _subscribers[type];
    type_set.emplace(Subscription(target, callback));
}

template<typename T, typename S1>
void EventTopic<T, S1>::publish(void *sender, T type, void *arg) {
    for (auto &sub: _broadcast_subscribers) {
        sub.call(sender, type, arg);
    }

    auto type_set = _subscribers.find(type);
    if (type_set == _subscribers.end()) return;

    for (auto &sub: type_set->second) {
        sub.call(sender, type, arg);
    }
}
