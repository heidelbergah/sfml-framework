/**
 * EventBus.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/EventBus.hpp"

ListenerHandle EventBus::subscribe(EventType type, EventCallback callback)
{
    ListenerHandle const handle = s_next_handle++;
    m_listeners[type].emplace_back(handle, std::move(callback));
    return handle;
}

void EventBus::unsubscribe(EventType type, ListenerHandle handle)
{
    auto it = m_listeners.find(type);
    if(it == m_listeners.end()) return;

    auto& listeners = it->second;
    std::erase_if(listeners, [handle](const ListenerEntry& entry)
    {
        return entry.m_handle == handle;
    });
}

void EventBus::publish(const Event& event)
{
    auto it = m_listeners.find(event.m_type);
    if(it == m_listeners.end()) return;

    /*
     * Snapshot before dispatching, so a callback that subscribes or
     * unsubscribes cannot invalidate the iteration underneath us.
     *
     * Then re-check each handle against the live list before calling
     * it: a listener removed earlier in this same dispatch should not
     * still fire just because it was in the snapshot.
     */
    std::vector<ListenerEntry> const snapshot = it->second;

    for(const auto& entry : snapshot)
    {
        auto live = m_listeners.find(event.m_type);
        if(live == m_listeners.end()) break;   // cleared mid-dispatch

        bool const still_subscribed = std::any_of(
            live->second.begin(), live->second.end(),
            [&entry](const ListenerEntry& e) { return e.m_handle == entry.m_handle; });

        if(still_subscribed && entry.m_callback)
        {
            entry.m_callback(event);
        }
    }
}

void EventBus::clear_all_listeners()
{
    m_listeners.clear();
}

std::size_t EventBus::listener_count(EventType type)
{
    auto it = m_listeners.find(type);
    return (it == m_listeners.end()) ? 0u : it->second.size();
}
