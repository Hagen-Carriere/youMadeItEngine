#include "EventBus.h"

#include "EventBus.h"

EventBus& EventBus::Get() {
    static EventBus instance;
    return instance;
}

void EventBus::Publish(const std::string& event_type, const luabridge::LuaRef& event_obj) {
    auto& subs = subscribers[event_type];
    for (auto& [comp, func] : subs) {
        if (func.isFunction() && comp.isTable()) {
            try {
                func(comp, event_obj); // call with self and event data
            }
            catch (luabridge::LuaException& e) {
                std::cout << "\033[31mEventBus Error: " << e.what() << "\033[0m\n";
            }
        }
    }
}

void EventBus::Subscribe(const std::string& event_type, const luabridge::LuaRef& comp, const luabridge::LuaRef& func) {
    pendingOps.push_back({ DeferredOp::SUBSCRIBE, event_type, comp, func });
}

void EventBus::Unsubscribe(const std::string& event_type, const luabridge::LuaRef& comp, const luabridge::LuaRef& func) {
    pendingOps.push_back({ DeferredOp::UNSUBSCRIBE, event_type, comp, func });
}

void EventBus::ApplyDeferredSubscriptions() {
    for (auto& op : pendingOps) {
        auto& list = subscribers[op.event_type];
        if (op.type == DeferredOp::SUBSCRIBE) {
            list.emplace_back(op.component, op.function);
        }
        else if (op.type == DeferredOp::UNSUBSCRIBE) {
            list.erase(
                std::remove_if(list.begin(), list.end(), [&](const Subscription& s) {
                    return s.first == op.component && s.second == op.function;
                    }),
                list.end()
            );
        }
    }
    pendingOps.clear();
}
