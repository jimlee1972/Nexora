#pragma once

#include "Nexora/Core/Handle.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nexora::core {
struct EventSubscriptionTag;
using SubscriptionHandle = Handle<EventSubscriptionTag>;

class EventBus final {
public:
  template <typename Event>
  SubscriptionHandle Subscribe(std::function<void(const Event &)> callback) {
    std::lock_guard lock{mutex_};
    const auto handle = handles_.Create();
    subscriptions_[std::type_index(typeid(Event))].push_back(
        {handle, [callback = std::move(callback)](const void *event) {
           callback(*static_cast<const Event *>(event));
         }});
    return handle;
  }

  bool Unsubscribe(SubscriptionHandle handle) {
    std::lock_guard lock{mutex_};
    if (!handles_.Destroy(handle))
      return false;
    for (auto &[type, subscriptions] : subscriptions_) {
      (void)type;
      std::erase_if(subscriptions,
                    [handle](const Subscription &entry) { return entry.handle == handle; });
    }
    return true;
  }

  template <typename Event> void Publish(const Event &event) const {
    std::vector<std::function<void(const void *)>> callbacks;
    {
      std::lock_guard lock{mutex_};
      const auto found = subscriptions_.find(std::type_index(typeid(Event)));
      if (found == subscriptions_.end())
        return;
      for (const auto &entry : found->second)
        callbacks.push_back(entry.callback);
    }
    for (const auto &callback : callbacks)
      callback(&event);
  }

  template <typename Event> void Enqueue(Event event) {
    std::lock_guard lock{mutex_};
    deferred_.push_back([this, event = std::move(event)] { Publish(event); });
  }

  void DispatchDeferred() {
    std::vector<std::function<void()>> events;
    {
      std::lock_guard lock{mutex_};
      events.swap(deferred_);
    }
    for (const auto &event : events)
      event();
  }

private:
  struct Subscription final {
    SubscriptionHandle handle;
    std::function<void(const void *)> callback;
  };
  mutable std::mutex mutex_;
  HandlePool<EventSubscriptionTag> handles_;
  std::unordered_map<std::type_index, std::vector<Subscription>> subscriptions_;
  std::vector<std::function<void()>> deferred_;
};
} // namespace nexora::core
