/**
 * Pad.cpp  -  Mud Cats
 */

#include "Pad.hpp"
#include "Settings.hpp"
#include "Text.hpp"

#include <SFML/Window/Joystick.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace mudcats::pad
{

namespace
{
    using Axis = sf::Joystick::Axis;

    constexpr std::size_t N = static_cast<std::size_t>(Button::COUNT);

    // How one kind of pad numbers its buttons and where it puts its right
    // stick and triggers.
    struct Layout
    {
        std::array<unsigned, N> number;             // A B X Y LB RB BACK START LS RS
        enum class Triggers { SHARED, AXES, BUTTONS } triggers;
        Axis right_x, right_y;
        unsigned lt_button{}, rt_button{};          // Triggers::BUTTONS only
    };

#if defined(_WIN32)
    // Windows: A B X Y LB RB BACK START, then the sticks' clicks. No guide
    // button. Both triggers on one axis, Z: one trigger one way, the other the other.
    constexpr Layout XBOX{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, Layout::Triggers::SHARED, Axis::U, Axis::R};
#else
    // Linux's joystick driver, for an Xbox pad (or an 8BitDo in X-input mode,
    // usually over a cable): the guide button sits between START and the
    // sticks' clicks. LT is Z, RT is R, each from -100 (let go) to 100.
    constexpr Layout XBOX{{0, 1, 2, 3, 4, 5, 6, 7, 9, 10}, Layout::Triggers::AXES, Axis::U, Axis::V};
#endif
    // Linux, for a pad the kernel reads as a plain HID gamepad: an 8BitDo
    // (SN30 Pro and others) over Bluetooth, for one. Its buttons come as
    // BtnA BtnB BtnC BtnX BtnY BtnZ TL TR TL2 TR2 Select Start Mode ThumbL
    // ThumbR, numbered 0 to 14 in that order, with C and Z never used. The
    // right stick is Z and Rz; the triggers are buttons TL2 and TR2 (they're
    // also axes, Gas and Brake, but those are axes SFML doesn't read).
    // Buttons go by the letter printed on them, so A is A.
    constexpr Layout HID{{0, 1, 3, 4, 6, 7, 10, 11, 13, 14}, Layout::Triggers::BUTTONS, Axis::Z, Axis::R, 8, 9};

    // Which this pad is: one with no U axis (the Xbox right stick's X) but a
    // Z and an R, and buttons enough, is the plain HID kind.
    const Layout& layout(unsigned pad)
    {
#if !defined(_WIN32)
        if(!sf::Joystick::hasAxis(pad, Axis::U) && sf::Joystick::hasAxis(pad, Axis::Z)
           && sf::Joystick::hasAxis(pad, Axis::R) && sf::Joystick::getButtonCount(pad) >= 15)
            return HID;
#endif
        (void)pad;
        return XBOX;
    }

    constexpr float DEAD = 0.22f;                   // how much of a stick's middle does nothing

    bool using_pad = false;

    // The made-up pad, for testing (see Pad.hpp).
    struct Fake
    {
        bool on{false};
        std::string file;
        std::array<bool, static_cast<std::size_t>(Button::COUNT)> buttons{};
        float lx{}, ly{}, rx{}, ry{}, lt{}, rt{}, dx{}, dy{};
    };
    Fake& fake()
    {
        static Fake f = []
        {
            Fake g;
            if(const char* file = std::getenv("MUDCATS_FAKE_PAD")) { g.on = true; g.file = file; }
            return g;
        }();
        return f;
    }
    sf::Vector2f dead_zone(sf::Vector2f v);
    bool made_up = false;           // the next key press is one key_event made, for a pad button

    sf::Vector2f stick(bool right)
    {
        auto const i = id();
        if(!i) return {};
        Axis const ax = right ? layout(*i).right_x : Axis::X, ay = right ? layout(*i).right_y : Axis::Y;
        return dead_zone({sf::Joystick::getAxisPosition(*i, ax) / 100.0f, sf::Joystick::getAxisPosition(*i, ay) / 100.0f});
    }

    sf::Vector2f dead_zone(sf::Vector2f v)
    {
        float const len = std::hypot(v.x, v.y);
        if(len < DEAD) return {};
        // What's left past the dead middle, stretched back out to the whole range.
        float const scaled = std::min(1.0f, (len - DEAD) / (1.0f - DEAD));
        return v / len * scaled;
    }

    float trigger(bool right)
    {
        auto const i = id();
        if(!i) return 0.0f;
        const Layout& l = layout(*i);
        if(l.triggers == Layout::Triggers::BUTTONS)
            return sf::Joystick::isButtonPressed(*i, right ? l.rt_button : l.lt_button) ? 1.0f : 0.0f;
        if(l.triggers == Layout::Triggers::SHARED)
        {
            float const z = sf::Joystick::getAxisPosition(*i, Axis::Z) / 100.0f;
            return std::clamp(right ? -z : z, 0.0f, 1.0f);
        }
        Axis const a = right ? Axis::R : Axis::Z;
        if(!sf::Joystick::hasAxis(*i, a)) return 0.0f;
        // Let go is -100. Before a trigger has been touched at all the driver
        // may say 0, which would read as half pressed: so half counts as nothing.
        float const v = sf::Joystick::getAxisPosition(*i, a) / 100.0f;
        return std::clamp((v - 0.1f) / 0.9f, 0.0f, 1.0f);
    }

    std::optional<Button> button_from(unsigned joystick, unsigned number)
    {
        if(auto const i = id(); !i || *i != joystick) return std::nullopt;
        const auto& numbers = layout(joystick).number;
        for(std::size_t b = 0; b < numbers.size(); ++b)
            if(numbers[b] == number) return static_cast<Button>(b);
        return std::nullopt;
    }

    // The menus' arrows from the d-pad and the left stick: one press per push.
    std::array<int, 4> latch{};                     // X, Y, PovX, PovY: -1, 0, 1
}

std::optional<unsigned> id()
{
    if(fake().on) return 0u;
    for(unsigned i = 0; i < sf::Joystick::Count; ++i)
        if(sf::Joystick::isConnected(i) && sf::Joystick::getButtonCount(i) >= 4) return i;
    return std::nullopt;
}

bool connected() { return id().has_value(); }

bool required() { return Settings::get().require_pad; }

void refresh()
{
    Fake& f = fake();
    if(!f.on) return;
    std::ifstream in(f.file);
    std::string line;
    std::getline(in, line);
    static const char* const names[] = {"A", "B", "X", "Y", "LB", "RB", "BACK", "START", "LS", "RS"};
    f.buttons = {};
    f.lx = f.ly = f.rx = f.ry = f.lt = f.rt = f.dx = f.dy = 0.0f;
    std::istringstream words(line);
    std::string w;
    while(words >> w)
    {
        auto const eq = w.find('=');
        if(eq == std::string::npos)
        {
            for(std::size_t b = 0; b < f.buttons.size(); ++b) if(w == names[b]) f.buttons[b] = true;
            continue;
        }
        std::string const k = w.substr(0, eq);
        float const v = std::strtof(w.c_str() + eq + 1, nullptr);
        if(k == "LX") f.lx = v; else if(k == "LY") f.ly = v; else if(k == "RX") f.rx = v; else if(k == "RY") f.ry = v;
        else if(k == "LT") f.lt = v; else if(k == "RT") f.rt = v; else if(k == "DX") f.dx = v; else if(k == "DY") f.dy = v;
    }
}

bool held(Button b)
{
    if(fake().on) return fake().buttons[static_cast<std::size_t>(b)];
    auto const i = id();
    return i && sf::Joystick::isButtonPressed(*i, layout(*i).number[static_cast<std::size_t>(b)]);
}

sf::Vector2f left_stick() { return fake().on ? dead_zone({fake().lx, fake().ly}) : stick(false); }
sf::Vector2f right_stick() { return fake().on ? dead_zone({fake().rx, fake().ry}) : stick(true); }
float left_trigger() { return fake().on ? fake().lt : trigger(false); }
float right_trigger() { return fake().on ? fake().rt : trigger(true); }

sf::Vector2i dpad()
{
    if(fake().on) return {fake().dx > 0.5f ? 1 : (fake().dx < -0.5f ? -1 : 0), fake().dy > 0.5f ? 1 : (fake().dy < -0.5f ? -1 : 0)};
    auto const i = id();
    if(!i) return {};
    float const x = sf::Joystick::getAxisPosition(*i, Axis::PovX), y = sf::Joystick::getAxisPosition(*i, Axis::PovY);
    return {x > 50.0f ? 1 : (x < -50.0f ? -1 : 0), y > 50.0f ? 1 : (y < -50.0f ? -1 : 0)};
}

std::optional<Button> pressed(const sf::Event& e)
{
    const auto* b = e.getIf<sf::Event::JoystickButtonPressed>();
    return b ? button_from(b->joystickId, b->button) : std::nullopt;
}

std::optional<Button> released(const sf::Event& e)
{
    const auto* b = e.getIf<sf::Event::JoystickButtonReleased>();
    return b ? button_from(b->joystickId, b->button) : std::nullopt;
}

void notice(const sf::Event& e)
{
    if(made_up && e.is<sf::Event::KeyPressed>()) { made_up = false; return; }
    bool was = using_pad;
    if(e.is<sf::Event::KeyPressed>() || e.is<sf::Event::MouseButtonPressed>()) using_pad = false;
    else if(e.is<sf::Event::JoystickButtonPressed>()) using_pad = true;
    else if(const auto* m = e.getIf<sf::Event::JoystickMoved>(); m && std::abs(m->position) > 60.0f
            && (m->axis == Axis::X || m->axis == Axis::Y || m->axis == Axis::U || m->axis == Axis::V)) using_pad = true;
    // On a console there's nothing else to use; nor with the made-up pad.
    if(required() || fake().on) using_pad = true;
    if(was != using_pad || required()) set_pad_text(using_pad);
}

bool in_use() { return using_pad || required() || fake().on; }

std::optional<sf::Keyboard::Scancode> menu_key(const sf::Event& e)
{
    using S = sf::Keyboard::Scancode;
    if(auto const b = pressed(e))
    {
        switch(*b)
        {
            case Button::A: case Button::START: return S::Enter;
            case Button::B: return S::Escape;
            default: return std::nullopt;
        }
    }
    const auto* m = e.getIf<sf::Event::JoystickMoved>();
    if(!m || !id() || m->joystickId != *id()) return std::nullopt;
    int slot = -1;
    if(m->axis == Axis::X) slot = 0;
    else if(m->axis == Axis::Y) slot = 1;
    else if(m->axis == Axis::PovX) slot = 2;
    else if(m->axis == Axis::PovY) slot = 3;
    if(slot < 0) return std::nullopt;
    int& l = latch[static_cast<std::size_t>(slot)];
    if(std::abs(m->position) < 30.0f) { l = 0; return std::nullopt; }
    if(std::abs(m->position) < 55.0f || l != 0) return std::nullopt;
    l = m->position > 0.0f ? 1 : -1;
    bool const across = slot == 0 || slot == 2;
    if(across) return l > 0 ? S::Right : S::Left;
    return l > 0 ? S::Down : S::Up;
}

sf::Event key_event(sf::Keyboard::Scancode code)
{
    made_up = true;
    sf::Event::KeyPressed k{};
    k.scancode = code;
    k.code = sf::Keyboard::Key::Unknown;
    return sf::Event(k);
}

}
