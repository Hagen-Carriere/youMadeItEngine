#ifndef EVENTBUS_H
#define EVENTBUS_H

#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include <string>
#include <vector>
#include <unordered_map>

class EventBus {
public:
    using Subscription = std::pair<luabridge::LuaRef, luabridge::LuaRef>; // (component, function)

    static void PublishLua(const std::string& event_type, const luabridge::LuaRef& event_obj) {
        EventBus::Get().Publish(event_type, event_obj);
    }

    static void SubscribeLua(const std::string& event_type, const luabridge::LuaRef& component, const luabridge::LuaRef& func) {
        EventBus::Get().Subscribe(event_type, component, func);
    }

    static void UnsubscribeLua(const std::string& event_type, const luabridge::LuaRef& component, const luabridge::LuaRef& func) {
        EventBus::Get().Unsubscribe(event_type, component, func);
    }


    void ApplyDeferredSubscriptions();

    static EventBus& Get();

private:
    std::unordered_map<std::string, std::vector<Subscription>> subscribers;
    void Publish(const std::string& event_type, const luabridge::LuaRef& event_obj);
    void Subscribe(const std::string& event_type, const luabridge::LuaRef& component, const luabridge::LuaRef& func);
    void Unsubscribe(const std::string& event_type, const luabridge::LuaRef& component, const luabridge::LuaRef& func);


    struct DeferredOp {
        enum Type { SUBSCRIBE, UNSUBSCRIBE } type;
        std::string event_type;
        luabridge::LuaRef component;
        luabridge::LuaRef function;
    };
    std::vector<DeferredOp> pendingOps;
};

#endif  // EVENTBUS_H
