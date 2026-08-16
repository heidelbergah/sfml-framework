/**
 * EventBus.hpp
 *
 * Global publish/subscribe channel, so subsystems can talk without
 * holding pointers to each other.
 *
 * Prefer subscribe_scoped(), which returns an RAII Subscription that
 * unsubscribes in its destructor. Listeners almost always capture
 * `this`, and a listener that outlives its owner is a dangling-pointer
 * crash waiting for the next publish.
 */

#ifndef EVENT_BUS_HPP
#define EVENT_BUS_HPP

#include "../Includes.hpp"

enum class EventType
{
    // Scene navigation
    SWITCH_TO_MAIN_MENU,
    SWITCH_TO_GAME,
    SWITCH_TO_SETTINGS,
    SWITCH_TO_GAME_OVER,
    SWITCH_TO_PERK_SELECT,

    // Flow control
    TOGGLE_PAUSE,
    PAUSE,
    RESUME,
    QUIT,

    // Generic hooks for game code
    PLAYER_DAMAGED,
    PLAYER_DIED,
    SCORE_CHANGED,
    CUSTOM
};

struct Event
{
    EventType   m_type;
    std::string m_data;
    float       m_value{0.0f};

    explicit Event(EventType type, std::string data = {}, float value = 0.0f)
        : m_type(type), m_data(std::move(data)), m_value(value)
    {}
};

using EventCallback  = std::function<void(const Event&)>;
using ListenerHandle = std::size_t;

inline constexpr ListenerHandle INVALID_LISTENER_HANDLE =
    static_cast<ListenerHandle>(-1);

class EventBus
{
private:
    EventBus()  = delete;
    ~EventBus() = delete;

    struct ListenerEntry
    {
        ListenerHandle m_handle;
        EventCallback  m_callback;

        ListenerEntry(ListenerHandle handle, EventCallback callback)
            : m_handle(handle), m_callback(std::move(callback))
        {}
    };

    inline static std::map<EventType, std::vector<ListenerEntry>> m_listeners;
    inline static ListenerHandle s_next_handle = 0;

public:
    /*
     * Raw subscription. You are responsible for calling unsubscribe()
     * before the captured object dies. Prefer subscribe_scoped().
     */
    static ListenerHandle subscribe(EventType type, EventCallback callback);

    static void unsubscribe(EventType type, ListenerHandle handle);

    /*
     * Invokes every listener for this event type. Safe to subscribe or
     * unsubscribe from inside a callback -- the listener list is
     * snapshotted first, and handles removed mid-dispatch are skipped
     * rather than called after the fact.
     */
    static void publish(const Event& event);

    static void clear_all_listeners();

    [[nodiscard]]
    static std::size_t listener_count(EventType type);
};

/*
 * Move-only handle that unsubscribes when it goes out of scope.
 *
 *     class MenuScene {
 *         Subscription m_on_start;
 *     public:
 *         MenuScene() {
 *             m_on_start = EventBus::subscribe_scoped(
 *                 EventType::SWITCH_TO_GAME,
 *                 [this](const Event&){ ... });
 *         }
 *     };  // unsubscribes automatically
 */
class Subscription
{
private:
    EventType      m_type{EventType::CUSTOM};
    ListenerHandle m_handle{INVALID_LISTENER_HANDLE};

public:
    Subscription() = default;

    Subscription(EventType type, ListenerHandle handle)
        : m_type(type), m_handle(handle)
    {}

    ~Subscription() { reset(); }

    Subscription(const Subscription&)            = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : m_type(other.m_type), m_handle(other.m_handle)
    {
        other.m_handle = INVALID_LISTENER_HANDLE;
    }

    Subscription& operator=(Subscription&& other) noexcept
    {
        if(this != &other)
        {
            reset();
            m_type         = other.m_type;
            m_handle       = other.m_handle;
            other.m_handle = INVALID_LISTENER_HANDLE;
        }
        return *this;
    }

    void reset()
    {
        if(m_handle != INVALID_LISTENER_HANDLE)
        {
            EventBus::unsubscribe(m_type, m_handle);
            m_handle = INVALID_LISTENER_HANDLE;
        }
    }

    [[nodiscard]]
    bool is_active() const { return m_handle != INVALID_LISTENER_HANDLE; }
};

/*
 * Preferred way to subscribe. Store the result as a member and the
 * listener dies with its owner.
 */
[[nodiscard]]
inline Subscription subscribe_scoped(EventType type, EventCallback callback)
{
    return Subscription{type, EventBus::subscribe(type, std::move(callback))};
}

#endif
