/**
 * ShaderScene.cpp  -  Showcase
 */

#include "ShaderScene.hpp"

namespace showcase
{

namespace
{
    constexpr float BAR_H    = 18.0f;
    constexpr float DOCK_W   = 88.0f;
    constexpr float DOCK_H   = 24.0f;
    constexpr float DOCK_GAP = 8.0f;
    constexpr float DOCK_Y   = VIEW_H - 76.0f;

    // Where events go for windows that should not react: far off screen,
    // so hover states clear without anything being hit.
    const sf::Vector2f NOWHERE{-10000.0f, -10000.0f};
}

ShaderScene::ShaderScene()
    : DemoScene("Shader windows", "open a window from the dock, drag it by its title")
{
    m_default_hint = m_hint->get_string();

    // Homes are each window's top-left when it first opens.
    add_window("plasma", std::make_unique<PlasmaEffect>(), {32.0f,  72.0f});
    add_window("ripple", std::make_unique<RippleEffect>(), {716.0f, 84.0f});
    add_window("fire",   std::make_unique<FireEffect>(),   {256.0f, 96.0f});
    add_window("crt",    std::make_unique<CrtEffect>(),    {486.0f, 72.0f});
    add_window("scope",  std::make_unique<ScopeEffect>(),  {316.0f, 286.0f});

    float const total = static_cast<float>(m_order.size()) * (DOCK_W + DOCK_GAP) - DOCK_GAP;
    float       x     = (VIEW_W - total) / 2.0f;

    for(const std::string& key : m_order)
    {
        Window& window = m_windows.at(key);
        window.dock_button = make_button({DOCK_W, DOCK_H}, key);
        window.dock_button->set_position({x, DOCK_Y}, true);
        window.dock_button->set_callback([this, key]() { open(key); });
        m_widgets.add_widget("dock_" + key, window.dock_button);
        x += DOCK_W + DOCK_GAP;
    }

    refresh_dock();
}

void ShaderScene::add_window(const std::string& key, std::unique_ptr<HintedEffect> effect,
                             sf::Vector2f home)
{
    Window window;
    window.effect = std::move(effect);
    window.home   = home;
    m_windows.emplace(key, std::move(window));
    m_order.push_back(key);
}

ShaderScene::Window* ShaderScene::find(const std::shared_ptr<Widget>& frame)
{
    for(auto& [key, window] : m_windows)
        if(window.frame && window.frame == frame) return &window;
    return nullptr;
}

void ShaderScene::open(const std::string& key)
{
    Window& window = m_windows.at(key);

    // Already open: just bring it forward.
    if(window.frame && m_desktop.has_widget(key))
    {
        m_desktop.bring_to_front(key);
        return;
    }

    // Frames are built on first open, because loading shaders needs the
    // manager's ShaderManager, which a scene only has once it is added.
    if(!window.frame)
    {
        if(!m_manager) return;

        sf::Vector2f const content(window.effect->size());

        window.view = std::make_shared<ShaderView>(*window.effect, m_manager->get_shaders());
        window.view->set_position({1.0f, BAR_H}, true);

        float const controls = window.effect->controls_height();

        window.frame = std::make_shared<Frame>(
            sf::Vector2f{content.x + 2.0f, BAR_H + content.y + controls + 1.0f}, PANEL, *font());
        window.frame->set_outline(LINE, 1.0f);
        window.frame->add_taskbar(BAR_H, "  " + window.effect->title(), taskbar_style());
        window.frame->add_widget("view", window.view);

        window.effect->build_controls(*window.frame, {1.0f, BAR_H + content.y});

        window.frame->set_transition(TransitionFunction::EaseOutBack);
        window.frame->set_transition_duration(0.3f);
    }

    // A closed window is out of the desktop, so nothing has been clearing
    // its per-frame flags, including the close click that shut it.
    // Clear them now, or it would close again on its first update.
    window.frame->begin_frame();

    window.frame->set_visible(true);
    window.frame->set_position(window.home + sf::Vector2f{0.0f, 12.0f}, true);
    window.frame->set_position(window.home);

    m_desktop.add_widget(key, window.frame);
    m_desktop.bring_to_front(key);
    window.effect->on_open();
    refresh_dock();
}

void ShaderScene::close(const std::string& key)
{
    Window& window = m_windows.at(key);
    if(!window.frame || !m_desktop.has_widget(key)) return;

    window.effect->on_close();
    m_desktop.remove_widget(key);

    if(m_captured == window.frame) m_captured = nullptr;
    if(m_hovered == window.frame)  set_hovered(nullptr);

    // Remember where it was left, so it reopens there.
    window.home = window.frame->get_target_position();
    refresh_dock();
}

void ShaderScene::refresh_dock()
{
    for(auto& [key, window] : m_windows)
    {
        if(!window.dock_button) continue;
        bool const open = m_desktop.has_widget(key);
        recolor(*window.dock_button, open ? LINE : PANEL_HI);
        window.dock_button->set_text_color(open ? TEXT : TEXT_DIM);
    }
}

void ShaderScene::set_hovered(const std::shared_ptr<Frame>& frame)
{
    if(frame == m_hovered) return;
    m_hovered = frame;

    Window const* window = frame ? find(frame) : nullptr;
    m_hint->set_string(window ? window->effect->hint() + "   esc  back" : m_default_hint);
}

void ShaderScene::on_enter()
{
    DemoScene::on_enter();
    m_hint->set_string(m_default_hint);
    m_hovered  = nullptr;
    m_captured = nullptr;

    // Start with three windows open; the noisy one waits to be asked.
    for(const char* key : {"plasma", "fire", "crt"}) open(key);
}

void ShaderScene::on_exit()
{
    for(const std::string& key : m_order) close(key);
}

void ShaderScene::begin_frame()
{
    DemoScene::begin_frame();
    m_desktop.begin_frame();
}

void ShaderScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    bool const is_mouse = event.is<sf::Event::MouseMoved>() ||
                          event.is<sf::Event::MouseButtonPressed>() ||
                          event.is<sf::Event::MouseButtonReleased>() ||
                          event.is<sf::Event::MouseWheelScrolled>();

    if(!is_mouse)
    {
        DemoScene::handle_event(event, mouse_pos);   // escape, and anything else
        return;
    }

    // Releases go to everyone, so nothing is left thinking a button is
    // still held.
    if(event.is<sf::Event::MouseButtonReleased>())
    {
        m_widgets.handle_event(event, mouse_pos);
        for(const auto& widget : m_desktop.get_widgets())
            widget->handle_event(event, mouse_pos);
        m_captured = nullptr;
        set_hovered(std::dynamic_pointer_cast<Frame>(m_desktop.widget_at(mouse_pos)));
        return;
    }

    // Whoever has the mouse: the window holding it, else the front-most
    // window under the cursor, else the dock.
    std::shared_ptr<Frame> target = m_captured;
    if(!target) target = std::dynamic_pointer_cast<Frame>(m_desktop.widget_at(mouse_pos));

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>(); pressed && target)
    {
        m_desktop.bring_to_front(target);
        if(pressed->button == sf::Mouse::Button::Left) m_captured = target;
    }

    for(const auto& widget : m_desktop.get_widgets())
    {
        if(widget == target)
            widget->handle_event(event, mouse_pos);
        else if(event.is<sf::Event::MouseMoved>())
            widget->handle_event(event, NOWHERE);
    }

    if(target)
    {
        if(event.is<sf::Event::MouseMoved>()) m_widgets.handle_event(event, NOWHERE);
    }
    else
    {
        m_widgets.handle_event(event, mouse_pos);
    }

    set_hovered(target);
}

void ShaderScene::update(sf::Time delta_time)
{
    DemoScene::update(delta_time);
    m_desktop.update(delta_time);

    for(const std::string& key : m_order)
    {
        Window& window = m_windows.at(key);
        if(!window.frame || !m_desktop.has_widget(key)) continue;

        if(window.frame->close_requested())
        {
            close(key);
            continue;
        }

        // Keep at least part of the title bar on screen.
        if(window.frame->is_being_dragged())
        {
            sf::Vector2f const pos  = window.frame->get_position();
            sf::Vector2f const size = window.frame->get_size();
            sf::Vector2f const kept{std::clamp(pos.x, 48.0f - size.x, VIEW_W - 48.0f),
                                    std::clamp(pos.y, 0.0f, VIEW_H - BAR_H)};
            if(kept != pos) window.frame->set_position(kept, true);
        }
    }
}

void ShaderScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());
    target.draw(m_widgets);
    target.draw(m_desktop);
}

}
