#include "../../include/framework/EventBus.hpp"

ListenerHandle EventBus::subscribe(EventType type, EventCallback callback)
{
    /*
     * Create a new handle, and add the event type and callback to 
     * the listeners map
     */

    ListenerHandle handle = nextHandle++;
    m_listeners[type].emplace_back(handle, callback);
    return handle;
}

void EventBus::unsubscribe(EventType type, ListenerHandle handle)
{
    /*
     * First, find all instances of an EventType in the listeners
     * map. Then, find where the handles match, and remove that
     * listener.
     */
    auto it = m_listeners.find(type);
    if (it != m_listeners.end())
    {
        auto& callbacks = it->second;
        callbacks.erase(
            std::remove_if(callbacks.begin(), callbacks.end(),
                [handle](const ListenerEntry& entry)
                {
                    return entry.m_handle == handle;
                }),
            callbacks.end()
        );
    }
}

void EventBus::publish(const Event& event)
{
    /*
     * Find every instance of a listener for an event type,
     * make a copy of that, and call those callback functions
     */
    auto it = m_listeners.find(event.m_type);
    if(it != m_listeners.end())
    {
        auto listeners_copy = it->second;

        for(auto& entry : listeners_copy)
        {
            entry.m_callback(event);
        }
    }
}

void EventBus::clear_all_listeners()
{
    m_listeners.clear();
}

