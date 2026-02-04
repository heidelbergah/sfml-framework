#ifndef EVENT_BUS_HPP
#define EVENT_BUS_HPP

#include "../Includes.hpp"

enum class EventType
{
    SWITCH_TO_GAME,
    SWITCH_TO_SETTINGS,
    SWITCH_TO_MAIN_MENU,
    TOGGLE_PAUSE
};

struct Event
{
    EventType m_type;
    std::string m_data;

    Event(EventType t, std::string d) :
        m_type(t), m_data(d)
    {}
};

using EventCallback = std::function<void(const Event&)>;
using ListenerHandle = size_t;

class EventBus
{
private:
    EventBus() = delete;
    ~EventBus() = delete;

    struct ListenerEntry
    {
        ListenerHandle m_handle;
        EventCallback m_callback;

        ListenerEntry(ListenerHandle h, EventCallback c) :
            m_handle(h), m_callback(c)
        {}
    };

    inline static std::map<EventType, std::vector<ListenerEntry>> m_listeners;
    inline static ListenerHandle nextHandle = 0;

public:
    // Important to note that a subscription cannot be explicitly
    // removed unless the ListenerHandle is recorded. Otherwise,
    // clear_all_listeners is your only option.
    static ListenerHandle subscribe(EventType type, EventCallback callback);
    static void unsubscribe(EventType type, ListenerHandle handle);
    static void publish(const Event& event);
    static void clear_all_listeners();

};

#endif
